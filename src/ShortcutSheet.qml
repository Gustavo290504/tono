import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Every keyboard shortcut, opened with ?.
Popup {
    id: sheet

    property Item backdrop: null
    property int corner: 0
    property int keyCorner: 0
    readonly property color ink: theme.foreground

    anchors.centerIn: Overlay.overlay
    width: Math.min(420, (parent ? parent.width : 420) - 32)
    modal: true
    dim: true
    padding: 22

    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, 0.25) }

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 180 }
        NumberAnimation { property: "scale"; from: 0.94; to: 1; duration: 220; easing.type: Easing.OutCubic }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 140 }
    }

    background: GlassPanel {
        radius: sheet.corner
        backdrop: sheet.backdrop
    }

    contentItem: ColumnLayout {
        spacing: 8

        Label {
            Layout.bottomMargin: 6
            text: qsTr("Keyboard shortcuts")
            font.pixelSize: 20
            font.weight: Font.Bold
            color: sheet.ink
        }

        Repeater {
            model: [
                { keys: ["Space"], action: qsTr("Play or pause") },
                { keys: ["N", "Ctrl →"], action: qsTr("Next track") },
                { keys: ["P", "Ctrl ←"], action: qsTr("Previous track") },
                { keys: ["←", "→"], action: qsTr("Back or forward 10 seconds") },
                { keys: ["↑", "↓"], action: qsTr("Volume up or down") },
                { keys: ["S"], action: qsTr("Shuffle") },
                { keys: ["R"], action: qsTr("Repeat") },
                { keys: ["Tab"], action: qsTr("Next player") },
                { keys: ["O"], action: qsTr("Show the player's window") },
                { keys: [","], action: qsTr("Preferences") },
                { keys: ["?"], action: qsTr("Shortcuts") },
                { keys: ["Q"], action: qsTr("Quit") }
            ]

            delegate: RowLayout {
                required property var modelData
                Layout.fillWidth: true
                spacing: 6

                Label {
                    Layout.fillWidth: true
                    text: modelData.action
                    color: sheet.ink
                    font.pixelSize: 14
                    elide: Text.ElideRight
                }
                Repeater {
                    model: modelData.keys
                    delegate: Rectangle {
                        required property string modelData
                        implicitWidth: Math.max(28, cap.implicitWidth + 14)
                        implicitHeight: 24
                        radius: sheet.keyCorner
                        color: Qt.rgba(1, 1, 1, theme.dark ? 0.1 : 0.5)
                        border.width: 1
                        border.color: Qt.rgba(1, 1, 1, theme.dark ? 0.16 : 0.7)
                        Label {
                            id: cap
                            anchors.centerIn: parent
                            text: modelData
                            color: sheet.ink
                            font.pixelSize: 12
                            font.weight: Font.Medium
                        }
                    }
                }
            }
        }
    }
}
