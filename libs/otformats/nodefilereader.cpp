#include "nodefilereader.h"

#include <QFile>
#include <algorithm>
#include <limits>
#include <utility>

namespace {

uint16_t readLe16(const QByteArray &data, qsizetype offset)
{
    const auto *raw = reinterpret_cast<const uchar *>(data.constData());
    return static_cast<uint16_t>(raw[offset])
         | static_cast<uint16_t>(raw[offset + 1] << 8);
}

uint32_t readLe32(const QByteArray &data, qsizetype offset)
{
    const auto *raw = reinterpret_cast<const uchar *>(data.constData());
    return static_cast<uint32_t>(raw[offset])
         | (static_cast<uint32_t>(raw[offset + 1]) << 8)
         | (static_cast<uint32_t>(raw[offset + 2]) << 16)
         | (static_cast<uint32_t>(raw[offset + 3]) << 24);
}

}

bool BinaryNode::getU8(uint8_t &value)
{
    if (!m_storage || m_readOffset + 1 > m_dataSize) {
        m_readOffset = m_dataSize;
        return false;
    }

    value = static_cast<uint8_t>(m_storage->at(m_dataStart + m_readOffset));
    ++m_readOffset;
    return true;
}

bool BinaryNode::getU16(uint16_t &value)
{
    if (!m_storage || m_readOffset + 2 > m_dataSize) {
        m_readOffset = m_dataSize;
        return false;
    }

    value = readLe16(*m_storage, m_dataStart + m_readOffset);
    m_readOffset += 2;
    return true;
}

bool BinaryNode::getU32(uint32_t &value)
{
    if (!m_storage || m_readOffset + 4 > m_dataSize) {
        m_readOffset = m_dataSize;
        return false;
    }

    value = readLe32(*m_storage, m_dataStart + m_readOffset);
    m_readOffset += 4;
    return true;
}

bool BinaryNode::getString(QString &value)
{
    uint16_t length = 0;
    if (!getU16(length)) {
        return false;
    }

    QByteArray bytes;
    if (!readBytes(length, bytes)) {
        return false;
    }

    value = QString::fromLatin1(bytes);
    return true;
}

bool BinaryNode::skip(qsizetype count)
{
    if (count < 0 || count > bytesRemaining()) {
        m_readOffset = m_dataSize;
        return false;
    }

    m_readOffset += count;
    return true;
}

bool BinaryNode::readBytes(qsizetype count, QByteArray &out)
{
    if (count < 0 || count > bytesRemaining()) {
        m_readOffset = m_dataSize;
        return false;
    }

    out = m_storage->mid(m_dataStart + m_readOffset, count);
    m_readOffset += static_cast<uint32_t>(count);
    return true;
}

qsizetype BinaryNode::bytesRemaining() const
{
    return m_readOffset < m_dataSize ? m_dataSize - m_readOffset : 0;
}

const QVector<BinaryNode> &BinaryNode::children() const
{
    static const QVector<BinaryNode> empty;
    return m_children ? *m_children : empty;
}

