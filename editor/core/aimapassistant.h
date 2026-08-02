#ifndef AIMAPASSISTANT_H
#define AIMAPASSISTANT_H

#include <QObject>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class QNetworkAccessManager;
class QNetworkReply;

class AiMapAssistant : public QObject
{
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool configured READ configured NOTIFY configuredChanged)
    Q_PROPERTY(QString model READ model CONSTANT)

public:
    explicit AiMapAssistant(QObject *parent = nullptr);

    bool busy() const { return m_busy; }
    bool configured() const;
    QString model() const;

    Q_INVOKABLE void generate(const QString &prompt, const QVariantMap &selection);
    Q_INVOKABLE void cancel();

signals:
    void busyChanged();
    void configuredChanged();
    void planReady(const QVariantMap &plan);
    void failed(const QString &message);

private:
    void finishReply();
    void setBusy(bool busy);

    QNetworkAccessManager *m_network = nullptr;
    QNetworkReply *m_reply = nullptr;
    bool m_busy = false;
};

#endif
