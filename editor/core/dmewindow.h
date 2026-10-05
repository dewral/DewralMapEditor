#ifndef DMEWINDOW_H
#define DMEWINDOW_H

#include <QPointer>
#include <QQuickItem>
#include <QQuickWindow>

// Keep the QML frame while exposing native window capabilities to Windows.
class DmeWindow : public QQuickWindow
{
    Q_OBJECT
    Q_PROPERTY(QQuickItem *maximizeButton READ maximizeButton WRITE setMaximizeButton
               NOTIFY maximizeButtonChanged)
    Q_PROPERTY(bool maximizeHovered READ maximizeHovered NOTIFY maximizeInteractionChanged)
    Q_PROPERTY(bool maximizePressed READ maximizePressed NOTIFY maximizeInteractionChanged)

public:
    explicit DmeWindow(QWindow *parent = nullptr);

    QQuickItem *maximizeButton() const { return m_maximizeButton; }
    void setMaximizeButton(QQuickItem *button);
    bool maximizeHovered() const { return m_maximizeHovered; }
    bool maximizePressed() const { return m_maximizePressed; }

signals:
    void maximizeButtonChanged();
    void maximizeInteractionChanged();

protected:
    bool event(QEvent *event) override;
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;

private:
    void setMaximizeInteraction(bool hovered, bool pressed);
    bool containsMaximizeButton(const QPointF &clientPosition) const;

    QPointer<QQuickItem> m_maximizeButton;
    bool m_maximizeHovered = false;
    bool m_maximizePressed = false;
};

#endif
