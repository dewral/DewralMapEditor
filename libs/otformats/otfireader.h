#ifndef OTFIREADER_H
#define OTFIREADER_H

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

class OtfiReader : public QObject
{
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(bool found READ found NOTIFY foundChanged)
    Q_PROPERTY(bool extended READ extended NOTIFY foundChanged)
    Q_PROPERTY(bool transparency READ transparency NOTIFY foundChanged)
    Q_PROPERTY(bool frameDurations READ frameDurations NOTIFY foundChanged)
    Q_PROPERTY(bool frameGroups READ frameGroups NOTIFY foundChanged)
    Q_PROPERTY(QString metadataFile READ metadataFile NOTIFY foundChanged)
    Q_PROPERTY(QString spritesFile READ spritesFile NOTIFY foundChanged)

public:
    explicit OtfiReader(QObject *parent = nullptr);

    Q_INVOKABLE bool loadFromFolder(const QString &folder);

    bool found() const { return m_found; }
    bool extended() const { return m_extended; }
    bool transparency() const { return m_transparency; }
    bool frameDurations() const { return m_frameDurations; }
    bool frameGroups() const { return m_frameGroups; }
    QString metadataFile() const { return m_metadataFile; }
    QString spritesFile() const { return m_spritesFile; }

signals:
    void foundChanged();

private:
    friend class ClientLoadService;
    bool m_found = false;
    bool m_extended = false;
    bool m_transparency = false;
    bool m_frameDurations = false;
    bool m_frameGroups = false;
    QString m_metadataFile;
    QString m_spritesFile;
};

#endif
