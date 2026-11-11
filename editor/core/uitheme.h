#ifndef UITHEME_H
#define UITHEME_H

#include <QColor>
#include <QImage>
#include <QMutex>
#include <QObject>
#include <QQuickImageProvider>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class UiTheme : public QObject
{
    Q_OBJECT
    QML_ANONYMOUS

    Q_PROPERTY(int uiScalePercent READ uiScalePercent WRITE setUiScalePercent NOTIFY uiScaleChanged)
    Q_PROPERTY(int activeUiScalePercent READ activeUiScalePercent CONSTANT)
    Q_PROPERTY(bool uiScaleRestartRequired READ uiScaleRestartRequired NOTIFY uiScaleChanged)
    Q_PROPERTY(QString style READ style WRITE setStyle NOTIFY themeChanged)
    Q_PROPERTY(QVariantList styles READ styles CONSTANT)
    Q_PROPERTY(QVariantMap colorOverrides READ colorOverrides NOTIFY colorsChanged)
    Q_PROPERTY(QString highlightedColor READ highlightedColor WRITE setHighlightedColor NOTIFY highlightedColorChanged)

public:
    explicit UiTheme(QObject *parent = nullptr);

    int uiScalePercent() const { return m_uiScalePercent; }
    int activeUiScalePercent() const { return m_activeUiScalePercent; }
    bool uiScaleRestartRequired() const { return m_uiScalePercent != m_activeUiScalePercent; }
    void setUiScalePercent(int percent);
    QString style() const;
    void setStyle(const QString &s);
    QVariantList styles() const;
    QVariantMap colorOverrides() const { return m_colorOverrides; }
    Q_INVOKABLE bool setUiColor(const QString &key, const QColor &color);
    Q_INVOKABLE void resetUiColor(const QString &key);
    Q_INVOKABLE void resetUiColors();
    Q_INVOKABLE QStringList classicItemTextLines(const QString &text) const;
    QString highlightedColor() const { return m_highlightedColor; }
    void setHighlightedColor(const QString &key);

signals:
    void uiScaleChanged();
    void themeChanged();
    void colorsChanged();
    void highlightedColorChanged();

private:
    int m_uiScalePercent = 100;
    int m_activeUiScalePercent = 100;
    mutable QMutex m_mutex;
    QString m_style;
    QVariantMap m_colorOverrides;
    QString m_highlightedColor;
};

class UiThemeImageProvider : public QQuickImageProvider
{
public:
    UiThemeImageProvider();
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
};

#endif
