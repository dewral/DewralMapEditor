#ifndef DOCUMENTMANAGER_H
#define DOCUMENTMANAGER_H

#include "otbmreader.h"

#include <QObject>
#include <QVariantList>
#include <QVector>
#include <QHash>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

class DocumentManager : public QObject
{
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(int count READ count NOTIFY tabsChanged)
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentChanged)
    Q_PROPERTY(OtbmReader *current READ current NOTIFY currentChanged)

    Q_PROPERTY(QVariantList tabs READ tabs NOTIFY tabsChanged)
    Q_PROPERTY(QVariantList recoveries READ recoveries NOTIFY recoveriesChanged)
    Q_PROPERTY(int recoveryCount READ recoveryCount NOTIFY recoveriesChanged)

public:
    explicit DocumentManager(QObject *parent = nullptr);

    int count() const { return static_cast<int>(m_docs.size()); }
    int currentIndex() const { return m_current; }
    void setCurrentIndex(int i);
    OtbmReader *current() const;
    QVariantList tabs() const;
    QVariantList recoveries() const { return m_recoveries; }
    int recoveryCount() const { return static_cast<int>(m_recoveries.size()); }

    Q_INVOKABLE OtbmReader *newDocument();

    Q_INVOKABLE bool closeDocument(int i);

    Q_INVOKABLE int indexOfPath(const QString &path) const;
    Q_INVOKABLE void configureAutosave(bool enabled, int intervalMinutes);
    Q_INVOKABLE bool autosaveNow();
    Q_INVOKABLE void discardRecoveries();
    Q_INVOKABLE bool adoptCurrentRecovery(const QString &recoveryId,
                                          const QString &originalPath);
    Q_INVOKABLE void setCurrentProfileKey(const QString &profileKey);
    void markCleanShutdown();

signals:
    void currentChanged();
    void tabsChanged();
    void recoveriesChanged();
    void autosaveCompleted(const QString &title);
    void autosaveFailed(const QString &title, const QString &error);

private:
    void hookDocument(OtbmReader *doc);
    void loadPreviousSession();
    void writeSession(bool cleanExit = false);
    void autosaveDueDocument();
    QString recoveryDirectory() const;
    QString recoveryPath(const QString &id) const;
    void removeRecoveryFiles(const QString &id);
    QString documentId(OtbmReader *doc);

    QVector<OtbmReader *> m_docs;
    QHash<OtbmReader *, QString> m_documentIds;
    QHash<OtbmReader *, qint64> m_lastEditMs;
    QHash<OtbmReader *, qint64> m_lastRecoveryMs;
    QHash<OtbmReader *, QString> m_profileKeys;
    QVariantList m_recoveries;
    QTimer m_autosaveTimer;
    bool m_autosaveEnabled = true;
    int m_autosaveIntervalMinutes = 3;
    bool m_shuttingDown = false;
    bool m_initializing = true;
    int m_current = 0;
};

#endif
