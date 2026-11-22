import QtQuick 2.15

// Event-driven bridge between native windows, even when C++ gives Settings its
// own QQmlEngine. Only a bounded JSON string crosses the QObject boundary.
Item {
    id: relay
    objectName:"voidReferenceThemeRelay"
    visible:false
    property var hostWindow:null
    property var peer:null
    property bool receiving:false
    readonly property string current:Theme.exportState()
    property string state:current
    function publish() {
        if(receiving)return
        if(state!==current)state=current
        if(peer && peer.state!==state)peer.state=state
    }
    function acceptState() {
        if(receiving)return
        if(state!==current) {
            receiving=true
            var ok=Theme.importState(state)
            receiving=false
            if(!ok){state=current;return}
        }
        if(peer && peer.state!==state)peer.state=state
    }
    onCurrentChanged:publish()
    onStateChanged:acceptState()
    function find(node,depth,budget) {
        if(!node || depth>32 || budget.count++>4096)return null
        if(node!==relay && node.objectName==="voidReferenceThemeRelay")return node
        var children=node.children
        if(children)for(var i=0;i<children.length;++i){var found=find(children[i],depth+1,budget);if(found)return found}
        return null
    }
    function attach() {
        peer=null
        if(!hostWindow)return
        var ancestor=hostWindow.transientParent
        for(var i=0;ancestor && i<12;++i,ancestor=ancestor.transientParent) {
            var found=find(ancestor.contentItem,0,{count:0})
            if(found) {
                peer=found
                state=peer.state
                acceptState()
                return
            }
        }
    }
    onHostWindowChanged:attach()
    Component.onCompleted:attach()
    Connections {
        target:relay.hostWindow
        ignoreUnknownSignals:true
        function onTransientParentChanged(){relay.attach()}
        function onVisibleChanged(){if(target && target.visible)relay.attach()}
    }
    Connections {
        target:relay.peer
        ignoreUnknownSignals:true
        function onStateChanged(){if(relay.peer && relay.state!==relay.peer.state)relay.state=relay.peer.state}
    }
}
