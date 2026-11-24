import QtQuick
import QtQuick.Effects

// A frosted surface: a light tint, a hairline edge and a soft highlight from
// above, like iOS glass. Given a `backdrop`, it also blurs what that item
// shows underneath it, for sheets that float over the window's content.
Rectangle {
    id: panel

    property Item backdrop: null
    property real tint: theme.dark ? 0.08 : 0.42
    readonly property color base: theme.background

    color: Qt.rgba(1, 1, 1, panel.tint)
    border.width: 1
    border.color: Qt.rgba(1, 1, 1, theme.dark ? 0.13 : 0.55)

    // The blurred copy of what's behind, cut to the panel's rounded shape.
    ShaderEffectSource {
        id: behind
        anchors.fill: parent
        visible: false
        live: true
        sourceItem: panel.backdrop
        sourceRect: {
            if (!panel.backdrop)
                return Qt.rect(0, 0, 0, 0)
            // Re-evaluated when the panel moves or resizes.
            void (panel.x + panel.y + panel.width + panel.height)
            const origin = panel.mapToItem(panel.backdrop, 0, 0)
            return Qt.rect(origin.x, origin.y, panel.width, panel.height)
        }
    }

    Item {
        id: shape
        anchors.fill: parent
        visible: false
        layer.enabled: true
        Rectangle {
            anchors.fill: parent
            radius: panel.radius
            color: "black"
        }
    }

    MultiEffect {
        anchors.fill: parent
        z: -1
        visible: panel.backdrop !== null
        source: behind
        autoPaddingEnabled: false
        blurEnabled: true
        blur: 1.0
        blurMax: 48
        saturation: 0.2
        maskEnabled: true
        maskSource: shape
        maskThresholdMin: 0.5
        maskSpreadAtMin: 1.0
    }

    // Under a sheet the tint has to carry the theme's color, or text on it
    // stops reading against busy artwork.
    Rectangle {
        anchors.fill: parent
        radius: panel.radius
        visible: panel.backdrop !== null
        color: Qt.rgba(panel.base.r, panel.base.g, panel.base.b, 0.66)
        z: -1
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 1
        radius: Math.max(0, panel.radius - 1)
        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.rgba(1, 1, 1, theme.dark ? 0.07 : 0.3) }
            GradientStop { position: 0.45; color: Qt.rgba(1, 1, 1, 0) }
        }
    }
}
