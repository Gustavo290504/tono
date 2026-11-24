import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Shown when no music player is on the bus, or the bus itself can't be
// reached, saying which and what to do about it.
ColumnLayout {
    id: idle

    readonly property color ink: theme.foreground
    property int corner: 0

    spacing: 14

    GlassIconButton {
        Layout.alignment: Qt.AlignHCenter
        Layout.bottomMargin: 10
        size: 112
        iconScale: 0.4
        icon.source: "qrc:/icons/note.svg"
        enabled: players.busAvailable && players.spotifyInstalled
        tip: qsTr("Open Spotify")
        onClicked: players.launchSpotify()
    }

    Label {
        Layout.alignment: Qt.AlignHCenter
        Layout.maximumWidth: 320
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        text: !players.busAvailable ? qsTr("Can't reach your players")
              : players.startingSpotify ? qsTr("Opening Spotify…")
              : qsTr("Nothing is playing")
        font.pixelSize: 22
        font.weight: Font.Bold
        color: idle.ink
    }

    Label {
        Layout.alignment: Qt.AlignHCenter
        Layout.maximumWidth: 320
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        color: Qt.rgba(idle.ink.r, idle.ink.g, idle.ink.b, 0.65)
        font.pixelSize: 14
        text: !players.busAvailable
              ? qsTr("Tono talks to music players over the session D-Bus, and this session doesn't have one.")
              : players.startingSpotify
                ? qsTr("It starts in the background; press Space to play once it's ready.")
              : players.spotifyInstalled
                ? qsTr("Open Spotify, or any app that plays music, and Tono will follow it.")
                : qsTr("Spotify isn't installed. Any player that speaks MPRIS works too: a browser, mpv, or a terminal player.")
    }

    Button {
        Layout.alignment: Qt.AlignHCenter
        Layout.topMargin: 6
        visible: players.busAvailable && players.spotifyInstalled && !players.startingSpotify
        highlighted: true
        focusPolicy: Qt.NoFocus
        Material.roundedScale: idle.corner
        text: qsTr("Open Spotify")
        onClicked: players.launchSpotify()
    }
}
