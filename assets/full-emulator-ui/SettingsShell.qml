// Native Settings host. Custom visual shell; original page objects/callbacks stay native.
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import "BSISettingsIconsV3.js" as Icons

Item {
    id:shell
    objectName:"bsiSettingsShell"
    property var bsiHost
    property var bsiTheme
    readonly property bool bsiCompact:width<720
    readonly property int bsiRailWidth:bsiCompact?64:width<1100?200:224
    readonly property var nativeTabs:(bsiHost && bsiHost.pSettingsItems)?bsiHost.pSettingsItems.pSettingElements:[]
    readonly property var bsiTabs:tabEntries()
    property bool appearanceOpen:false
    readonly property int nativeIndex:bsiHost?bsiHost.pCurrentTabIndex:0
    readonly property int selectedIndex:appearanceOpen?nativeTabs.length:nativeIndex
    readonly property var currentRow:nativeIndex>=0 && nativeIndex<nativeTabs.length?nativeTabs[nativeIndex]:null
    readonly property string currentKind:appearanceOpen?"palette":currentRow?iconForEntry(currentRow):"preferences"
    readonly property var nativePage:currentRow && currentRow.pComponentObj?currentRow.pComponentObj:null
    readonly property bool showInspector:!appearanceOpen && width>=1260 && currentKind==="performance" && nativePage && nativePage.pCompLoaded===true && nativePage.pVoidFps!==undefined
    property bool motionEnabled:Theme.motionEnabled
    property alias pageContainer:bsiPageBody
    function stylePage(page){nativeStyle.prepare(page)}
    function tabEntries(){
        var result=[]
        for(var i=0;i<nativeTabs.length;++i)result.push({entry:nativeTabs[i],nativeIndex:i,appearance:false})
        result.push({entry:{pButtonId:"appearance",pButtonText:"Appearance",pShowRedDot:false},nativeIndex:-1,appearance:true})
        return result
    }
    function iconForEntry(entry){return Icons.kindForTab(entry.pButtonId,entry.pQmlSource||"",entry.pButtonText)}
    function openNative(index){
        if(bsiHost)bsiHost.fVoidSelectNative(index)
        // Native code alone accepts/rejects a tab change. Returning from Appearance
        // reveals whichever page the native guard actually selected.
        appearanceOpen=false
    }
    function intro(){
        var descriptions={
            performance:["More power. Your way.","Fine-tune your resources for a smoother, faster gaming experience."],
            display:["A clearer point of view.","Shape your display with the original resolution and scaling controls."],
            graphics:["Every detail, considered.","Find the rendering setup that works for your games and hardware."],
            palette:["Make this space yours.","Six distinct dark worlds. Your colors, corners and motion — across the native interface."],
            shortcuts:["Less reaching. More playing.","Keep your native shortcuts close and your workflow uninterrupted."],
            devices:["Connected to your play.","Manage your native audio and device preferences in one place."],
            advanced:["The finer controls.","Your original advanced settings, with a more focused workspace."]
        }
        return descriptions[currentKind]||[currentRow?currentRow.pButtonText:"Your workspace.","The native controls you know, in a calmer, more considered interface."]
    }
    function fitHost(){
        var host=bsiHost
        if(!host || !host.screen)return
        var area={x:host.screen.virtualX,y:host.screen.virtualY,width:host.screen.desktopAvailableWidth,height:host.screen.desktopAvailableHeight}
        if(!(area.width>0 && area.height>0))return
        var w=Math.min(1440,area.width-48),h=Math.min(960,area.height-64)
        if(w>host.width)host.width=w
        if(h>host.height)host.height=h
        host.x=Math.max(area.x,Math.min(host.x,area.x+area.width-host.width))
        host.y=Math.max(area.y,Math.min(host.y,area.y+area.height-host.height))
    }
    onBsiHostChanged:{SurfaceRegistry.attach(shell,bsiHost);if(bsiHost)fitHost()}
    Component.onCompleted:{fitHost();SurfaceRegistry.attach(shell,bsiHost)}
    Component.onDestruction:SurfaceRegistry.detach(shell)
    Connections {
        target:shell.bsiHost
        ignoreUnknownSignals:true
        function onVisibleChanged(){if(target && target.visible){shell.fitHost();Theme.sync();nativeStyle.prepare(bsiPageBody)}}
        function onPCurrentTabIndexChanged(){shell.appearanceOpen=false}
    }
    ThemeRelay {hostWindow:shell.bsiHost}
    component OutlineIcon:ThemeIcon {
        property string kind:"preferences"
        source:Icons.pngUrl(kind)
        tint:Theme.text
        sourceSize:Qt.size(48,48)
    }
    Rectangle {anchors.fill:parent;color:Theme.background;radius:Theme.corner;border.width:1;border.color:Theme.border}
    Item {
        id:header
        anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top
        height:80
        MouseArea {anchors.fill:parent;anchors.rightMargin:72;onPressed:if(shell.bsiHost)shell.bsiHost.startSystemMove()}
        Rectangle {
            x:24;anchors.verticalCenter:parent.verticalCenter;width:34;height:34;radius:11;color:Theme.accent
            Text {anchors.centerIn:parent;text:"ϟ";color:Theme.background;font.family:"Segoe UI Symbol";font.pixelSize:28}
        }
        Text {x:72;anchors.verticalCenter:parent.verticalCenter;text:"𝓥𝓞𝓘𝓓";color:Theme.text;font.family:Theme.fontFamily;font.pixelSize:25;font.bold:true}
        Text {x:162;anchors.verticalCenter:parent.verticalCenter;text:"EMULATOR STUDIO";visible:!shell.bsiCompact;color:Theme.muted;font.family:Theme.fontFamily;font.pixelSize:9;font.letterSpacing:2}
        Text {objectName:"voidFullUiVersion";anchors.right:closeButton.left;anchors.rightMargin:26;anchors.verticalCenter:parent.verticalCenter;text:"OLED STUDIO 11";color:Theme.muted;font.family:Theme.fontFamily;font.pixelSize:10;font.letterSpacing:1.3}
        ToolButton {
            id:closeButton;objectName:"bsiSettingsClose"
            anchors.right:parent.right;anchors.rightMargin:24;anchors.verticalCenter:parent.verticalCenter
            width:32;height:32;padding:7;hoverEnabled:true
            Accessible.name:qsTranslate("QObject","Close")
            contentItem:OutlineIcon {kind:"close";tint:closeButton.hovered?Theme.text:Theme.muted}
            background:Rectangle {radius:9;color:closeButton.hovered?Theme.raised:"transparent";Behavior on color {enabled:Theme.motionEnabled;ColorAnimation {duration:Theme.duration}}}
            onClicked:if(shell.bsiHost)shell.bsiHost.close()
        }
        Rectangle {anchors.left:parent.left;anchors.right:parent.right;anchors.bottom:parent.bottom;height:1;color:Theme.border}
    }
    Item {
        id:rail
        x:16;width:shell.bsiRailWidth
        anchors.top:header.bottom;anchors.bottom:parent.bottom;anchors.topMargin:24;anchors.bottomMargin:20
        Text {x:12;text:shell.bsiCompact?"UI":"WORKSPACE";color:Theme.muted;font.family:Theme.fontFamily;font.pixelSize:9;font.letterSpacing:2}
        ListView {
            id:navigation;objectName:"bsiSettingsNavigation"
            anchors.fill:parent;anchors.topMargin:34;anchors.bottomMargin:shell.bsiCompact?40:108
            model:shell.bsiTabs;currentIndex:shell.selectedIndex;clip:true;spacing:5
            boundsBehavior:Flickable.StopAtBounds
            onCurrentIndexChanged:if(currentIndex>=0)positionViewAtIndex(currentIndex,ListView.Contain)
            ScrollBar.vertical:ScrollBar {policy:ScrollBar.AsNeeded;width:3}
            delegate:ItemDelegate {
                id:tab
                objectName:model.modelData.appearance?"bsiSettingsAppearance":"bsiSettingsTab_"+model.modelData.nativeIndex
                property var row:model.modelData
                readonly property bool selected:index===shell.selectedIndex
                width:navigation.width;height:43;hoverEnabled:true;padding:0
                scale:down?0.98:1
                Behavior on scale {enabled:Theme.motionEnabled;NumberAnimation {duration:Theme.duration;easing.type:Easing.OutCubic}}
                Accessible.name:row.entry.pButtonText
                contentItem:Item {
                    OutlineIcon {id:tabIcon;objectName:"voidSettingsNavIcon";x:16;anchors.verticalCenter:parent.verticalCenter;width:19;height:19;kind:tab.row.appearance?"palette":shell.iconForEntry(tab.row.entry);tint:tab.selected||tab.hovered?Theme.accent:Theme.muted}
                    Text {anchors.left:tabIcon.right;anchors.leftMargin:14;anchors.right:parent.right;anchors.rightMargin:10;anchors.verticalCenter:parent.verticalCenter;visible:!shell.bsiCompact;text:tab.row.entry.pButtonText;elide:Text.ElideRight;color:tab.selected?Theme.text:Theme.muted;font.family:Theme.fontFamily;font.pixelSize:13;font.weight:tab.selected?Font.DemiBold:Font.Normal}
                }
                background:Rectangle {
                    radius:10;color:tab.selected?Theme.accent_transparent:tab.hovered?Theme.surface:"transparent"
                    border.width:tab.activeFocus?1:0;border.color:Theme.accent
                    Behavior on color {enabled:Theme.motionEnabled;ColorAnimation {duration:Theme.duration}}
                    Rectangle {x:0;anchors.verticalCenter:parent.verticalCenter;width:3;height:20;radius:1;color:Theme.accent;visible:tab.selected}
                }
                onClicked:if(row.appearance)shell.appearanceOpen=true;else shell.openNative(row.nativeIndex)
                ToolTip.visible:hovered && shell.bsiCompact;ToolTip.text:row.entry.pButtonText
            }
        }
        Rectangle {
            visible:!shell.bsiCompact
            anchors.left:parent.left;anchors.right:parent.right;anchors.bottom:parent.bottom;height:82;radius:12;color:Theme.surface
            Text {x:14;y:14;text:"Designed around you.";color:Theme.text;font.family:Theme.fontFamily;font.pixelSize:12}
            Text {x:14;y:35;text:Theme.name;elide:Text.ElideRight;width:parent.width-28;color:Theme.accent;font.family:Theme.fontFamily;font.pixelSize:11}
            Text {x:14;y:57;text:"Developed By NOVA XIS";color:Theme.muted;font.family:Theme.fontFamily;font.pixelSize:9}
        }
    }
    Item {
        id:workspace
        anchors.left:rail.right;anchors.leftMargin:32;anchors.right:parent.right;anchors.rightMargin:24
        anchors.top:header.bottom;anchors.topMargin:28;anchors.bottom:parent.bottom;anchors.bottomMargin:24
        Item {
            id:hero
            anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;height:112
            Text {text:"SETTINGS / "+(shell.appearanceOpen?"APPEARANCE":shell.currentRow?shell.currentRow.pButtonText.toUpperCase():"WORKSPACE");color:Theme.muted;font.family:Theme.fontFamily;font.pixelSize:9;font.letterSpacing:1.8}
            Text {y:24;width:parent.width;text:shell.intro()[0];color:Theme.text;font.family:Theme.fontFamily;font.pixelSize:shell.bsiCompact?22:28;font.weight:Font.DemiBold;elide:Text.ElideRight}
            Text {y:66;width:parent.width;text:shell.intro()[1];color:Theme.muted;font.family:Theme.fontFamily;font.pixelSize:12;wrapMode:Text.Wrap}
        }
        // Original StackLayout/pages are reparented here by the reviewed bridge.
        // Hiding for Appearance keeps unsaved native values alive; nothing is cloned.
        Item {
            id:bsiPageBody;objectName:"bsiSettingsPageContainer"
            anchors.left:parent.left;anchors.right:inspector.visible?inspector.left:parent.right;anchors.rightMargin:inspector.visible?20:0
            anchors.top:hero.bottom;anchors.bottom:parent.bottom
            visible:!shell.appearanceOpen
            NumberAnimation on opacity {id:pageReveal;from:0.4;to:1;duration:Theme.duration;easing.type:Easing.OutCubic;running:false}
        }
        Connections {target:shell.bsiHost;ignoreUnknownSignals:true;function onPCurrentTabIndexChanged(){if(Theme.motionEnabled)pageReveal.restart()}}
        AppearancePanel {id:appearance;anchors.left:parent.left;anchors.right:parent.right;anchors.top:hero.bottom;anchors.bottom:parent.bottom;visible:shell.appearanceOpen}
        Rectangle {
            id:inspector;objectName:"voidNativeResourceSummary"
            visible:shell.showInspector;anchors.right:parent.right;anchors.top:hero.bottom;width:260;height:350
            color:Theme.surface;radius:Theme.corner;border.width:1;border.color:Theme.border
            Column {
                anchors.fill:parent;anchors.margins:22;spacing:18
                Text {text:"YOUR SETUP";color:Theme.muted;font.pixelSize:10;font.letterSpacing:2}
                Text {width:parent.width;text:"Built for your\nnext high score.";color:Theme.text;font.family:Theme.fontFamily;font.pixelSize:23;font.bold:true}
                Rectangle {width:parent.width;height:1;color:Theme.border}
                Text {text:"CPU cores   "+(shell.showInspector?shell.nativePage.pCurrentCpu+" / "+shell.nativePage.pMaxCpu:"—");color:Theme.muted;font.pixelSize:12}
                Rectangle {width:parent.width;height:5;radius:2;color:Theme.border;Rectangle {width:parent.width*(shell.showInspector?Math.max(0,Math.min(1,shell.nativePage.pCurrentCpu/Math.max(1,shell.nativePage.pMaxCpu))):0);height:parent.height;radius:2;color:Theme.accent;Behavior on width {enabled:Theme.motionEnabled;NumberAnimation {duration:Theme.duration}}}}
                Text {text:"Memory   "+(shell.showInspector?shell.nativePage.pVoidRam+" MB":"—");color:Theme.text;font.pixelSize:13}
                Text {text:shell.showInspector?Math.round(shell.nativePage.pVoidFps)+" FPS selected":"—";color:Theme.accent;font.pixelSize:20;font.bold:true}
                Text {width:parent.width;text:"Current selections. Apply them with the native Save changes button.";wrapMode:Text.Wrap;color:Theme.muted;font.pixelSize:11}
            }
        }
    }
    NativeControlStyle {id:nativeStyle;objectName:"voidNativeControlStyle";stylePanels:true;pages:bsiPageBody;visible:false}
}
