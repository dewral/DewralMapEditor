#include "dmewindow.h"

#include <QPlatformSurfaceEvent>

#ifdef Q_OS_WIN
#include <QtCore/qt_windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#endif

DmeWindow::DmeWindow(QWindow *parent) : QQuickWindow(parent)
{
    setFlags(Qt::Window | Qt::FramelessWindowHint);
}

void DmeWindow::setMaximizeButton(QQuickItem *button)
{
    if (m_maximizeButton == button)
        return;
    m_maximizeButton = button;
    setMaximizeInteraction(false, false);
    emit maximizeButtonChanged();
}

void DmeWindow::setMaximizeInteraction(bool hovered, bool pressed)
{
    if (m_maximizeHovered == hovered && m_maximizePressed == pressed)
        return;
    m_maximizeHovered = hovered;
    m_maximizePressed = pressed;
    emit maximizeInteractionChanged();
}

bool DmeWindow::containsMaximizeButton(const QPointF &clientPosition) const
{
    return m_maximizeButton && m_maximizeButton->window() == this
        && m_maximizeButton->isVisible() && m_maximizeButton->isEnabled()
        && m_maximizeButton->contains(m_maximizeButton->mapFromScene(clientPosition));
}

bool DmeWindow::event(QEvent *event)
{
    const bool handled = QQuickWindow::event(event);
#ifdef Q_OS_WIN
    if (event->type() == QEvent::PlatformSurface
        && static_cast<QPlatformSurfaceEvent *>(event)->surfaceEventType()
               == QPlatformSurfaceEvent::SurfaceCreated) {
        // Qt's frameless style omits WS_THICKFRAME, which disables Aero Snap
        // and the Windows 11 drag-to-top snap bar. Reapply for each new HWND.
        const HWND hwnd = reinterpret_cast<HWND>(winId());
        const LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
        SetWindowLongPtrW(hwnd, GWL_STYLE,
                         style | WS_THICKFRAME | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SYSMENU);
        SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        // Match a normal dark Windows window (as used by OTEditor), while
        // retaining our QML caption and native Snap support. Unsupported
        // Windows versions simply ignore the Windows 11 corner attributes.
        const BOOL darkFrame = TRUE;
        DwmSetWindowAttribute(hwnd, 20, &darkFrame, sizeof(darkFrame));
        const int roundCorners = 2; // DWMWCP_ROUND
        DwmSetWindowAttribute(hwnd, 33, &roundCorners, sizeof(roundCorners));
        const COLORREF systemBorder = 0xffffffff; // DWMWA_COLOR_DEFAULT
        DwmSetWindowAttribute(hwnd, 34, &systemBorder, sizeof(systemBorder));
        const MARGINS frameMargins{1, 1, 1, 1};
        DwmExtendFrameIntoClientArea(hwnd, &frameMargins);
    }
#endif
    return handled;
}

bool DmeWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
#ifdef Q_OS_WIN
    const auto *msg = static_cast<MSG *>(message);
    const auto buttonAt = [this, msg](LPARAM position) {
        // Native coordinates are physical pixels, including negative screen
        // coordinates on monitors to the left/above the primary display.
        POINT point{GET_X_LPARAM(position), GET_Y_LPARAM(position)};
        ScreenToClient(msg->hwnd, &point);
        return containsMaximizeButton(QPointF(point.x, point.y) / devicePixelRatio());
    };

    switch (msg->message) {
    case WM_NCCALCSIZE: {
        // Retain the native resize/maximize styles without drawing a second
        // frame. A maximized thick frame extends outside the monitor's work
        // area, so clip the client to keep the QML controls and taskbar visible.
        auto *client = msg->wParam
            ? &reinterpret_cast<NCCALCSIZE_PARAMS *>(msg->lParam)->rgrc[0]
            : reinterpret_cast<RECT *>(msg->lParam);
        if (IsZoomed(msg->hwnd) && visibility() != QWindow::FullScreen) {
            MONITORINFO monitor{sizeof(MONITORINFO)};
            if (GetMonitorInfoW(MonitorFromWindow(msg->hwnd, MONITOR_DEFAULTTONEAREST), &monitor)) {
                RECT clipped;
                if (IntersectRect(&clipped, client, &monitor.rcWork))
                    *client = clipped;
            }
        }
        *result = 0;
        return true;
    }
    case WM_NCHITTEST:
        // HTMAXBUTTON is the shell's signal to offer Windows 11 Snap Layouts.
        // Other input still goes through the existing QML drag/resize areas.
        *result = buttonAt(msg->lParam) ? HTMAXBUTTON : HTCLIENT;
        return true;
    case WM_NCMOUSEMOVE:
        setMaximizeInteraction(buttonAt(msg->lParam), m_maximizePressed);
        if (m_maximizeHovered) {
            TRACKMOUSEEVENT tracking{sizeof(TRACKMOUSEEVENT), TME_LEAVE | TME_NONCLIENT,
                                     msg->hwnd, 0};
            TrackMouseEvent(&tracking);
        }
        break; // Let the shell process the hover and display its snap menu.
    case WM_NCMOUSELEAVE:
        setMaximizeInteraction(false, m_maximizePressed);
        break;
    case WM_NCLBUTTONDOWN:
    case WM_NCLBUTTONDBLCLK:
        if (msg->wParam == HTMAXBUTTON) {
            setMaximizeInteraction(true, true);
            SetCapture(msg->hwnd);
            *result = 0;
            return true;
        }
        break;
    case WM_MOUSEMOVE:
        setMaximizeInteraction(containsMaximizeButton(
                                   QPointF(GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam))
                                   / devicePixelRatio()), m_maximizePressed);
        break;
    case WM_LBUTTONUP:
    case WM_NCLBUTTONUP:
        if (m_maximizePressed) {
            const bool hovered = msg->message == WM_NCLBUTTONUP
                ? buttonAt(msg->lParam)
                : containsMaximizeButton(QPointF(GET_X_LPARAM(msg->lParam),
                                                 GET_Y_LPARAM(msg->lParam)) / devicePixelRatio());
            setMaximizeInteraction(hovered, false);
            if (GetCapture() == msg->hwnd)
                ReleaseCapture();
            if (hovered)
                visibility() == QWindow::Maximized ? showNormal() : showMaximized();
            *result = 0;
            return true;
        }
        break;
    case WM_CAPTURECHANGED:
        setMaximizeInteraction(false, false);
        break;
    case WM_CANCELMODE:
        setMaximizeInteraction(false, false);
        if (GetCapture() == msg->hwnd)
            ReleaseCapture();
        break;
    default:
        break;
    }
#endif
    return QQuickWindow::nativeEvent(eventType, message, result);
}
