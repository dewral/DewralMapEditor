import QtQuick
import QtTest
import "../../editor/qml/components" as Components
Item {
    QtObject { id: state; property int value: 7 }
    Components.LazyToolHost {
        id: host
        toolSource: Qt.resolvedUrl("fixtures/LazyToolFixture.qml")
        initialProperties: ({environment: state})
    }
    TestCase {
        name: "LazyToolHost"
        when: windowShown
        function cleanup() { host.active = false; state.value = 7; }
        function test_notCreatedUntilRequested() {
            compare(host.item, null);
            verify(host.ensureLoaded());
            compare(host.item.observed, 7);
            const first = host.item;
            verify(host.ensureLoaded()); compare(host.item, first);
            host.item.open(); verify(host.item.opened);
        }
        function test_bindingAndReopen() {
            verify(host.ensureLoaded());
            state.value = 11; compare(host.item.observed, 11);
            host.active = false; compare(host.item, null);
            verify(host.ensureLoaded()); compare(host.item.observed, 11);
        }
    }
}