bool NodeFileReader::loadFile(const QString &path,
                              const QVector<QByteArray> &acceptedIdentifiers,
                              ProgressCallback progressCallback,
                              CancelCallback cancelCallback)
{
    m_root = BinaryNode();
    m_nodeData.clear();
    m_ok = false;
    m_errorString.clear();
    m_progressCallback = std::move(progressCallback);
    m_cancelCallback = std::move(cancelCallback);
    m_lastProgressPosition = 0;
    reportProgress(0, 1);

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(QStringLiteral("Cannot open file: %1").arg(path));
        return false;
    }

    const qint64 fileSize = file.size();
    if (fileSize < 5) {
        setError(QStringLiteral("The file is too small to be a valid node file"));
        return false;
    }
    if (static_cast<quint64>(fileSize) > std::numeric_limits<uint32_t>::max()) {
        setError(QStringLiteral("Node files larger than 4 GiB are not supported"));
        return false;
    }
    m_nodeData.reserve(static_cast<qsizetype>(fileSize));

    uchar *mapped = file.map(0, fileSize);
    QByteArray fallback;
    if (!mapped) {
        fallback = file.readAll();
        if (fallback.size() != fileSize) {
            setError(QStringLiteral("Could not read the complete node file"));
            return false;
        }
    }
    const QByteArrayView data = mapped
        ? QByteArrayView(reinterpret_cast<const char *>(mapped), static_cast<qsizetype>(fileSize))
        : QByteArrayView(fallback);

    const QByteArrayView identifier = data.first(4);
    const bool wildcardIdentifier = identifier[0] == '\0' && identifier[1] == '\0'
                                 && identifier[2] == '\0' && identifier[3] == '\0';
    bool accepted = wildcardIdentifier || acceptedIdentifiers.isEmpty();
    for (const QByteArray &candidate : acceptedIdentifiers) {
        if (candidate.size() == 4 && identifier == QByteArrayView(candidate)) {
            accepted = true;
            break;
        }
    }

    if (!accepted) {
        setError(QStringLiteral("Invalid file identifier"));
        return false;
    }

    qsizetype pos = 4;
    if (static_cast<uint8_t>(data.at(pos)) != Start) {
        setError(QStringLiteral("Invalid node file structure"));
        return false;
    }
    ++pos;

    if (!parseNode(data, pos, m_root, 0)) {
        return false;
    }

    m_ok = true;
    reportProgress(data.size(), data.size());
    if (mapped) file.unmap(mapped);
    return true;
}

bool NodeFileReader::parseNode(QByteArrayView data, qsizetype &pos, BinaryNode &node, int depth)
{
    if (depth > kMaxDepth) {
        setError(QStringLiteral("Node nesting is too deep (corrupt file?)"));
        return false;
    }

    node.m_storage = &m_nodeData;
    node.m_dataStart = static_cast<uint32_t>(m_nodeData.size());
    node.m_dataSize = 0;
    node.m_readOffset = 0;

    while (pos < data.size()) {
        if ((pos & 0xFFFF) == 0 && m_cancelCallback && m_cancelCallback()) {
            setError(QStringLiteral("Loading cancelled"));
            return false;
        }
        reportProgress(pos, data.size());
        uint8_t byte = static_cast<uint8_t>(data.at(pos));
        ++pos;

        if (byte == Escape) {
            if (pos >= data.size()) {
                setError(QStringLiteral("Truncated escape sequence in node file"));
                return false;
            }
            m_nodeData.append(data.at(pos));
            ++node.m_dataSize;
            ++pos;
            continue;
        }

        if (byte == Start) {
            --pos;
            return parseChildNodes(data, pos, node, depth);
        }

        if (byte == End) {
            return true;
        }

        m_nodeData.append(static_cast<char>(byte));
        ++node.m_dataSize;
    }

    setError(QStringLiteral("Unexpected end of node file"));
    return false;
}

void NodeFileReader::reportProgress(qsizetype position, qsizetype total)
{
    if (!m_progressCallback || total <= 0) {
        return;
    }
    constexpr qsizetype kReportInterval = 64 * 1024;
    if (position < total && position - m_lastProgressPosition < kReportInterval) {
        return;
    }
    m_lastProgressPosition = position;
    m_progressCallback(std::clamp(static_cast<double>(position) / static_cast<double>(total),
                                  0.0, 1.0));
}

bool NodeFileReader::parseChildNodes(QByteArrayView data, qsizetype &pos, BinaryNode &node, int depth)
{
    while (pos < data.size()) {
        if ((pos & 0xFFFF) == 0 && m_cancelCallback && m_cancelCallback()) {
            setError(QStringLiteral("Loading cancelled"));
            return false;
        }
        uint8_t marker = static_cast<uint8_t>(data.at(pos));
        ++pos;

        if (marker == Start) {
            BinaryNode child;
            if (!parseNode(data, pos, child, depth + 1)) {
                return false;
            }
            if (!node.m_children)
                node.m_children = std::make_shared<QVector<BinaryNode>>();
            node.m_children->append(std::move(child));
            continue;
        }

        if (marker == End) {
            return true;
        }

        setError(QStringLiteral("Invalid child marker in node file"));
        return false;
    }

    setError(QStringLiteral("Unexpected end of child list in node file"));
    return false;
}

void NodeFileReader::setError(const QString &message)
{
    m_errorString = message;
    m_ok = false;
}
