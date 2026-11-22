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
    readonly property bool bsiCompact:width<760
    readonly property int bsiRailWidth:bsiCompact?52:180
    readonly property var nativeTabs:(bsiHost && bsiHost.pSettingsItems)?bsiHost.pSettingsItems.pSettingElements:[]
    readonly property var bsiTabs:tabEntries()
    property bool appearanceOpen:false
    readonly property int nativeIndex:bsiHost?bsiHost.pCurrentTabIndex:0
    readonly property int appearanceIndex:Math.min(3,nativeTabs.length)
    readonly property int selectedIndex:appearanceOpen?appearanceIndex:nativeIndex+(nativeIndex>=appearanceIndex?1:0)
    readonly property var currentRow:nativeIndex>=0 && nativeIndex<nativeTabs.length?nativeTabs[nativeIndex]:null
    readonly property string currentKind:appearanceOpen?"palette":currentRow?iconForEntry(currentRow):"preferences"
    readonly property var nativePage:currentRow && currentRow.pComponentObj?currentRow.pComponentObj:null
    readonly property bool showInspector:!appearanceOpen && width>=880 && currentKind==="performance" && nativePage && nativePage.pCompLoaded===true && nativePage.pVoidFps!==undefined
    property bool motionEnabled:Theme.motionEnabled
    property alias pageContainer:bsiPageBody
    function stylePage(page){nativeStyle.prepare(page)}
    function tabEntries(){
        var result=[]
        for(var i=0;i<nativeTabs.length;++i)result.push({entry:nativeTabs[i],nativeIndex:i,appearance:false})
        // Keep Appearance discoverable above the fold, even with every OEM tab.
        result.splice(appearanceIndex,0,{entry:{pButtonId:"appearance",pButtonText:"Appearance",pShowRedDot:false},nativeIndex:-1,appearance:true})
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
        return appearanceOpen?["Appearance","Colors, shape & motion"]:
            [currentRow?currentRow.pButtonText:"Settings", currentKind==="performance"?"Fine-tune resources for a smoother experience.":"Your preferences, carefully organized."]
    }
    function fitHost(){
        var host=bsiHost
        if(!host || !host.screen)return
        var area={x:host.screen.virtualX,y:host.screen.virtualY,width:host.screen.desktopAvailableWidth,height:host.screen.desktopAvailableHeight}
        if(!(area.width>0 && area.height>0))return
        // A compact utility window, never a desktop-filling dashboard.
        host.width=Math.max(320,Math.min(1000,area.width-32))
        host.height=Math.max(320,Math.min(700,area.height-32))
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
    component OutlineIcon:StudioGlyph {tint:Theme.text}
    Rectangle {anchors.fill:parent;color:Theme.background;radius:Theme.corner;border.width:1;border.color:Theme.border}
    Item {
        id:header
        anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top
        height:52
        MouseArea {anchors.fill:parent;anchors.rightMargin:56;onPressed:if(shell.bsiHost)shell.bsiHost.startSystemMove()}
        BrandLogo {x:14;anchors.verticalCenter:parent.verticalCenter;width:31;height:27}
        Text {x:51;anchors.verticalCenter:parent.verticalCenter;text:"VOID";color:Theme.text;font.family:Theme.fontFamily;font.pixelSize:21;font.bold:true}
        Text {x:142;anchors.verticalCenter:parent.verticalCenter;text:"Settings";visible:!shell.bsiCompact;color:Theme.muted;font.family:Theme.fontFamily;font.pixelSize:12}
        Text {objectName:"voidFullUiVersion";visible:false;anchors.right:closeButton.left;anchors.rightMargin:26;anchors.verticalCenter:parent.verticalCenter;text:"NATIVE SETTINGS";color:Theme.muted;font.family:Theme.fontFamily;font.pixelSize:10;font.letterSpacing:1.3}
        Rectangle {x:124;anchors.verticalCenter:parent.verticalCenter;width:1;height:21;color:Theme.border;visible:!shell.bsiCompact}
        ToolButton {
            id:closeButton;objectName:"bsiSettingsClose"
            anchors.right:parent.right;anchors.rightMargin:12;anchors.verticalCenter:parent.verticalCenter
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
        x:12;width:shell.bsiRailWidth
        anchors.top:header.bottom;anchors.bottom:parent.bottom;anchors.topMargin:16;anchors.bottomMargin:12
        Row {
            visible:!shell.bsiCompact;x:12;y:2;spacing:10
            BrandLogo {width:34;height:29}
            Text {text:"VOID";color:Theme.text;font.family:Theme.fontFamily;font.pixelSize:21;font.bold:true;anchors.verticalCenter:parent.verticalCenter}
        }
        Text {visible:!shell.bsiCompact;x:12;y:36;text:"Modern · Fast · Minimal";color:Theme.muted;font.family:Theme.fontFamily;font.pixelSize:9}
        Text {x:12;y:shell.bsiCompact?0:62;text:shell.bsiCompact?"UI":"PREFERENCES";color:Theme.muted;font.family:Theme.fontFamily;font.pixelSize:9;font.letterSpacing:1.5}
        ListView {
            id:navigation;objectName:"bsiSettingsNavigation"
            anchors.fill:parent;anchors.topMargin:shell.bsiCompact?24:84;anchors.bottomMargin:shell.bsiCompact?20:86
            model:shell.bsiTabs;currentIndex:shell.selectedIndex;clip:true;spacing:3
            boundsBehavior:Flickable.StopAtBounds
            onCurrentIndexChanged:if(currentIndex>=0)positionViewAtIndex(currentIndex,ListView.Contain)
            ScrollBar.vertical:ScrollBar {policy:ScrollBar.AsNeeded;width:3}
            delegate:ItemDelegate {
                id:tab
                objectName:model.modelData.appearance?"bsiSettingsAppearance":"bsiSettingsTab_"+model.modelData.nativeIndex
                property var row:model.modelData
                readonly property bool selected:index===shell.selectedIndex
                width:navigation.width;height:28;hoverEnabled:true;padding:0
                scale:down?0.98:1
                Behavior on scale {enabled:Theme.motionEnabled;NumberAnimation {duration:Theme.duration;easing.type:Easing.OutCubic}}
                Accessible.name:row.entry.pButtonText
                contentItem:Item {
                    OutlineIcon {id:tabIcon;objectName:"voidSettingsNavIcon";x:12;anchors.verticalCenter:parent.verticalCenter;width:20;height:20;kind:tab.row.appearance?"palette":shell.iconForEntry(tab.row.entry);tint:tab.selected||tab.hovered?Theme.accent:Theme.muted}
                    Text {anchors.left:tabIcon.right;anchors.leftMargin:10;anchors.right:parent.right;anchors.rightMargin:10;anchors.verticalCenter:parent.verticalCenter;visible:!shell.bsiCompact;text:tab.row.entry.pButtonText;elide:Text.ElideRight;color:tab.selected?Theme.text:Theme.muted;font.family:Theme.fontFamily;font.pixelSize:12;font.weight:tab.selected?Font.DemiBold:Font.Normal}
                }
                background:Rectangle {
                    radius:8;color:tab.selected?Qt.rgba(Theme.accent.r,Theme.accent.g,Theme.accent.b,0.18):tab.hovered?Theme.surface:"transparent"
                    border.width:tab.selected||tab.activeFocus?1:0;border.color:Qt.rgba(Theme.accent.r,Theme.accent.g,Theme.accent.b,0.4)
                    Behavior on color {enabled:Theme.motionEnabled;ColorAnimation {duration:Theme.duration}}

                }
                onClicked:if(row.appearance)shell.appearanceOpen=true;else shell.openNative(row.nativeIndex)
                ToolTip.visible:hovered && shell.bsiCompact;ToolTip.text:row.entry.pButtonText
            }
        }
        Column {
            visible:!shell.bsiCompact
            anchors.left:parent.left;anchors.leftMargin:12;anchors.right:parent.right;anchors.bottom:parent.bottom;spacing:7
            Row {spacing:6
                Rectangle {width:5;height:5;radius:3;color:Theme.accent;anchors.verticalCenter:parent.verticalCenter}
                Text {text:"Theme";color:Theme.muted;font.family:Theme.fontFamily;font.pixelSize:10}
            }
            ComboBox {
                id:sidebarTheme;objectName:"voidSidebarThemePicker";width:shell.bsiRailWidth-24;height:26;padding:8;rightPadding:26
                Accessible.name:"Emulator theme"
                model:Theme.presets;textRole:"name";valueRole:"id"
                displayText:Theme.name
                currentIndex:{for(var i=0;i<Theme.presets.length;++i)if(Theme.presets[i].id===Theme.selectedId)return i;return -1}
                onActivated:Theme.choose(currentValue)
                contentItem:Text {text:sidebarTheme.displayText;color:Theme.text;font.family:Theme.fontFamily;font.pixelSize:10;verticalAlignment:Text.AlignVCenter;elide:Text.ElideRight}
                indicator:StudioGlyph {x:sidebarTheme.width-21;y:7;width:12;height:12;kind:"chevrondown";tint:Theme.muted}
                background:Rectangle {color:Theme.background;radius:10;border.width:1;border.color:Theme.border}
                delegate:ItemDelegate {
                    width:sidebarTheme.width-12;height:29
                    contentItem:Text {text:modelData.name;color:Theme.text;font.family:Theme.fontFamily;font.pixelSize:10;verticalAlignment:Text.AlignVCenter}
                    background:Rectangle {radius:6;color:highlighted?Theme.raised:Theme.surface}
                    highlighted:sidebarTheme.highlightedIndex===index
                }
                popup:Popup {
                    y:-implicitHeight-4;width:sidebarTheme.width;padding:6
                    implicitHeight:contentItem.implicitHeight+12
                    contentItem:ListView {clip:true;implicitHeight:contentHeight;model:sidebarTheme.popup.visible?sidebarTheme.delegateModel:null;currentIndex:sidebarTheme.highlightedIndex}
                    background:Rectangle {radius:10;color:Theme.surface;border.width:1;border.color:Theme.border}
                }
            }
            Text {text:"Developed By NOVA XIS";color:Theme.muted;font.family:Theme.fontFamily;font.pixelSize:8}
        }
        Rectangle {anchors.right:parent.right;anchors.rightMargin:-8;anchors.top:parent.top;anchors.bottom:parent.bottom;width:1;color:Theme.border;opacity:0.55}
    }

    Item {
        id:workspace
        anchors.left:rail.right;anchors.leftMargin:16;anchors.right:parent.right;anchors.rightMargin:16
        anchors.top:header.bottom;anchors.topMargin:16;anchors.bottom:parent.bottom;anchors.bottomMargin:16
        Item {
            id:hero
            anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;height:shell.showInspector?108:66
            StudioGlyph {x:0;y:1;width:32;height:32;kind:shell.currentKind==="performance"?"gauge":shell.currentKind;tint:Theme.accent}
            Text {x:45;y:0;width:parent.width-45;text:shell.intro()[0];color:Theme.text;font.family:Theme.fontFamily;font.pixelSize:23;font.weight:Font.DemiBold;elide:Text.ElideRight}
            Text {x:45;y:32;width:parent.width-45;text:shell.intro()[1];color:Theme.muted;font.family:Theme.fontFamily;font.pixelSize:11;elide:Text.ElideRight}
            Rectangle {
                id:inspector;objectName:"voidNativeResourceSummary";visible:shell.showInspector
                anchors.left:parent.left;anchors.right:parent.right;y:56;height:40
                color:Theme.surface;radius:Math.min(12,Theme.corner);border.width:1;border.color:Theme.border
                Row {
                    anchors.fill:parent
                    Repeater {
                        model:shell.showInspector?[
                            {icon:"cpu",value:shell.nativePage.pCurrentCpu+" cores"},
                            {icon:"memory",value:shell.nativePage.pVoidRam+" MB RAM"},
                            {icon:"framerate",value:Math.round(shell.nativePage.pVoidFps)+" FPS selected"}
                        ]:[]
                        delegate:Item {
                            width:inspector.width/3;height:inspector.height
                            Row {anchors.centerIn:parent;spacing:9
                                StudioGlyph {width:18;height:18;kind:modelData.icon;tint:Theme.accent}
                                Text {text:modelData.value;color:Theme.text;font.family:Theme.fontFamily;font.pixelSize:11;anchors.verticalCenter:parent.verticalCenter}
                            }
                            Rectangle {visible:index<2;anchors.right:parent.right;anchors.verticalCenter:parent.verticalCenter;width:1;height:18;color:Theme.border}
                        }
                    }
                }
            }
        }
        // Original StackLayout/pages are reparented here by the reviewed bridge.
        // Hiding for Appearance keeps unsaved native values alive; nothing is cloned.
        Item {
            id:bsiPageBody;objectName:"bsiSettingsPageContainer"
            anchors.left:parent.left;anchors.right:parent.right
            anchors.top:hero.bottom;anchors.bottom:parent.bottom
            visible:!shell.appearanceOpen
            NumberAnimation on opacity {id:pageReveal;from:0.4;to:1;duration:Theme.duration;easing.type:Easing.OutCubic;running:false}
        }
        Connections {target:shell.bsiHost;ignoreUnknownSignals:true;function onPCurrentTabIndexChanged(){if(Theme.motionEnabled)pageReveal.restart()}}
        AppearancePanel {id:appearance;anchors.left:parent.left;anchors.right:parent.right;anchors.top:hero.bottom;anchors.bottom:parent.bottom;visible:shell.appearanceOpen}
    }

    NativeControlStyle {id:nativeStyle;objectName:"voidNativeControlStyle";stylePanels:true;pages:bsiPageBody;visible:false}
}
