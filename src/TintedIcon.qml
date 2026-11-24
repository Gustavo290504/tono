import QtQuick
import QtQuick.Effects

// An SVG icon painted in exactly one color, whatever color the file uses:
// the glyph is the mask, a flat rectangle the paint.
Item {
    id: icon

    property url source
    property color color: "white"

    implicitWidth: 24
    implicitHeight: 24

    Image {
        id: glyph
        anchors.fill: parent
        source: icon.source
        sourceSize: Qt.size(Math.ceil(icon.width * Screen.devicePixelRatio),
                            Math.ceil(icon.height * Screen.devicePixelRatio))
        smooth: true
        visible: false
    }

    Rectangle {
        id: paint
        anchors.fill: parent
        color: icon.color
        visible: false
        layer.enabled: true
    }

    MultiEffect {
        anchors.fill: parent
        source: paint
        maskEnabled: true
        maskSource: glyph
        maskThresholdMin: 0.5
        maskSpreadAtMin: 1.0
    }
}
