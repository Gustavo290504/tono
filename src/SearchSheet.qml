import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Effects
import QtQuick.Layouts

// Search Spotify for songs and playlists, or filter your own playlists, and
// play a result in the Spotify app. Without a connected account it explains
// how to connect one, and can still hand the search to the Spotify app.
Popup {
    id: sheet

    property Item backdrop: null
    property int corner: 0
    property int smallCorner: 0
    property string kind: "track"
    readonly property bool signedIn: spotify.state === "signedIn"
    readonly property color ink: theme.foreground
    readonly property color faint: Qt.rgba(ink.r, ink.g, ink.b, 0.6)
    readonly property color accent: theme.accent

    function runSearch() {
        spotify.search(field.text, kind)
    }

    function play(index) {
        const item = spotify.results[index]
        if (!item)
            return
        players.playUri(item.uri)
        close()
    }

    function searchInSpotify() {
        const query = field.text.trim()
        players.playUri(query !== "" ? "spotify:search:" + encodeURIComponent(query) : "spotify:")
        close()
    }

    function formatTime(ms) {
        const total = Math.max(0, Math.floor(ms / 1000))
        const minutes = Math.floor(total / 60), seconds = total % 60
        const locale = Qt.locale()
        return minutes.toLocaleString(locale, 'f', 0) + ":"
             + (seconds < 10 ? (0).toLocaleString(locale, 'f', 0) : "") + seconds.toLocaleString(locale, 'f', 0)
    }

    anchors.centerIn: Overlay.overlay
    width: Math.min(560, (parent ? parent.width : 560) - 24)
    height: (parent ? parent.height : 700) - 36
    modal: true
    dim: true
    focus: true
    padding: 18
    Material.roundedScale: sheet.smallCorner

    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, 0.25) }

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 180 }
        NumberAnimation { property: "scale"; from: 0.96; to: 1; duration: 220; easing.type: Easing.OutCubic }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 140 }
    }

    background: GlassPanel {
        radius: sheet.corner
        backdrop: sheet.backdrop
    }

    onOpened: {
        field.forceActiveFocus()
        field.selectAll()
        runSearch()
    }
    onClosed: spotify.clearError()
    onKindChanged: runSearch()

    Timer {
        id: debounce
        interval: 350
        onTriggered: sheet.runSearch()
    }

    // A sign-in finishing while the sheet is open shows results right away.
    Connections {
        target: spotify
        function onAccountChanged() {
            if (sheet.opened && sheet.signedIn)
                sheet.runSearch()
        }
    }

    contentItem: ColumnLayout {
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            TextField {
                id: field
                Layout.fillWidth: true
                placeholderText: sheet.kind === "mine" ? qsTr("Filter your playlists") : qsTr("Search Spotify")
                onTextEdited: debounce.restart()
                onAccepted: {
                    if (sheet.signedIn && results.count > 0)
                        sheet.play(Math.max(0, results.currentIndex))
                    else if (!sheet.signedIn)
                        sheet.searchInSpotify()
                }
                Keys.onDownPressed: results.incrementCurrentIndex()
                Keys.onUpPressed: results.decrementCurrentIndex()
                Keys.onTabPressed: event => {
                    const kinds = ["track", "playlist", "mine"]
                    sheet.kind = kinds[(kinds.indexOf(sheet.kind) + 1) % kinds.length]
                    event.accepted = true
                }
            }
            GlassIconButton {
                size: 34
                icon.source: "qrc:/icons/close.svg"
                tip: qsTr("Close")
                onClicked: sheet.close()
            }
        }

        RowLayout {
            spacing: 6
            visible: sheet.signedIn

            Repeater {
                model: [
                    { key: "track", label: qsTr("Songs") },
                    { key: "playlist", label: qsTr("Playlists") },
                    { key: "mine", label: qsTr("Your playlists") }
                ]

                delegate: AbstractButton {
                    id: chip
                    required property var modelData
                    readonly property bool chosen: sheet.kind === modelData.key

                    implicitHeight: 32
                    implicitWidth: chipLabel.implicitWidth + 28
                    focusPolicy: Qt.NoFocus
                    hoverEnabled: true
                    onClicked: sheet.kind = modelData.key

                    background: Rectangle {
                        radius: settings.softCorners ? height / 2 : sheet.smallCorner
                        color: chip.chosen ? Qt.rgba(sheet.accent.r, sheet.accent.g, sheet.accent.b, 0.9)
                                           : Qt.rgba(1, 1, 1, chip.hovered ? 0.16 : 0.08)
                        border.width: 1
                        border.color: Qt.rgba(1, 1, 1, chip.chosen ? 0.3 : 0.12)
                    }
                    contentItem: Label {
                        id: chipLabel
                        text: chip.modelData.label
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        font.pixelSize: 13
                        font.weight: Font.Medium
                        color: chip.chosen ? theme.accentForeground : sheet.ink
                    }
                }
            }
        }

        Label {
            Layout.fillWidth: true
            visible: spotify.error !== ""
            text: spotify.error
            wrapMode: Text.WordWrap
            font.pixelSize: 13
            color: theme.danger
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                id: results
                anchors.fill: parent
                visible: sheet.signedIn
                model: spotify.results
                clip: true
                spacing: 2
                boundsBehavior: Flickable.StopAtBounds
                highlightMoveDuration: 0
                onCountChanged: currentIndex = count > 0 ? 0 : -1

                delegate: AbstractButton {
                    id: row
                    required property var modelData
                    required property int index

                    width: results.width
                    height: 62
                    hoverEnabled: true
                    focusPolicy: Qt.NoFocus
                    onClicked: sheet.play(index)

                    background: Rectangle {
                        radius: settings.softCorners ? 12 : sheet.smallCorner
                        color: Qt.rgba(1, 1, 1, row.ListView.isCurrentItem ? 0.12 : row.hovered ? 0.07 : 0)
                    }

                    contentItem: RowLayout {
                        spacing: 12

                        Item {
                            Layout.leftMargin: 8
                            Layout.preferredWidth: 46
                            Layout.preferredHeight: 46

                            Rectangle {
                                anchors.fill: parent
                                radius: thumbMask.radius
                                color: Qt.rgba(1, 1, 1, 0.08)
                                visible: thumb.status !== Image.Ready
                                TintedIcon {
                                    anchors.centerIn: parent
                                    width: 22
                                    height: 22
                                    source: row.modelData.kind === "track" ? "qrc:/icons/note.svg" : "qrc:/icons/playlist.svg"
                                    color: sheet.faint
                                }
                            }
                            Image {
                                id: thumb
                                anchors.fill: parent
                                source: row.modelData.image
                                sourceSize: Qt.size(92, 92)
                                fillMode: Image.PreserveAspectCrop
                                asynchronous: true
                                visible: false
                            }
                            MultiEffect {
                                anchors.fill: parent
                                source: thumb
                                visible: thumb.status === Image.Ready
                                maskEnabled: true
                                maskSource: thumbShape
                                maskThresholdMin: 0.5
                                maskSpreadAtMin: 1.0
                            }
                            Item {
                                id: thumbShape
                                anchors.fill: parent
                                visible: false
                                layer.enabled: true
                                Rectangle {
                                    id: thumbMask
                                    anchors.fill: parent
                                    radius: settings.softCorners ? 8 : sheet.smallCorner
                                    color: "black"
                                }
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Label {
                                Layout.fillWidth: true
                                text: row.modelData.title
                                elide: Text.ElideRight
                                font.pixelSize: 15
                                font.weight: Font.Medium
                                color: sheet.ink
                            }
                            Label {
                                Layout.fillWidth: true
                                visible: text !== ""
                                text: row.modelData.subtitle
                                elide: Text.ElideRight
                                font.pixelSize: 12
                                color: sheet.faint
                            }
                        }

                        Label {
                            Layout.rightMargin: 10
                            visible: row.modelData.durationMs > 0
                            text: sheet.formatTime(row.modelData.durationMs)
                            font.pixelSize: 12
                            font.features: { "tnum": 1 }
                            color: sheet.faint
                        }
                    }
                }
            }

            BusyIndicator {
                anchors.centerIn: parent
                running: sheet.signedIn && spotify.searching
                visible: running
            }

            Label {
                anchors.centerIn: parent
                width: Math.min(parent.width - 24, 320)
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                visible: sheet.signedIn && !spotify.searching && results.count === 0 && spotify.error === ""
                text: field.text.trim() === "" && sheet.kind !== "mine"
                      ? qsTr("Type to search songs and playlists on Spotify.")
                      : qsTr("Nothing found.")
                font.pixelSize: 15
                color: sheet.faint
            }

            // How to connect, for searching inside Tono.
            Flickable {
                anchors.fill: parent
                visible: !sheet.signedIn
                contentHeight: connect.implicitHeight
                clip: true
                boundsBehavior: Flickable.StopAtBounds

                ColumnLayout {
                    id: connect
                    width: parent.width
                    spacing: 10

                    Label {
                        Layout.fillWidth: true
                        Layout.topMargin: 4
                        text: qsTr("Connect your Spotify account")
                        font.pixelSize: 18
                        font.weight: Font.Bold
                        color: sheet.ink
                        wrapMode: Text.WordWrap
                    }
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Searching from Tono goes through a Spotify developer app of your own, which needs Premium on the account that creates it. Without one, Enter searches in the Spotify app instead.")
                        font.pixelSize: 13
                        color: sheet.faint
                        wrapMode: Text.WordWrap
                    }

                    Label {
                        Layout.fillWidth: true
                        Layout.topMargin: 6
                        text: qsTr("1. Create an app in the Spotify developer dashboard, with the Web API.")
                        font.pixelSize: 14
                        color: sheet.ink
                        wrapMode: Text.WordWrap
                    }
                    Button {
                        flat: true
                        focusPolicy: Qt.NoFocus
                        text: qsTr("Open the dashboard")
                        onClicked: Qt.openUrlExternally("https://developer.spotify.com/dashboard")
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("2. Add this Redirect URI to the app:")
                        font.pixelSize: 14
                        color: sheet.ink
                        wrapMode: Text.WordWrap
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: 36
                            radius: sheet.smallCorner
                            color: Qt.rgba(1, 1, 1, 0.08)
                            border.width: 1
                            border.color: Qt.rgba(1, 1, 1, 0.14)
                            Label {
                                anchors.fill: parent
                                anchors.leftMargin: 10
                                verticalAlignment: Text.AlignVCenter
                                text: spotify.redirectUri
                                font.family: "monospace"
                                font.pixelSize: 13
                                color: sheet.ink
                                elide: Text.ElideRight
                            }
                        }
                        Button {
                            id: copyButton
                            property bool copied: false
                            focusPolicy: Qt.NoFocus
                            text: copied ? qsTr("Copied") : qsTr("Copy")
                            onClicked: {
                                spotify.copyRedirectUri()
                                copied = true
                                copiedReset.restart()
                            }
                            Timer { id: copiedReset; interval: 2000; onTriggered: copyButton.copied = false }
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        Layout.topMargin: 6
                        text: qsTr("3. Paste the app's Client ID and connect.")
                        font.pixelSize: 14
                        color: sheet.ink
                        wrapMode: Text.WordWrap
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        TextField {
                            id: clientId
                            Layout.fillWidth: true
                            placeholderText: qsTr("Client ID")
                            text: spotify.clientId
                            font.family: "monospace"
                            onTextEdited: spotify.clientId = text
                            onAccepted: if (spotify.isClientId(text)) spotify.signIn()
                        }
                        Button {
                            highlighted: true
                            focusPolicy: Qt.NoFocus
                            enabled: spotify.isClientId(clientId.text) && spotify.state !== "signingIn"
                            text: qsTr("Connect")
                            onClicked: spotify.signIn()
                        }
                    }

                    RowLayout {
                        visible: spotify.state === "signingIn"
                        spacing: 8
                        BusyIndicator { implicitWidth: 28; implicitHeight: 28; running: parent.visible }
                        Label {
                            text: qsTr("Finish signing in in your browser…")
                            font.pixelSize: 13
                            color: sheet.faint
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                Layout.fillWidth: true
                text: sheet.signedIn && spotify.userName !== "" ? qsTr("Signed in as %1").arg(spotify.userName) : ""
                elide: Text.ElideRight
                font.pixelSize: 12
                color: sheet.faint
            }
            Button {
                flat: true
                focusPolicy: Qt.NoFocus
                text: qsTr("Search in the Spotify app")
                onClicked: sheet.searchInSpotify()
            }
        }
    }
}
