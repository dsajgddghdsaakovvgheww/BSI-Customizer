import QtQuick 2.15
import QtQuick.Layouts 1.15

// Reviewed Performance content wrapper only. No new controls or backend model.
// Existing native children keep their creation context, ids and handlers.
Item {
    id:panel
    default property alias pChildren:grid.data
    property real spacing:12
    readonly property int columns:width>=736?2:1
    implicitWidth:760
    implicitHeight:grid.implicitHeight
    GridLayout {
        id:grid
        width:parent.width
        columns:panel.columns
        columnSpacing:panel.spacing
        rowSpacing:panel.spacing
    }
}
