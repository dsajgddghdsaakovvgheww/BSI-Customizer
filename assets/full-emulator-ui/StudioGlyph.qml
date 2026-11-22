import QtQuick 2.15
import "BSISettingsIconsV3.js" as NativeIcons

ThemeIcon {
    property string kind:"preferences"
    readonly property string resolvedKind: ["cpu","memory","rocket","framerate","brand","chevrondown","minimize","sync","check","layers"].indexOf(kind)>=0?kind:NativeIcons.normalizedKind(kind)
    source:Qt.resolvedUrl("StudioIcons/"+resolvedKind+".png")
    sourceSize:Qt.size(64,64)
}
