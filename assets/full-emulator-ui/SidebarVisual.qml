// Theme-bound native emulator command rail. Passive visuals only; native input stays native.
import QtQuick 2.15
import "Icons.js" as Icons

Item {
    id: skin
    property string mode: "button"
    property string elementId: ""
    property string assetName: ""
    property bool hovered: false
    property bool pressed: false
    property bool available: true
    property bool active: false
    property bool animating: false
    property bool motionEnabled: Theme.motionEnabled
    // Reserve recognized glyphs during creation; never paint a stock icon while
    // its replacement warms up. A missing PNG restores the native fallback.
    readonly property bool known: iconKey !== "" && face.ready && mode === "button"
    readonly property string iconKey: resolveIcon()
    readonly property color ink: !available ? Theme.border : pressed ? Theme.text :
                                hovered || active ? Theme.text : Theme.text
    readonly property string svg: Icons.shapes[iconKey] || ""

    function resolveIcon() {
        var id = elementId.toLowerCase().replace(/[^a-z0-9]/g, "");
        var name = assetName.toLowerCase();
        if (id === "volume") return name.indexOf("mute") >= 0 ? "mute" : "volume";
        var aliases = {
            fullscreen:"fullscreen", gamecontrol:"keymap", gamecontrols:"keymap",
            controls:"keymap", memorytrim:"memory", trimmemory:"memory",
            installapk:"install", apk:"install", install:"install",
            screenshot:"screenshot", macro:"macro", macrorecorder:"macro",
            screenrecord:"record", screenrecording:"record", record:"record",
            media:"media", mediafolder:"media", mediamanager:"media", rotate:"rotate", rotatescreen:"rotate",
            setlocation:"location", location:"location", shake:"shake", sync:"sync",
            multiinstance:"sync", mim:"sync", eco:"eco", ecomode:"eco",
            settings:"advanced", back:"back", home:"home", recent:"recent",
            recentapps:"recent", kebab:"more", more:"more", lockcursor:"lock"
        };
        if (aliases[id]) return aliases[id];
        // Only explicit native asset names: unknown tools keep their original icons.
        var assets = {
            sidebarsound:"volume", sidebarcontrols:"keymap", sidebarmediafolder:"media",
            sidebarfullscreen:"fullscreen", sidebarvolume:"volume", sidebarmute:"mute",
            sidebargamecontrols:"keymap", sidebargamecontrol:"keymap",
            sidebarmemorytrim:"memory", sidebartrimmemory:"memory",
            sidebarinstallapk:"install", sidebarscreenshot:"screenshot",
            sidebarmacro:"macro", sidebarmacrorecorder:"macro",
            sidebarscreenrecord:"record", sidebarmedia:"media", sidebarrotate:"rotate",
            sidebarsetlocation:"location", sidebarshake:"shake", sidebarsync:"sync",
            sidebareco:"eco", sidebarmim:"sync", sidebarlockcursor:"lock",
            sidebarsettings:"advanced", navbarback:"back", navbarhome:"home",
            navbarrecent:"recent", sidebarkebab:"more"
        };
        name = name.replace(/(?:[_-]?(?:active|hover|pressed|disabled|white)|\.svg)+$/g, "");
        return assets[name] || "";
    }

    Rectangle {
        objectName:"voidRailBackground";anchors.fill:parent
        visible:skin.mode==="rail";color:Theme.background
        Rectangle {objectName:"voidRailEdge";width:1;height:parent.height;color:Theme.border}
    }
    ChromeButtonVisual {
        id:face;anchors.fill:parent;visible:skin.mode==="button"
        glyphObjectName:"bsiOutlineGlyph"
        iconSource:skin.svg?"SharedIcons/"+skin.iconKey+".png":""
        hovered:skin.hovered;pressed:skin.pressed;active:skin.active
        available:skin.available;animating:skin.animating;motionEnabled:skin.motionEnabled
    }
}
