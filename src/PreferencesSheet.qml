import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Everything about how Tono looks and behaves. Colors aren't here: they
// always come from the Omarchy theme.
Popup {
    id: sheet

    property Item backdrop: null

    // Asks the window to show the search sheet, where connecting happens.
    signal connectSpotify()
    property int corner: 0
    readonly property color ink: theme.foreground
    readonly property color faint: Qt.rgba(ink.r, ink.g, ink.b, 0.62)

    function percent(value) {
        return qsTr("%1%").arg(Math.round(value * 100).toLocaleString(Qt.locale(), 'f', 0))
    }

    anchors.centerIn: Overlay.overlay
    width: Math.min(400, (parent ? parent.width : 400) - 32)
    modal: true
    dim: true
    padding: 22
    Material.roundedScale: sheet.corner

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
        spacing: 6

        RowLayout {
            Layout.fillWidth: true
            Layout.bottomMargin: 6
            Label {
                Layout.fillWidth: true
                text: qsTr("Preferences")
                font.pixelSize: 20
                font.weight: Font.Bold
                color: sheet.ink
            }
            GlassIconButton {
                size: 32
                icon.source: "qrc:/icons/close.svg"
                tip: qsTr("Close")
                onClicked: sheet.close()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Label { Layout.fillWidth: true; text: qsTr("Transparency"); color: sheet.ink; font.pixelSize: 14 }
            Label { text: sheet.percent(settings.transparency); color: sheet.faint; font.pixelSize: 13 }
        }
        Slider {
            Layout.fillWidth: true
            from: 0
            to: 1
            focusPolicy: Qt.NoFocus
            value: settings.transparency
            onMoved: settings.transparency = value
        }

        RowLayout {
            Layout.fillWidth: true
            Label { Layout.fillWidth: true; text: qsTr("Artwork glow"); color: sheet.ink; font.pixelSize: 14 }
            Label { text: sheet.percent(settings.artworkGlow); color: sheet.faint; font.pixelSize: 13 }
        }
        Slider {
            Layout.fillWidth: true
            from: 0
            to: 1
            focusPolicy: Qt.NoFocus
            value: settings.artworkGlow
            onMoved: settings.artworkGlow = value
        }

        Repeater {
            model: [
                { key: "softCorners", title: qsTr("Soft corners"), detail: qsTr("Rounder than the theme's windows, like iOS.") },
                { key: "motion", title: qsTr("Drifting artwork"), detail: qsTr("The cover slowly turns behind the glass while music plays.") },
                { key: "followPlaying", title: qsTr("Follow what's playing"), detail: qsTr("Switch to whichever player starts playing.") }
            ]

            delegate: RowLayout {
                required property var modelData
                Layout.fillWidth: true
                Layout.topMargin: 4
                spacing: 12

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1
                    Label { Layout.fillWidth: true; text: modelData.title; color: sheet.ink; font.pixelSize: 14; wrapMode: Text.WordWrap }
                    Label { Layout.fillWidth: true; text: modelData.detail; color: sheet.faint; font.pixelSize: 12; wrapMode: Text.WordWrap }
                }
                Switch {
                    focusPolicy: Qt.NoFocus
                    checked: settings[modelData.key]
                    onToggled: settings[modelData.key] = checked
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 8
            spacing: 12

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1
                Label { Layout.fillWidth: true; text: "Spotify"; color: sheet.ink; font.pixelSize: 14 }
                Label {
                    Layout.fillWidth: true
                    text: spotify.state === "signedIn"
                          ? (spotify.userName !== "" ? qsTr("Connected as %1").arg(spotify.userName) : qsTr("Connected"))
                          : qsTr("Not connected. Connecting lets you search from Tono.")
                    color: sheet.faint
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }
            }
            Button {
                flat: true
                focusPolicy: Qt.NoFocus
                text: spotify.state === "signedIn" ? qsTr("Disconnect") : qsTr("Connect")
                onClicked: spotify.state === "signedIn" ? spotify.signOut() : sheet.connectSpotify()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 10
            Label {
                Layout.fillWidth: true
                text: qsTr("Colors follow your Omarchy theme.")
                color: sheet.faint
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }
            Button {
                flat: true
                focusPolicy: Qt.NoFocus
                text: qsTr("Reset")
                onClicked: settings.reset()
            }
        }
    }
}
