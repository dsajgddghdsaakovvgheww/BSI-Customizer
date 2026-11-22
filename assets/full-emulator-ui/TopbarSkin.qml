// Passive titlebar glyphs: native UiImageButton retains input, tooltips and actions.
import QtQuick 2.15
import QtQuick.Window 2.15
import "Icons.js" as Icons
Item {
    id: skin
    objectName:"voidReferenceTopbarSkin"
    property var host
    property var watched:new Set()
    property var pending:new Set()
    property bool scheduled:false
    property int visitedNodes:0
    readonly property url brandSource:Qt.resolvedUrl("Branding/VOID-Mark.png")
    ThemeRelay {hostWindow:skin.host ? skin.host.Window.window : null}
    property var borderVisual:null
    function prepareHost() {
        if(!host)return
        // plr.qml assigns the OEM color on the PlrTopbar INSTANCE. Source-color
        // rewriting inside PlrTopbar.qml alone cannot override that binding.
        if(host.color!==undefined)host.color=Qt.binding(function(){return Theme.background})
        if(host.gradient!==undefined)host.gradient=null
        if(!borderVisual)borderVisual=divider.createObject(host)
        walk(host,0)
    }
    function enqueue(item) {
        if (!item) return
        pending.add(item)
        if(scheduled)return
        scheduled=true;Qt.callLater(flush)
    }
    function flush() {
        scheduled=false;var nodes=Array.from(pending);pending.clear()
        for(var i=0;i<nodes.length;++i) {
            var node=nodes[i]
            if(!node)continue
            if(!watched.has(node))walk(node,0)
            else if(node.children)for(var j=0;j<node.children.length;++j)walk(node.children[j],1)
        }
    }
    function walk(item,depth) {
        if(!item || depth>32 || watched.size>4096 || watched.has(item))return
        if(String(item.objectName||"").indexOf("voidReference")===0)return
        watched.add(item);visitedNodes++
        if(item.children!==undefined) {
            var seen=watched,queue=pending
            hook.createObject(item,{owner:item,cleanup:function(){seen.delete(item);queue.delete(item)}})
        }
        if(/UiImage/.test(String(item)) && !/UiImageButton/.test(String(item)) && item.asset==="BlueStacksLogo" && item.source!==undefined) {
            item.source=Qt.binding(function(){return skin.brandSource})
            item.asynchronous=false
            if(item.pImageWidth!==undefined)item.pImageWidth=28
            if(item.pImageHeight!==undefined)item.pImageHeight=20
        }
        if(/UiImageButton/.test(String(item)) && item.contentItem && item.asset!==undefined)
            face.createObject(item,{control:item})
        if(item.children)for(var i=0;i<item.children.length;++i)walk(item.children[i],depth+1)
    }
    function icon(asset) {
        var key=String(asset||"").toLowerCase()
        var aliases={titlebarclose:"close",titlebarminimize:"minimize",titlebarmaximize:"maximize",titlebarrestore:"restore",titlebarcollapse:"collapse",titlebarexpand:"expand",titlebarmenu:"menu",titlebarhelp:"help",titlebardisablepintop:"lock",sidebarback:"back",sidebarhome:"home",sidebarrecents:"recent"}
        return aliases[key]||""
    }
    Component { id:divider; Rectangle {objectName:"voidReferenceChromeDivider";anchors.left:parent.left;anchors.right:parent.right;anchors.bottom:parent.bottom;height:1;color:Theme.border} }
    Component { id:hook; Connections {
        property var owner
        property var cleanup
        Component.onDestruction: if (cleanup) cleanup()
        target:owner
        function onChildrenChanged(){skin.enqueue(owner)}
    } }
    property bool initialized:false
    onHostChanged:if(initialized)prepareHost()
    Component.onCompleted:{initialized=true;Theme.sync();prepareHost()}
    Connections {
        target:skin.host ? skin.host.Window.window : null
        function onActiveChanged(){if(target && target.active)Theme.sync()}
    }
    Component { id:face
        ChromeButtonVisual {
            id:visual
            objectName:"voidReferenceTopbarFace"
            property var control
            readonly property string key:skin.icon(control.asset)
            anchors.fill:parent;z:5
            iconSource:key?"SharedIcons/"+key+".png":""
            hovered:control.hovered;pressed:control.down;available:control.enabled
            danger:key==="close"
            function updateNative() {
                control.contentItem.opacity=Qt.binding(function(){return visual.ready?0:1})
                if(control.background)control.background.opacity=Qt.binding(function(){return visual.ready?0:1})
                var children=control.children
                for(var i=0;i<children.length;++i)if(children[i]!==visual && /BsiChromeFace|BsiTopbar/.test(String(children[i])))children[i].visible=Qt.binding(function(){return !visual.ready})
            }
            onReadyChanged:updateNative()
            Component.onCompleted:updateNative()
        }
    }
}
