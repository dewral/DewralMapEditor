#ifndef HOTKEYSETTINGS_H
#define HOTKEYSETTINGS_H

#include <QObject>
#include <QSettings>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class HotkeySettings : public QObject
{
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(QVariantList commands READ commands NOTIFY bindingsChanged)
    Q_PROPERTY(QVariantMap bindings READ bindings NOTIFY bindingsChanged)
    Q_PROPERTY(QVariantMap activeBindings READ activeBindings NOTIFY activeBindingsChanged)
    Q_PROPERTY(bool capturing READ capturing WRITE setCapturing NOTIFY capturingChanged)
    Q_PROPERTY(bool previewActive READ previewActive WRITE setPreviewActive NOTIFY previewActiveChanged)

public:
    explicit HotkeySettings(QObject *parent = nullptr);
    QVariantList commands() const;
    QVariantMap bindings() const;
    QVariantMap activeBindings() const;
    bool capturing() const { return m_capturing; }
    void setCapturing(bool capturing);
    bool previewActive() const { return m_previewActive; }
    void setPreviewActive(bool active);

    // Empty result means success. Empty shortcuts explicitly unbind a command.
    Q_INVOKABLE QString setShortcut(const QString &id, const QString &shortcut);
    Q_INVOKABLE QString resetShortcut(const QString &id);
    Q_INVOKABLE void resetAll();
    Q_INVOKABLE QString shortcutFromKey(int key, int modifiers) const;
    Q_INVOKABLE bool matches(const QString &id, int key, int modifiers) const;

signals:
    void bindingsChanged();
    void capturingChanged();
    void previewActiveChanged();
    void activeBindingsChanged();

private:
    QSettings m_settings;
    QVariantMap m_overrides;
    bool m_capturing = false;
    bool m_previewActive = false;
};

#endif
