#include "dmewindow.h"

#include <QPlatformSurfaceEvent>
#include <atomic>

#ifdef Q_OS_WIN
#include <QtCore/qt_windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#endif

struct DmeWindow::PresentedFrameState {
    std::atomic<quint64> requested{0};
    std::atomic<quint64> synchronized{0};
};

DmeWindow::DmeWindow(QWindow *parent) : QQuickWindow(parent),
    m_presentedFrameState(std::make_shared<PresentedFrameState>())
{
    setFlags(Qt::Window | Qt::FramelessWindowHint);
    // Synchronization blocks the GUI thread. Capturing the request here avoids
    // accepting a queued frameSwapped from before the feedback became visible.
    const auto state = m_presentedFrameState;
    connect(this, &QQuickWindow::beforeSynchronizing, this, [state] {
        state->synchronized.store(state->requested.exchange(0));
    }, Qt::DirectConnection);
    connect(this, &QQuickWindow::frameSwapped, this, [this, state] {
        const auto token = state->synchronized.exchange(0);
        if (token) QMetaObject::invokeMethod(this, [this, token] {
            emit presentedFrame(token);
        }, Qt::QueuedConnection);
    }, Qt::DirectConnection);

}

quint64 DmeWindow::requestPresentedFrame()
{
    if (!isVisible() || visibility() == QWindow::Minimized) return 0;
    const auto token = ++m_nextFrameToken;
    m_presentedFrameState->requested.store(token);
    update();
    return token;
}

void DmeWindow::setRoundedCorners(bool rounded)
{
    if (m_roundedCorners == rounded)
        return;
    m_roundedCorners = rounded;
    applyCornerPreference();
    emit roundedCornersChanged();
}

void DmeWindow::applyCornerPreference()
{
#ifdef Q_OS_WIN
    if (handle()) {
        // DWMWCP_DONOTROUND for the square Tibia skin; DWMWCP_ROUND otherwise.
        const int preference = m_roundedCorners ? 2 : 1;
        DwmSetWindowAttribute(reinterpret_cast<HWND>(winId()), 33,
                             &preference, sizeof(preference));
    }
#endif
}

void DmeWindow::setMaximizeButton(QQuickItem *button)
{
    if (m_maximizeButton == button)
        return;
    cancelMaximizeInteraction();
    m_maximizeButton = button;
    emit maximizeButtonChanged();
}

void DmeWindow::cancelMaximizeInteraction()
{
#ifdef Q_OS_WIN
    const bool wasPressed = m_maximizePressed;
#endif
    setMaximizeInteraction(false, false);
#ifdef Q_OS_WIN
    // Only release capture owned by our caption interaction. Qt owns client
    // drags and popups; cancelling a caption must not interrupt those grabs.
    if (wasPressed && handle() && GetCapture() == reinterpret_cast<HWND>(winId()))
        setMouseGrabEnabled(false);
#endif
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
    if (event->type() == QEvent::WindowDeactivate || event->type() == QEvent::WindowBlocked
        || event->type() == QEvent::Hide
        || (event->type() == QEvent::PlatformSurface
            && static_cast<QPlatformSurfaceEvent *>(event)->surfaceEventType()
                   == QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed)) {
        cancelMaximizeInteraction();
    }
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
        // The custom frameless caption must still expose native icons to the
        // taskbar and Alt+Tab. Shared resource handles live for the process.
        const HINSTANCE module = GetModuleHandleW(nullptr);
        const auto smallIcon = reinterpret_cast<HICON>(LoadImageW(
            module, L"IDI_DME_ICON", IMAGE_ICON,
            GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_SHARED));
        const auto largeIcon = reinterpret_cast<HICON>(LoadImageW(
            module, L"IDI_DME_ICON", IMAGE_ICON,
            GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_SHARED));
        if (smallIcon)
            SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(smallIcon));
        if (largeIcon)
            SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(largeIcon));
        // Match a normal dark Windows window (as used by OTEditor), while
        // retaining our QML caption and native Snap support. Unsupported
        // Windows versions simply ignore the Windows 11 corner attributes.
        const BOOL darkFrame = TRUE;
        DwmSetWindowAttribute(hwnd, 20, &darkFrame, sizeof(darkFrame));
        applyCornerPreference();
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
    case WM_GETMINMAXINFO: {
        // Windows maximizes a frameless HWND to the full monitor on first
        // show. Clipping only WM_NCCALCSIZE leaves an invisible window over
        // the taskbar, so constrain the native frame itself to the work area.
        if (visibility() != QWindow::FullScreen) {
            MONITORINFO monitor{sizeof(MONITORINFO)};
            if (GetMonitorInfoW(MonitorFromWindow(msg->hwnd, MONITOR_DEFAULTTONEAREST), &monitor)) {
                auto *limits = reinterpret_cast<MINMAXINFO *>(msg->lParam);
                limits->ptMaxPosition = {monitor.rcWork.left - monitor.rcMonitor.left,
                                         monitor.rcWork.top - monitor.rcMonitor.top};
                limits->ptMaxSize = {monitor.rcWork.right - monitor.rcWork.left,
                                     monitor.rcWork.bottom - monitor.rcWork.top};
            }
        }
        // Qt still needs to apply the QML minimum/maximum size constraints.
        break;
    }
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
    case WM_NCHITTEST: {
        // A maximized WS_THICKFRAME extends past the clipped client area.
        // Do not turn that invisible frame (or a point outside the window
        // during capture) into an editor hit over the taskbar or desktop.
        POINT point{GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam)};
        ScreenToClient(msg->hwnd, &point);
        RECT client;
        GetClientRect(msg->hwnd, &client);
        if (!PtInRect(&client, point)) {
            *result = HTNOWHERE;
            return true;
        }
        // HTMAXBUTTON is the shell's signal to offer Windows 11 Snap Layouts.
        // Other input still goes through the existing QML drag/resize areas.
        *result = buttonAt(msg->lParam) ? HTMAXBUTTON : HTCLIENT;
        return true;
    }
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
            setMouseGrabEnabled(true);
            setMaximizeInteraction(true, GetCapture() == msg->hwnd);
            *result = 0;
            return true;
        }
        break;
    case WM_MOUSEMOVE:
        // A release can be lost when native UI interrupts the click. Recover
        // before the next click outside the editor instead of retaining capture.
        if (m_maximizePressed && !(msg->wParam & MK_LBUTTON))
            cancelMaximizeInteraction();
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
                setMouseGrabEnabled(false);
            if (hovered)
                visibility() == QWindow::Maximized ? showNormal() : showMaximized();
            *result = 0;
            return true;
        }
        break;
    case WM_CAPTURECHANGED:
        setMaximizeInteraction(false, false);
        break;
    case WM_ACTIVATE:
        if (LOWORD(msg->wParam) == WA_INACTIVE)
            cancelMaximizeInteraction();
        break;
    case WM_ACTIVATEAPP:
        if (!msg->wParam)
            cancelMaximizeInteraction();
        break;
    case WM_KILLFOCUS:
        cancelMaximizeInteraction();
        break;
    case WM_CANCELMODE:
        cancelMaximizeInteraction();
        if (GetCapture() == msg->hwnd)
            setMouseGrabEnabled(false);
        break;
    default:
        break;
    }
#endif
    return QQuickWindow::nativeEvent(eventType, message, result);
}
