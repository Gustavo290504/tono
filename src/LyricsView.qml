import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Effects
import QtQuick.Layouts

// The words of the song, following the music line by line when LRCLIB has
// them synced, like Apple Music: the line being sung bright, the rest dim,
// and a click on a line jumps the song there.
Item {
    id: view

    property QtObject player: null
    readonly property color ink: theme.foreground
    readonly property bool synced: lyrics.synced
    readonly property int current: synced && player ? lyrics.indexAt(player.position) : -1
    // Scrolling by hand stops the view following the song for a moment.
    property bool following: true

    // Waits a beat, so newly shown or loaded lyrics are laid out first.
    function follow() {
        Qt.callLater(glideToCurrent)
    }

    function glideToCurrent() {
        if (!following || current < 0 || !list.visible)
            return
        list.forceLayout()
        const line = list.itemAtIndex(current)
        if (!line) {
            list.positionViewAtIndex(current, ListView.Center)
            return
        }
        glide.to = Math.max(list.originY, line.y - list.height * 0.28)
        glide.restart()
    }

    onCurrentChanged: follow()

    Connections {
        target: lyrics
        function onChanged() {
            glide.stop()
            list.positionViewAtBeginning()
            view.following = true
            view.follow()
        }
    }

    Timer {
        id: resume
        interval: 3500
        onTriggered: {
            view.following = true
            view.follow()
        }
    }

    NumberAnimation {
        id: glide
        target: list
        property: "contentY"
        duration: 650
        easing.type: Easing.OutCubic
    }

    ListView {
        id: list
        anchors.fill: parent
        visible: lyrics.status === "found"
        model: lyrics.lines
        clip: true
        spacing: view.synced ? 18 : 4
        cacheBuffer: 6000
        boundsBehavior: Flickable.StopAtBounds
        header: Item { width: 1; height: view.synced ? list.height * 0.28 : 12 }
        footer: Item { width: 1; height: view.synced ? list.height * 0.62 : list.height * 0.2 }

        onVisibleChanged: view.follow()
        onHeightChanged: view.follow()

        onMovementStarted: {
            glide.stop()
            view.following = false
            resume.stop()
        }
        onMovementEnded: resume.restart()

        // The words fade out at the top and bottom edges instead of being cut.
        layer.enabled: true
        layer.effect: MultiEffect {
            maskEnabled: true
            maskSource: edgeFade
            maskThresholdMin: 0.5
            maskSpreadAtMin: 1.0
        }

        delegate: Label {
            id: line
            required property var modelData
            required property int index
            readonly property bool sung: index < view.current
            readonly property bool singing: index === view.current

            width: list.width
            text: modelData.text !== "" ? modelData.text : view.synced ? "♪" : " "
            wrapMode: Text.WordWrap
            font.pixelSize: view.synced ? 26 : 18
            font.weight: view.synced ? Font.Bold : Font.Medium
            lineHeight: 1.08
            color: view.ink
            opacity: !view.synced ? 0.88 : singing ? 1 : sung ? 0.28 : 0.42
            Behavior on opacity { NumberAnimation { duration: 380 } }

            HoverHandler {
                enabled: view.synced
                cursorShape: Qt.PointingHandCursor
            }
            TapHandler {
                enabled: view.synced && view.player !== null
                onTapped: {
                    view.player.seekTo(line.modelData.time)
                    view.following = true
                }
            }
        }
    }

    Item {
        id: edgeFade
        anchors.fill: list
        visible: false
        layer.enabled: true
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: "transparent" }
                GradientStop { position: 0.1; color: "black" }
                GradientStop { position: 0.84; color: "black" }
                GradientStop { position: 1.0; color: "transparent" }
            }
        }
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - 24, 340)
        visible: lyrics.status !== "found"
        spacing: 10

        BusyIndicator {
            Layout.alignment: Qt.AlignHCenter
            running: lyrics.status === "loading"
            visible: running
        }
        Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            font.pixelSize: 20
            font.weight: Font.Bold
            color: Qt.rgba(view.ink.r, view.ink.g, view.ink.b, 0.8)
            text: lyrics.status === "loading" ? qsTr("Finding the lyrics…")
                : lyrics.status === "instrumental" ? qsTr("Instrumental")
                : lyrics.status === "missing" ? qsTr("No lyrics for this song yet")
                : lyrics.status === "failed" ? qsTr("Couldn't load the lyrics")
                : qsTr("Lyrics show up here while a song plays")
        }
        Label {
            Layout.fillWidth: true
            visible: text !== ""
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            font.pixelSize: 13
            color: Qt.rgba(view.ink.r, view.ink.g, view.ink.b, 0.55)
            text: lyrics.status === "failed" ? lyrics.error : ""
        }
        Button {
            Layout.alignment: Qt.AlignHCenter
            visible: lyrics.status === "failed"
            flat: true
            focusPolicy: Qt.NoFocus
            text: qsTr("Try again")
            onClicked: lyrics.retry()
        }
    }

    Label {
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        visible: lyrics.status === "found"
        text: lyrics.synced ? qsTr("Synced lyrics from LRCLIB") : qsTr("Lyrics from LRCLIB")
        font.pixelSize: 11
        color: Qt.rgba(view.ink.r, view.ink.g, view.ink.b, 0.4)
    }
}
