import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// A round glass button holding one icon. `filled` paints it in the theme
// accent for the main action; `active` marks a toggle that's on.
Button {
    id: control

    property real size: 48
    property real iconScale: 0.42
    property bool filled: false
    property bool active: false
    property string tip: ""
    readonly property color accent: theme.accent
    readonly property color ink: theme.foreground

    implicitWidth: size
    implicitHeight: size
    padding: 0
    topInset: 0
    bottomInset: 0
    leftInset: 0
    rightInset: 0
    focusPolicy: Qt.NoFocus
    hoverEnabled: true
    display: AbstractButton.IconOnly

    icon.width: Math.round(size * iconScale)
    icon.height: Math.round(size * iconScale)
    icon.color: filled ? theme.accentForeground : active ? accent : ink

    opacity: enabled ? 1 : 0.35
    scale: pressed ? 0.9 : hovered ? 1.05 : 1
    Behavior on scale { NumberAnimation { duration: 160; easing.type: Easing.OutBack } }

    contentItem: Item {
        TintedIcon {
            anchors.centerIn: parent
            width: control.icon.width
            height: control.icon.height
            source: control.icon.source
            color: control.icon.color
        }
    }

    ToolTip.visible: hovered && tip !== ""
    ToolTip.text: tip
    ToolTip.delay: 700

    // A true circle, so it keeps its shape whatever the theme's rounding.
    background: Rectangle {
        radius: width / 2
        color: control.filled ? Qt.rgba(control.accent.r, control.accent.g, control.accent.b, control.hovered ? 1 : 0.9)
             : control.active ? Qt.rgba(control.accent.r, control.accent.g, control.accent.b, 0.2)
             : Qt.rgba(1, 1, 1, (theme.dark ? 0.08 : 0.4) + (control.hovered ? 0.07 : 0))
        border.width: 1
        border.color: Qt.rgba(1, 1, 1, control.filled ? 0.3 : theme.dark ? 0.14 : 0.6)
        Behavior on color { ColorAnimation { duration: 150 } }

        Rectangle {
            anchors.fill: parent
            anchors.margins: 1
            radius: width / 2
            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.rgba(1, 1, 1, control.filled ? 0.22 : 0.1) }
                GradientStop { position: 0.5; color: Qt.rgba(1, 1, 1, 0) }
            }
        }
    }
}
