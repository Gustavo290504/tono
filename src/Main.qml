import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Effects
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 440
    height: 800
    minimumWidth: 360
    minimumHeight: 640
    visible: true
    title: player && player.title ? player.title + " — Tono" : "Tono"

    readonly property QtObject player: players.current
    readonly property color base: theme.background
    readonly property color ink: theme.foreground
    readonly property color soft: Qt.rgba(ink.r, ink.g, ink.b, 0.64)

    // The window itself is see-through, and Hyprland blurs the desktop behind
    // it; the theme's background tints the glass. The tint is a rectangle
    // rather than the window color, because Qt clears with that color
    // unpremultiplied, which a light theme turns into garbage once composited.
    color: "transparent"
    background: Rectangle {
        color: Qt.rgba(win.base.r, win.base.g, win.base.b, 1 - settings.transparency * 0.75)
    }

    Material.theme: theme.dark ? Material.Dark : Material.Light
    Material.accent: theme.accent
    Material.foreground: theme.foreground
    Material.roundedScale: corner

    // Corners follow Hyprland's window rounding, or go soft when the user
    // asks for the iOS look. Round buttons are circles either way.
    readonly property int corner: settings.softCorners ? Math.max(22, theme.radius) : theme.radius
    readonly property int smallCorner: settings.softCorners ? 999 : Math.min(4, theme.radius)
    readonly property bool wide: width > 760 && width > height * 1.2

    property string toast: ""

    // The lyrics take the cover's place; while the search sheet is open its
    // text field gets the keys instead of the shortcuts.
    property bool showLyrics: false
    readonly property bool typing: search.visible

    function refreshLyrics() {
        if (showLyrics && player)
            lyrics.lookup(player.title, player.leadArtist, player.album, player.length)
    }

    onShowLyricsChanged: refreshLyrics()
    onPlayerChanged: refreshLyrics()

    Connections {
        target: win.player
        function onTrackChanged() { win.refreshLyrics() }
    }

    function formatTime(ms) {
        const total = Math.max(0, Math.floor(ms / 1000))
        const minutes = Math.floor(total / 60), seconds = total % 60
        const locale = Qt.locale()
        return minutes.toLocaleString(locale, 'f', 0) + ":"
             + (seconds < 10 ? (0).toLocaleString(locale, 'f', 0) : "") + seconds.toLocaleString(locale, 'f', 0)
    }

    function showToast(message) {
        toast = message
        toastTimer.restart()
    }

    Connections {
        target: players
        function onFailed(message) { win.showToast(message) }
    }

    Timer { id: toastTimer; interval: 4000; onTriggered: win.toast = "" }

    Shortcut { sequence: "?"; enabled: !win.typing; context: Qt.ApplicationShortcut; onActivated: shortcuts.visible ? shortcuts.close() : shortcuts.open() }
    Shortcut { sequence: ","; enabled: !win.typing; context: Qt.ApplicationShortcut; onActivated: preferences.visible ? preferences.close() : preferences.open() }
    Shortcut { sequence: "Q"; enabled: !win.typing; context: Qt.ApplicationShortcut; onActivated: Qt.quit() }
    Shortcut { sequence: "Space"; enabled: !win.typing; context: Qt.ApplicationShortcut; onActivated: if (win.player) win.player.playPause() }
    Shortcut { sequences: ["N", "Ctrl+Right"]; enabled: !win.typing; context: Qt.ApplicationShortcut; onActivated: if (win.player) win.player.next() }
    Shortcut { sequences: ["P", "Ctrl+Left"]; enabled: !win.typing; context: Qt.ApplicationShortcut; onActivated: if (win.player) win.player.previous() }
    Shortcut { sequence: "Right"; enabled: !win.typing; context: Qt.ApplicationShortcut; onActivated: if (win.player) win.player.seekBy(10000) }
    Shortcut { sequence: "Left"; enabled: !win.typing; context: Qt.ApplicationShortcut; onActivated: if (win.player) win.player.seekBy(-10000) }
    Shortcut { sequence: "Up"; enabled: !win.typing; context: Qt.ApplicationShortcut; onActivated: if (win.player) win.player.setVolume(win.player.volume + 0.05) }
    Shortcut { sequence: "Down"; enabled: !win.typing; context: Qt.ApplicationShortcut; onActivated: if (win.player) win.player.setVolume(win.player.volume - 0.05) }
    Shortcut { sequence: "S"; enabled: !win.typing; context: Qt.ApplicationShortcut; onActivated: if (win.player) win.player.toggleShuffle() }
    Shortcut { sequence: "R"; enabled: !win.typing; context: Qt.ApplicationShortcut; onActivated: if (win.player) win.player.cycleLoop() }
    Shortcut { sequence: "Tab"; enabled: !win.typing; context: Qt.ApplicationShortcut; onActivated: players.selectNext() }
    Shortcut { sequence: "Ctrl+F"; context: Qt.ApplicationShortcut; onActivated: search.visible ? search.close() : search.open() }
    Shortcut { sequence: "/"; enabled: !win.typing; context: Qt.ApplicationShortcut; onActivated: search.open() }
    Shortcut { sequence: "L"; enabled: !win.typing; context: Qt.ApplicationShortcut; onActivated: win.showLyrics = !win.showLyrics }
    Shortcut { sequence: "O"; enabled: !win.typing; context: Qt.ApplicationShortcut; onActivated: if (win.player) win.player.raise() }

    // Right-to-left languages mirror the window; the Translator sets the
    // application's layout direction.
    Item {
        id: root
        anchors.fill: parent
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        // The cover, blurred far past recognition and slowly turning, colors
        // the glass the way Apple Music's player does.
        Item {
            id: backdrop
            anchors.fill: parent
            opacity: backdropArt.ready ? settings.artworkGlow : 0
            visible: opacity > 0
            Behavior on opacity { NumberAnimation { duration: 700 } }

            layer.enabled: true
            layer.effect: MultiEffect {
                autoPaddingEnabled: false
                blurEnabled: true
                blur: 1.0
                blurMax: 64
                saturation: 0.3
                brightness: theme.dark ? -0.08 : 0.0
            }

            ArtworkFade {
                id: backdropArt
                width: Math.hypot(parent.width, parent.height) * 1.1
                height: width
                anchors.centerIn: parent
                source: win.player ? win.player.artUrl : ""
                sourceSize: Qt.size(160, 160)

                RotationAnimation on rotation {
                    from: 0
                    to: 360
                    duration: 120000
                    loops: Animation.Infinite
                    running: settings.motion && win.player !== null && win.player.playing
                }
            }
        }

        // Keeps text readable when the artwork is bright.
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.rgba(win.base.r, win.base.g, win.base.b, 0.05) }
                GradientStop { position: 1.0; color: Qt.rgba(win.base.r, win.base.g, win.base.b, 0.4) }
            }
        }

        RowLayout {
            id: topBar
            anchors { top: parent.top; left: parent.left; right: parent.right; margins: 18 }
            spacing: 8

            SourceStrip {
                visible: players.players.length > 0
                radius: settings.softCorners ? height / 2 : win.corner
            }
            Item { Layout.fillWidth: true }
            GlassIconButton {
                size: 36
                icon.source: "qrc:/icons/search.svg"
                tip: qsTr("Search")
                onClicked: search.open()
            }
            GlassIconButton {
                size: 36
                icon.source: "qrc:/icons/lyrics.svg"
                tip: qsTr("Lyrics")
                active: win.showLyrics
                onClicked: win.showLyrics = !win.showLyrics
            }
            GlassIconButton {
                size: 36
                icon.source: "qrc:/icons/preferences.svg"
                tip: qsTr("Preferences")
                onClicked: preferences.open()
            }
            GlassIconButton {
                size: 36
                icon.source: "qrc:/icons/keyboard.svg"
                tip: qsTr("Shortcuts")
                onClicked: shortcuts.open()
            }
        }

        IdleState {
            anchors.centerIn: parent
            visible: win.player === null
            corner: win.corner
        }

        GridLayout {
            id: nowPlaying
            visible: win.player !== null
            anchors {
                top: topBar.bottom
                left: parent.left
                right: parent.right
                bottom: parent.bottom
                leftMargin: win.width < 420 ? 20 : 30
                rightMargin: win.width < 420 ? 20 : 30
                topMargin: win.wide ? 18 : 24
                bottomMargin: 26
            }
            columns: win.wide ? 2 : 1
            columnSpacing: 40
            rowSpacing: 0

            // The cover, shrinking back a little while paused.
            Item {
                id: cover
                visible: !win.showLyrics
                readonly property real side: win.wide
                    ? Math.min(nowPlaying.height - 20, nowPlaying.width * 0.48)
                    : Math.min(nowPlaying.width, nowPlaying.height * 0.5)
                Layout.preferredWidth: side
                Layout.preferredHeight: side
                Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                Layout.bottomMargin: win.wide ? 0 : 26

                scale: win.player && win.player.playing ? 1 : 0.86
                Behavior on scale { NumberAnimation { duration: 520; easing.type: Easing.OutBack; easing.overshoot: 1.2 } }

                RectangularShadow {
                    anchors.fill: parent
                    radius: win.corner
                    blur: 48
                    spread: -6
                    offset.y: 18
                    color: Qt.rgba(0, 0, 0, theme.dark ? 0.55 : 0.25)
                }

                GlassPanel {
                    anchors.fill: parent
                    radius: win.corner
                    visible: !coverArt.ready

                    TintedIcon {
                        anchors.centerIn: parent
                        width: parent.width * 0.3
                        height: width
                        source: "qrc:/icons/note.svg"
                        color: Qt.rgba(win.ink.r, win.ink.g, win.ink.b, 0.35)
                    }
                }

                ArtworkFade {
                    id: coverArt
                    anchors.fill: parent
                    source: win.player ? win.player.artUrl : ""
                    sourceSize: Qt.size(800, 800)
                    visible: ready

                    layer.enabled: win.corner > 0
                    layer.effect: MultiEffect {
                        maskEnabled: true
                        maskSource: coverMask
                        maskThresholdMin: 0.5
                        maskSpreadAtMin: 1.0
                    }
                }

                Item {
                    id: coverMask
                    anchors.fill: parent
                    visible: false
                    layer.enabled: true
                    Rectangle { anchors.fill: parent; radius: win.corner; color: "black" }
                }

                // A hairline of glass around the cover.
                Rectangle {
                    anchors.fill: parent
                    radius: win.corner
                    color: "transparent"
                    border.width: 1
                    border.color: Qt.rgba(1, 1, 1, 0.12)
                }
            }

            LyricsView {
                visible: win.showLyrics
                player: win.player
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: win.wide ? cover.side : -1
                Layout.bottomMargin: win.wide ? 0 : 18
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: !win.wide && !win.showLyrics
                Layout.alignment: Qt.AlignVCenter
                spacing: 0

                Label {
                    Layout.fillWidth: true
                    text: win.player && win.player.title ? win.player.title : qsTr("Not playing")
                    font.pixelSize: 24
                    font.weight: Font.Bold
                    font.letterSpacing: -0.3
                    color: win.ink
                    elide: Text.ElideRight
                }
                Label {
                    Layout.fillWidth: true
                    Layout.topMargin: 2
                    text: win.player ? (win.player.artist || win.player.identity) : ""
                    font.pixelSize: 18
                    color: win.soft
                    elide: Text.ElideRight
                }
                Label {
                    Layout.fillWidth: true
                    Layout.topMargin: 2
                    visible: text !== ""
                    text: win.player ? win.player.album : ""
                    font.pixelSize: 13
                    color: Qt.rgba(win.ink.r, win.ink.g, win.ink.b, 0.45)
                    elide: Text.ElideRight
                }

                TrackScrubber {
                    id: scrubber
                    Layout.fillWidth: true
                    Layout.topMargin: 22
                    enabled: win.player !== null && win.player.canSeek
                    corner: win.smallCorner
                    from: 0
                    to: 1
                    onPressedChanged: if (!pressed && win.player) win.player.seekTo(value * win.player.length)
                }
                Binding {
                    target: scrubber
                    property: "value"
                    when: !scrubber.pressed
                    value: win.player && win.player.length > 0 ? win.player.position / win.player.length : 0
                    restoreMode: Binding.RestoreNone
                }

                RowLayout {
                    Layout.fillWidth: true
                    readonly property real shown: scrubber.pressed && win.player ? scrubber.value * win.player.length
                                                                                 : win.player ? win.player.position : 0
                    id: times
                    Label {
                        text: win.formatTime(times.shown)
                        font.pixelSize: 12
                        font.features: { "tnum": 1 }
                        color: win.soft
                    }
                    Item { Layout.fillWidth: true }
                    Label {
                        text: win.player && win.player.length > 0
                              ? "−" + win.formatTime(win.player.length - times.shown) : ""
                        font.pixelSize: 12
                        font.features: { "tnum": 1 }
                        color: win.soft
                    }
                }

                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: 14
                    // The five buttons are 270 wide; the gaps give way first
                    // on a narrow window.
                    spacing: Math.max(6, Math.min(18, (nowPlaying.width - 270) / 4))

                    GlassIconButton {
                        size: 40
                        icon.source: "qrc:/icons/shuffle.svg"
                        tip: qsTr("Shuffle")
                        active: win.player !== null && win.player.shuffle
                        enabled: win.player !== null && win.player.hasShuffle
                        onClicked: win.player.toggleShuffle()
                    }
                    GlassIconButton {
                        size: 56
                        icon.source: "qrc:/icons/previous.svg"
                        tip: qsTr("Previous track")
                        enabled: win.player !== null && win.player.canGoPrevious
                        onClicked: win.player.previous()
                    }
                    GlassIconButton {
                        size: 78
                        iconScale: 0.4
                        filled: true
                        icon.source: win.player && win.player.playing ? "qrc:/icons/pause.svg" : "qrc:/icons/play.svg"
                        tip: win.player && win.player.playing ? qsTr("Pause") : qsTr("Play")
                        enabled: win.player !== null
                        onClicked: win.player.playPause()
                    }
                    GlassIconButton {
                        size: 56
                        icon.source: "qrc:/icons/next.svg"
                        tip: qsTr("Next track")
                        enabled: win.player !== null && win.player.canGoNext
                        onClicked: win.player.next()
                    }
                    GlassIconButton {
                        size: 40
                        icon.source: win.player && win.player.loopStatus === "Track" ? "qrc:/icons/repeat-one.svg"
                                                                                     : "qrc:/icons/repeat.svg"
                        tip: qsTr("Repeat")
                        active: win.player !== null && win.player.loopStatus !== "None"
                        enabled: win.player !== null && win.player.hasLoop
                        onClicked: win.player.cycleLoop()
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: 18
                    visible: win.player !== null && win.player.hasVolume
                    spacing: 10

                    TintedIcon {
                        Layout.preferredWidth: 18
                        Layout.preferredHeight: 18
                        source: "qrc:/icons/volume-low.svg"
                        color: win.soft
                    }
                    TrackScrubber {
                        id: volume
                        Layout.fillWidth: true
                        corner: win.smallCorner
                        from: 0
                        to: 1
                        onMoved: if (win.player) win.player.setVolume(value)
                    }
                    Binding {
                        target: volume
                        property: "value"
                        when: !volume.pressed
                        value: win.player ? win.player.volume : 0
                        restoreMode: Binding.RestoreNone
                    }
                    TintedIcon {
                        Layout.preferredWidth: 18
                        Layout.preferredHeight: 18
                        source: "qrc:/icons/volume-high.svg"
                        color: win.soft
                    }
                }

                Item { Layout.fillHeight: true; visible: !win.wide && !win.showLyrics }
            }
        }

        // What went wrong with the last thing a player was asked to do.
        GlassPanel {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 20
            width: Math.min(parent.width - 40, toastLabel.implicitWidth + 32)
            height: toastLabel.implicitHeight + 18
            radius: settings.softCorners ? height / 2 : win.corner
            backdrop: null
            tint: theme.dark ? 0.14 : 0.6
            opacity: win.toast !== "" ? 1 : 0
            visible: opacity > 0
            Behavior on opacity { NumberAnimation { duration: 200 } }

            Label {
                id: toastLabel
                anchors.centerIn: parent
                width: Math.min(implicitWidth, parent.parent.width - 72)
                text: win.toast
                color: win.ink
                font.pixelSize: 13
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }

    PreferencesSheet {
        id: preferences
        backdrop: root
        corner: win.corner
        onConnectSpotify: {
            close()
            search.open()
        }
    }

    SearchSheet {
        id: search
        backdrop: root
        corner: win.corner
        smallCorner: win.smallCorner
    }

    ShortcutSheet {
        id: shortcuts
        backdrop: root
        corner: win.corner
        keyCorner: settings.softCorners ? 8 : win.smallCorner
    }
}
