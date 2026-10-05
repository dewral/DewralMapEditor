#include "dmewindow.h"

#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QtCore/qt_windows.h>
#include <windowsx.h>

#include <cstdlib>
#include <iostream>
#include <memory>

namespace {
void expect(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

LPARAM buttonPosition(HWND hwnd, const DmeWindow &window, const QQuickItem &button)
{
    const QPointF position = button.mapToScene(QPointF(button.width() / 2, button.height() / 2))
        * window.devicePixelRatio();
    POINT point{qRound(position.x()), qRound(position.y())};
    ClientToScreen(hwnd, &point);
    return MAKELPARAM(point.x, point.y);
}
}

int main(int argc, char **argv)
{
    qputenv("QT_QUICK_BACKEND", "software");
    QGuiApplication app(argc, argv);
    qmlRegisterType<DmeWindow>("DmeWindowTests", 1, 0, "DmeWindow");
    qmlRegisterType<QQuickItem>("DmeWindowTests", 1, 0, "TestItem");
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(DME_QT_QML_DIR));
    QQmlComponent component(&engine);
    component.setData(R"(
        import QtQuick
        import DmeWindowTests 1.0
        import "components"
        DmeWindow {
            id: editorWindow
            x: 100; y: 100; width: 800; height: 600
            minimumWidth: 720; minimumHeight: 480
            maximizeButton: maximizeControl
            TestItem {
                id: maximizeControl
                x: 700; y: 1; width: 46; height: 40
            }
            WindowResizeHandles { objectName: "resizeHandles"; targetWindow: editorWindow }
        }
    )", QUrl::fromLocalFile(QStringLiteral(DME_QML_SOURCE_DIR "/NativeWindowTest.qml")));
    std::unique_ptr<QObject> root(component.create());
    if (!root)
        std::cerr << component.errorString().toStdString();
    expect(root && qobject_cast<DmeWindow *>(root.get()),
           "QML must create the custom window and its caption controls");
    DmeWindow &window = *qobject_cast<DmeWindow *>(root.get());
    QQuickItem *resizeHandles = window.findChild<QQuickItem *>(QStringLiteral("resizeHandles"));
    expect(resizeHandles && resizeHandles->property("targetWindow").value<QObject *>() == &window,
           "resize handles must bind the native DME window");
    expect(resizeHandles->isEnabled(), "resize handles must be active on a normal DME window");
    expect(window.maximizeButton(), "QML must bind the native maximize hit region");
    QQuickItem &button = *window.maximizeButton();
    window.show();
    app.processEvents();

    auto hwnd = reinterpret_cast<HWND>(window.winId());
    const LONG_PTR snapStyles = WS_THICKFRAME | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SYSMENU;
    expect((GetWindowLongPtrW(hwnd, GWL_STYLE) & snapStyles) == snapStyles,
           "the native window must advertise snapping, resizing and caption actions");
    RECT frame, client;
    GetWindowRect(hwnd, &frame);
    GetClientRect(hwnd, &client);
    expect(frame.right - frame.left == client.right && frame.bottom - frame.top == client.bottom,
           "native snap styles must not add a second frame around the QML content");

    LPARAM position = buttonPosition(hwnd, window, button);
    expect(SendMessageW(hwnd, WM_NCHITTEST, 0, position) == HTMAXBUTTON,
           "the QML maximize button must be recognized by Windows Snap Layouts");
    button.setVisible(false);
    expect(SendMessageW(hwnd, WM_NCHITTEST, 0, position) == HTCLIENT,
           "hidden maximize controls must not expose a native hit region");
    button.setVisible(true);
    button.setEnabled(false);
    expect(SendMessageW(hwnd, WM_NCHITTEST, 0, position) == HTCLIENT,
           "disabled maximize controls must not expose a native hit region");
    button.setEnabled(true);

    POINT outside{qRound(100 * window.devicePixelRatio()), qRound(100 * window.devicePixelRatio())};
    ClientToScreen(hwnd, &outside);
    expect(SendMessageW(hwnd, WM_NCHITTEST, 0, MAKELPARAM(outside.x, outside.y)) == HTCLIENT,
           "other QML controls must continue receiving client input");
    SendMessageW(hwnd, WM_NCMOUSEMOVE, HTMAXBUTTON, position);
    expect(window.maximizeHovered(), "native maximize hover must update the QML highlight");
    SendMessageW(hwnd, WM_NCMOUSELEAVE, 0, 0);
    expect(!window.maximizeHovered(), "native maximize hover must clear when the mouse leaves");

    SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTMAXBUTTON, position);
    expect(window.maximizePressed(), "native maximize press must update the QML highlight");
    SendMessageW(hwnd, WM_LBUTTONUP, 0, MAKELPARAM(100, 100));
    expect(window.visibility() == QWindow::Windowed && !window.maximizePressed(),
           "releasing outside maximize must cancel the click");
    SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTMAXBUTTON, position);
    SendMessageW(hwnd, WM_NCLBUTTONUP, HTMAXBUTTON, position);
    app.processEvents();
    expect(window.visibility() == QWindow::Maximized && !window.maximizePressed(),
           "a native maximize click must maximize exactly once");
    MONITORINFO monitor{sizeof(MONITORINFO)};
    GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &monitor);
    GetClientRect(hwnd, &client);
    POINT origin{0, 0};
    ClientToScreen(hwnd, &origin);
    expect(origin.x == monitor.rcWork.left && origin.y == monitor.rcWork.top
               && client.right == monitor.rcWork.right - monitor.rcWork.left
               && client.bottom == monitor.rcWork.bottom - monitor.rcWork.top,
           "maximized content must fit the work area without hiding controls or covering the taskbar");
    position = buttonPosition(hwnd, window, button);
    SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTMAXBUTTON, position);
    SendMessageW(hwnd, WM_NCLBUTTONUP, HTMAXBUTTON, position);
    app.processEvents();
    expect(window.visibility() == QWindow::Windowed,
           "the native restore button must restore the normal window");

    SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTMAXBUTTON, position);
    SendMessageW(hwnd, WM_CANCELMODE, 0, 0);
    expect(!window.maximizePressed() && GetCapture() != hwnd,
           "cancelled native input must clear pressed state and release capture");
    window.hide();
    window.destroy();
    window.create();
    hwnd = reinterpret_cast<HWND>(window.winId());
    expect((GetWindowLongPtrW(hwnd, GWL_STYLE) & snapStyles) == snapStyles,
           "snap support must survive recreation of the native window");

    std::cout << "Native window snap integration passed\n";
    return EXIT_SUCCESS;
}
