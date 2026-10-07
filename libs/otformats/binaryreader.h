#ifndef BINARYREADER_H
#define BINARYREADER_H

#include <QFile>
#include <QString>
#include <QByteArray>
#include <QDataStream>
#include <cstdint>
#include <vector>

class BinaryReader
{
public:
    BinaryReader();
    explicit BinaryReader(const QString &path);
    ~BinaryReader();

    BinaryReader(const BinaryReader &) = delete;
    BinaryReader &operator=(const BinaryReader &) = delete;

    bool open(const QString &path);
    void close();
    bool isOpen() const { return m_file.isOpen(); }

    uint8_t readU8();
    uint16_t readU16();
    uint32_t readU32();
    uint64_t readU64();

    int8_t readS8();
    int16_t readS16();
    int32_t readS32();

    QString readString();
    QString readString(size_t length);

    std::vector<uint8_t> readBytes(size_t count);

    size_t tell() const;
    bool seek(size_t position);
    bool skip(size_t bytes);

    size_t size() const { return m_fileSize; }
    size_t remaining() const;
    bool eof() const;

    bool good() const { return !m_error && m_file.isOpen() && m_stream.status() == QDataStream::Ok; }
    bool hasError() const { return m_error; }
    const QString &getError() const { return m_errorMessage; }
    void clearError();

private:
    template<typename T> T readNumber();
    QByteArray readBlock(size_t length);
    void setError(const QString &message);

    QFile m_file;
    QDataStream m_stream;
    size_t m_fileSize = 0;
    bool m_error = false;
    QString m_errorMessage;
};

#endif
