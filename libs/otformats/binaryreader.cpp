#include "binaryreader.h"
#include <limits>

// QDataStream handles little-endian decoding; this adapter bounds every read.
BinaryReader::BinaryReader() : m_stream(&m_file)
{
    m_stream.setByteOrder(QDataStream::LittleEndian);
}
BinaryReader::BinaryReader(const QString &path) : BinaryReader() { open(path); }
BinaryReader::~BinaryReader() = default;

bool BinaryReader::open(const QString &path)
{
    close();
    m_file.setFileName(path);
    if (!m_file.open(QIODevice::ReadOnly)) {
        setError(m_file.errorString());
        return false;
    }
    m_fileSize = size_t(m_file.size());
    return true;
}
void BinaryReader::close()
{
    m_file.close();
    m_fileSize = 0;
    clearError();
}
template<typename T> T BinaryReader::readNumber()
{
    if (!good() || sizeof(T) > remaining()) {
        setError(QStringLiteral("Truncated integer at byte %1").arg(tell()));
        return T{};
    }
    T result{};
    m_stream >> result;
    if (m_stream.status() != QDataStream::Ok) {
        setError(QStringLiteral("Binary read failed at byte %1").arg(tell()));
        return T{};
    }
    return result;
}
uint8_t BinaryReader::readU8() { return readNumber<quint8>(); }
uint16_t BinaryReader::readU16() { return readNumber<quint16>(); }
uint32_t BinaryReader::readU32() { return readNumber<quint32>(); }
uint64_t BinaryReader::readU64() { return readNumber<quint64>(); }
int8_t BinaryReader::readS8() { return readNumber<qint8>(); }
int16_t BinaryReader::readS16() { return readNumber<qint16>(); }
int32_t BinaryReader::readS32() { return readNumber<qint32>(); }

QByteArray BinaryReader::readBlock(size_t length)
{
    if (!good() || length > remaining()
        || length > size_t(std::numeric_limits<qsizetype>::max())) {
        setError(QStringLiteral("Invalid binary block of %1 bytes at %2").arg(length).arg(tell()));
        return {};
    }
    const auto data = m_file.read(qint64(length));
    if (size_t(data.size()) != length) {
        setError(QStringLiteral("Incomplete binary block at byte %1").arg(tell()));
        return {};
    }
    return data;
}
QString BinaryReader::readString()
{
    const auto length = readU16();
    return good() ? readString(length) : QString{};
}
QString BinaryReader::readString(size_t length)
{
    return QString::fromLatin1(readBlock(length));
}
std::vector<uint8_t> BinaryReader::readBytes(size_t count)
{
    const auto bytes = readBlock(count);
    if (bytes.isEmpty()) return {};
    const auto *start = reinterpret_cast<const uint8_t *>(bytes.constData());
    return {start, start + bytes.size()};
}
size_t BinaryReader::tell() const { return isOpen() ? size_t(m_file.pos()) : 0; }
size_t BinaryReader::remaining() const
{
    const auto offset = tell();
    return offset <= size() ? size() - offset : 0;
}
bool BinaryReader::seek(size_t position)
{
    if (!good() || position > size() || !m_file.seek(qint64(position))) {
        setError(QStringLiteral("Invalid binary position %1").arg(position));
        return false;
    }
    return true;
}
bool BinaryReader::skip(size_t bytes)
{
    if (bytes > remaining()) {
        setError(QStringLiteral("Cannot skip %1 bytes past end of stream").arg(bytes));
        return false;
    }
    return seek(tell() + bytes);
}
bool BinaryReader::eof() const { return !isOpen() || remaining() == 0; }
void BinaryReader::setError(const QString &message)
{
    if (!m_error) m_errorMessage = message;
    m_error = true;
}
void BinaryReader::clearError()
{
    m_error = false;
    m_errorMessage.clear();
    m_stream.resetStatus();
}
