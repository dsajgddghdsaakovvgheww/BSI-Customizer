import QtQuick 2.15
import QtQuick.Layouts 1.15

// Vertical, native content. No grid positions, surrogate inputs or model copies.
Rectangle {
    id:card
    default property alias pChildren:content.data
    property string pIcon:"preferences"
    property string pDescription:""
    property real spacing:10
    readonly property bool pVoidSectionCard:true
    implicitWidth:620
    implicitHeight:stack.implicitHeight+32
    color:Theme.surface
    radius:Math.min(Theme.corner,12)
    border.width:1
    border.color:Theme.border
    Rectangle {
        x:16;y:16;width:30;height:30;radius:8
        color:Qt.rgba(Theme.accent.r,Theme.accent.g,Theme.accent.b,0.08)
        StudioGlyph {objectName:"voidReferenceSectionIcon";anchors.centerIn:parent;width:19;height:19;kind:card.pIcon;tint:Theme.accent}
    }
    ColumnLayout {
        id:stack
        x:60;y:16;width:Math.max(0,card.width-x-18)
        spacing:8
        ColumnLayout {
            id:content
            Layout.fillWidth:true
            spacing:card.spacing
        }
        Text {
            visible:card.pDescription.length>0
            Layout.fillWidth:true
            text:card.pDescription;wrapMode:Text.Wrap
            color:Theme.muted;font.family:Theme.fontFamily;font.pixelSize:11
            lineHeight:1.2
        }
    }
}
