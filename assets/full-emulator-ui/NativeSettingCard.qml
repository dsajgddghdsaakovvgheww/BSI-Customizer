import QtQuick 2.15
import QtQuick.Layouts 1.15

// Only the reviewed Performance section wrapper changes. Native objects remain
// in their original QML creation context; no surrogate settings/input model.
Rectangle {
    id:card
    default property alias pChildren:layout.data
    property string pIcon:"cpu"
    property real spacing:6
    property real padding:14
    readonly property bool pInline:width>=650
    readonly property bool pVoidSectionCard:true
    implicitWidth:680
    implicitHeight:layout.implicitHeight+padding*2
    color:Theme.surface
    radius:Math.min(Theme.corner,14)
    border.width:1
    border.color:Theme.border
    StudioGlyph {
        objectName:"voidReferenceSectionIcon"
        x:16;y:card.pIcon==="framerate"?18:Math.max(14,(card.height-29)/2)
        width:29;height:29;kind:card.pIcon;tint:Theme.accent
    }
    GridLayout {
        id:layout
        x:60;y:card.padding;width:Math.max(0,card.width-x-card.padding)
        columns:2
        columnSpacing:18;rowSpacing:card.spacing
    }
}
