#pragma once

#include <QByteArray>
#include <QByteArrayView>
#include <QString>
#include <QStringDecoder>

inline QString decodeMapText(QByteArrayView bytes)
{
    QStringDecoder decoder(QStringDecoder::Utf8);
    const QString text = decoder(bytes);
    return decoder.hasError() ? QString::fromLatin1(bytes.data(), bytes.size()) : text;
}

// Keep untouched legacy text byte-for-byte; edited/new strings use UTF-8.
inline QByteArray encodeMapText(const QString &text, const QByteArray &original = {})
{
    return decodeMapText(original) == text ? original : text.toUtf8();
}
