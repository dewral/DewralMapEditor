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

    Q_PROPERTY(QString style READ style WRITE setStyle NOTIFY themeChanged)
    Q_PROPERTY(QVariantList styles READ styles CONSTANT)
    Q_PROPERTY(QVariantMap colorOverrides READ colorOverrides NOTIFY colorsChanged)
    Q_PROPERTY(QString highlightedColor READ highlightedColor WRITE setHighlightedColor NOTIFY highlightedColorChanged)

public:
    explicit UiTheme(QObject *parent = nullptr);

    QString style() const;
    void setStyle(const QString &s);
    QVariantList styles() const;
    QVariantMap colorOverrides() const { return m_colorOverrides; }
    Q_INVOKABLE bool setUiColor(const QString &key, const QColor &color);
    Q_INVOKABLE void resetUiColor(const QString &key);
    Q_INVOKABLE void resetUiColors();
    QString highlightedColor() const { return m_highlightedColor; }
    void setHighlightedColor(const QString &key);

signals:
    void themeChanged();
    void colorsChanged();
    void highlightedColorChanged();

private:
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
