import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// A glass pill with one round avatar per music player on the bus, and the
// name of the one being shown. Clicking an avatar shows that player.
GlassPanel {
    id: strip

    readonly property color accent: theme.accent
    readonly property color ink: theme.foreground

    implicitHeight: 38
    implicitWidth: row.implicitWidth + 10

    RowLayout {
        id: row
        anchors.verticalCenter: parent.verticalCenter
        x: 5
        spacing: 4

        Repeater {
            model: players.players

            delegate: AbstractButton {
                id: avatar
                required property var modelData
                required property int index
                readonly property bool selected: index === players.currentIndex
                readonly property bool hasIcon: modelData.iconUrl.toString() !== ""

                Layout.preferredWidth: 28
                Layout.preferredHeight: 28
                focusPolicy: Qt.NoFocus
                hoverEnabled: true
                onClicked: players.select(index)

                ToolTip.visible: hovered
                ToolTip.text: modelData.identity
                ToolTip.delay: 500

                background: Rectangle {
                    radius: width / 2
                    color: Qt.rgba(1, 1, 1, avatar.hovered ? 0.18 : 0.1)
                    border.width: avatar.selected ? 2 : 0
                    border.color: strip.accent
                }

                // The player's own icon in its own colors, or its initial.
                contentItem: Item {
                    Image {
                        anchors.centerIn: parent
                        width: 18
                        height: 18
                        visible: avatar.hasIcon
                        source: avatar.modelData.iconUrl
                        sourceSize: Qt.size(36, 36)
                        smooth: true
                        mipmap: true
                    }
                    Label {
                        anchors.centerIn: parent
                        visible: !avatar.hasIcon
                        text: avatar.modelData.identity.charAt(0).toUpperCase()
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        color: strip.ink
                    }
                }

                // A dot for a player that's playing but not shown.
                Rectangle {
                    visible: avatar.modelData.playing && !avatar.selected
                    width: 7
                    height: 7
                    radius: 3.5
                    color: strip.accent
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                }
            }
        }

        Label {
            text: players.current ? players.current.identity : ""
            visible: text !== ""
            leftPadding: 4
            rightPadding: 8
            font.pixelSize: 13
            font.weight: Font.Medium
            color: strip.ink
            elide: Text.ElideRight
            Layout.maximumWidth: 160
        }
    }
}
