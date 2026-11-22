import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

// Native objects stay in the page's QML context. Only their grid positions move.
Rectangle {
    id:card
    default property alias pChildren:content.data
    property string pIcon:"preferences"
    property string pDescription:""
    property bool pRowLayout:false
    property bool pHelpAvailable:false
    property bool pShowHelp:false
    property real spacing:6
    readonly property bool pInline:pRowLayout && width>=600
    readonly property bool pVoidSectionCard:true
    implicitWidth:680
    implicitHeight:Math.max(pRowLayout?72:0,content.implicitHeight+24)
    color:Theme.surface
    radius:Math.min(Theme.corner,12)
    border.width:1;border.color:Theme.border
    StudioGlyph {
        objectName:"voidReferenceSectionIcon"
        x:16;y:card.pIcon==="framerate"?16:(card.height-height)/2
        width:28;height:28;kind:card.pIcon;tint:Theme.accent
    }
    GridLayout {
        id:content
        x:58;y:12;width:Math.max(0,card.width-x-14)
        columns:card.pRowLayout?2:1
        columnSpacing:18;rowSpacing:card.spacing
        Text {
            visible:card.pDescription.length>0
            Layout.row:1;Layout.column:0;Layout.columnSpan:card.pInline?1:2
            Layout.fillWidth:true
            text:card.pDescription;wrapMode:Text.Wrap
            color:Theme.muted;font.family:Theme.fontFamily;font.pixelSize:11
        }
    }
    ToolButton {
        objectName:"voidReferenceFpsHelp"
        visible:card.pHelpAvailable
        x:16;y:50;width:28;height:24;padding:6
        Accessible.name:card.pShowHelp?"Hide FPS guidance":"Show FPS guidance"
        contentItem:StudioGlyph {kind:"about";tint:card.pShowHelp?Theme.accent:Theme.muted}
        background:Rectangle {radius:6;color:parent.hovered?Theme.raised:"transparent"}
        onClicked:card.pShowHelp=!card.pShowHelp
        ToolTip.visible:hovered;ToolTip.text:Accessible.name
    }
}
