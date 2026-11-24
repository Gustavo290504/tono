import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// A thin bar that thickens under the pointer, like Apple Music's scrubber
// and volume. The handle is the bar itself.
Slider {
    id: control

    property real corner: 3
    readonly property color ink: theme.foreground
    readonly property bool engaged: hovered || pressed

    padding: 0
    implicitHeight: 24
    focusPolicy: Qt.NoFocus
    hoverEnabled: true

    background: Item {
        x: control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        width: control.availableWidth
        height: control.engaged ? 10 : 6
        Behavior on height { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }

        Rectangle {
            anchors.fill: parent
            radius: Math.min(control.corner, height / 2)
            color: Qt.rgba(control.ink.r, control.ink.g, control.ink.b, 0.18)
        }
        Rectangle {
            width: control.visualPosition * parent.width
            height: parent.height
            radius: Math.min(control.corner, height / 2)
            color: Qt.rgba(control.ink.r, control.ink.g, control.ink.b, control.engaged ? 0.95 : 0.72)
            Behavior on color { ColorAnimation { duration: 160 } }
        }
    }

    handle: Item {}
}
