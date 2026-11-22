import QtQuick 2.15

// One passive face for BOTH player chrome surfaces. Never intercepts input.
Item {
    id:face
    property url iconSource
    property string glyphObjectName:"voidChromeGlyph"
    property bool hovered:false
    property bool pressed:false
    property bool active:false
    property bool available:true
    property bool animating:false
    property bool motionEnabled:Theme.motionEnabled
    property bool danger:false
    readonly property bool ready:iconSource.toString().length>0 && glyph.status!==Image.Error
    readonly property color stateInk:danger && (hovered||pressed)?Theme.danger:(hovered||pressed||active?Theme.accent:Theme.muted)
    readonly property int transitionMs:Math.min(160,Theme.duration)
    scale:pressed && available?0.96:1
    Behavior on scale {enabled:face.motionEnabled;NumberAnimation {duration:face.transitionMs;easing.type:Easing.OutCubic}}
    Rectangle {
        objectName:"voidChromeHover"
        anchors.fill:parent;anchors.margins:4;radius:Math.min(8,Theme.corner)
        color:face.danger?Theme.danger:Theme.accent
        opacity:!face.available||!face.ready?0:face.pressed?0.20:face.active?0.15:face.hovered?0.10:0
        Behavior on opacity {enabled:face.motionEnabled;NumberAnimation {duration:face.transitionMs;easing.type:Easing.OutCubic}}
    }
    Rectangle {
        visible:face.active && face.available
        anchors.left:parent.left;anchors.verticalCenter:parent.verticalCenter
        width:2;height:14;radius:1;color:Theme.accent
    }
    ThemeIcon {
        id:glyph;objectName:face.glyphObjectName
        anchors.centerIn:parent;width:18;height:18;sourceSize:Qt.size(64,64)
        source:face.iconSource;tint:face.stateInk;opacity:face.available?1:0.35
        RotationAnimation on rotation {
            from:0;to:360;duration:1100;loops:Animation.Infinite
            running:face.motionEnabled && face.animating && face.available && face.visible
            onRunningChanged:if(!running)glyph.rotation=0
        }
    }
}
