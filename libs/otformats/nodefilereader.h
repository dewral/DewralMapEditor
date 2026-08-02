#ifndef NODEFILEREADER_H
#define NODEFILEREADER_H

#include <QByteArray>
#include <QByteArrayView>
#include <QString>
#include <QVector>
#include <cstdint>
#include <functional>
#include <memory>

class BinaryNode
{
public:
    bool getU8(uint8_t &value);
    bool getU16(uint16_t &value);
    bool getU32(uint32_t &value);
    bool getString(QString &value);
    bool skip(qsizetype count);
    bool readBytes(qsizetype count, QByteArray &out);

    qsizetype bytesRemaining() const;
    const QVector<BinaryNode> &children() const;

private:
    const QByteArray *m_storage = nullptr;
    std::shared_ptr<QVector<BinaryNode>> m_children;
    uint32_t m_dataStart = 0;
    uint32_t m_dataSize = 0;
    uint32_t m_readOffset = 0;

    friend class NodeFileReader;
};

class NodeFileReader
{
public:
    using ProgressCallback = std::function<void(double)>;
    using CancelCallback = std::function<bool()>;

    bool loadFile(const QString &path,
                  const QVector<QByteArray> &acceptedIdentifiers,
                  ProgressCallback progressCallback = {},
                  CancelCallback cancelCallback = {});

    bool isOk() const { return m_ok; }
    QString errorString() const { return m_errorString; }
    BinaryNode &rootNode() { return m_root; }
    const BinaryNode &rootNode() const { return m_root; }

private:
    enum Marker : uint8_t {
        Start = 0xFE,
        End = 0xFF,
        Escape = 0xFD
    };

    static constexpr int kMaxDepth = 512;
    bool parseNode(QByteArrayView data, qsizetype &pos, BinaryNode &node, int depth);
    bool parseChildNodes(QByteArrayView data, qsizetype &pos, BinaryNode &node, int depth);
    void reportProgress(qsizetype position, qsizetype total);
    void setError(const QString &message);

    BinaryNode m_root;
    QByteArray m_nodeData;
    bool m_ok = false;
    QString m_errorString;
    ProgressCallback m_progressCallback;
    CancelCallback m_cancelCallback;
    qsizetype m_lastProgressPosition = 0;
};

#endif
