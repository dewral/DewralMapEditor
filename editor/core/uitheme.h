#ifndef UITHEME_H
#define UITHEME_H

#include <QColor>
#include <QImage>
#include <QMutex>
#include <QObject>
#include <QQuickImageProvider>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class UiTheme : public QObject
{
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(QColor tint READ tint WRITE setTint NOTIFY themeChanged)

    Q_PROPERTY(QString tex READ tex NOTIFY themeChanged)
    Q_PROPERTY(QVariantList presets READ presets CONSTANT)

    Q_PROPERTY(QString style READ style WRITE setStyle NOTIFY themeChanged)
    Q_PROPERTY(QVariantList styles READ styles CONSTANT)
    Q_PROPERTY(QVariantMap colorOverrides READ colorOverrides NOTIFY colorsChanged)
    Q_PROPERTY(QString highlightedColor READ highlightedColor WRITE setHighlightedColor NOTIFY highlightedColorChanged)

public:
    explicit UiTheme(QObject *parent = nullptr);

    QColor tint() const;
    void setTint(const QColor &c);

    QString tex() const;
    QVariantList presets() const;
    QString style() const;
    void setStyle(const QString &s);
    QVariantList styles() const;
    QVariantMap colorOverrides() const { return m_colorOverrides; }
    Q_INVOKABLE bool setUiColor(const QString &key, const QColor &color);
    Q_INVOKABLE void resetUiColor(const QString &key);
    Q_INVOKABLE void resetUiColors();
    QString highlightedColor() const { return m_highlightedColor; }
    void setHighlightedColor(const QString &key);

    QImage texture(const QString &file) const;

signals:
    void themeChanged();
    void colorsChanged();
    void highlightedColorChanged();

private:

    QImage flatTexture(const QString &file) const;
    QImage windowsClassicTexture(const QString &file) const;

    mutable QMutex m_mutex;
    QColor m_tint;
    QString m_style;
    int m_version = 0;
    QVariantMap m_colorOverrides;
    QString m_highlightedColor;
};

class UiThemeImageProvider : public QQuickImageProvider
{
public:
    explicit UiThemeImageProvider(UiTheme *theme);
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

private:
    UiTheme *m_theme;
};

#endif
