import QtQuick

// Album art that crossfades to the next cover instead of blinking, keeping
// the old one up until the new one has loaded.
Item {
    id: root

    property url source
    property size sourceSize: Qt.size(640, 640)
    readonly property bool ready: source.toString() !== ""
                                  && (front.status === Image.Ready || back.status === Image.Ready)

    onSourceChanged: if (source.toString() === "") back.source = ""

    Image {
        id: back
        anchors.fill: parent
        fillMode: Image.PreserveAspectCrop
        sourceSize: root.sourceSize
        asynchronous: true
        smooth: true
        mipmap: true
    }

    Image {
        id: front
        anchors.fill: parent
        fillMode: Image.PreserveAspectCrop
        sourceSize: root.sourceSize
        asynchronous: true
        smooth: true
        mipmap: true
        source: root.source
        opacity: 0

        // A cached cover can be ready before this handler runs.
        onSourceChanged: {
            opacity = 0
            if (status === Image.Ready)
                fadeIn.restart()
        }
        onStatusChanged: if (status === Image.Ready) fadeIn.restart()
    }

    NumberAnimation {
        id: fadeIn
        target: front
        property: "opacity"
        to: 1
        duration: 520
        easing.type: Easing.InOutQuad
        onFinished: back.source = front.source
    }
}
