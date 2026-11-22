import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id:panel
    objectName:"voidAppearancePanel"
    property var draft:({})
    property string errorText:""
    function loadDraft(){draft=JSON.parse(JSON.stringify(Theme.colors));errorText=""}
    Component.onCompleted:loadDraft()
    onVisibleChanged:if(visible)loadDraft()
    component Heading:ColumnLayout {
        property string title
        property string detail
        spacing:4
        Layout.fillWidth:true
        Label {text:parent.title;color:Theme.text;font.family:Theme.fontFamily;font.pixelSize:15;font.bold:true}
        Label {text:parent.detail;color:Theme.muted;font.family:Theme.fontFamily;font.pixelSize:11;wrapMode:Text.Wrap;Layout.fillWidth:true}
    }
    component Section:Rectangle {
        default property alias entries:body.data
        implicitHeight:body.implicitHeight+32
        Layout.fillWidth:true
        radius:Math.min(Theme.corner,12);color:Theme.surface;border.width:1;border.color:Theme.border
        ColumnLayout {id:body;x:16;y:16;width:parent.width-32;spacing:12}
    }
    component Field:TextField {
        color:Theme.text;selectionColor:Theme.accent;selectedTextColor:Theme.background
        font.family:Theme.fontFamily;font.pixelSize:12
        implicitHeight:36;leftPadding:10;rightPadding:10;selectByMouse:true
        background:Rectangle {radius:7;color:Theme.background;border.width:1;border.color:parent.activeFocus?Theme.accent:Theme.border}
    }
    ColumnLayout {
        anchors.fill:parent;spacing:12
        ScrollView {
            id:scroller;objectName:"voidAppearanceScroll"
            Layout.fillWidth:true;Layout.fillHeight:true;clip:true
            contentWidth:availableWidth
            ScrollBar.vertical.policy:ScrollBar.AsNeeded
            ColumnLayout {
                width:scroller.availableWidth-12;spacing:14
                Label {Layout.fillWidth:true;wrapMode:Text.Wrap;color:Theme.warning;visible:!Theme.persistenceAvailable;text:Theme.persistenceNotice}
                Heading {title:"Choose your atmosphere";detail:"Six dark palettes. Applied instantly across the emulator."}
                ColumnLayout {
                    Layout.fillWidth:true;spacing:8
                    Repeater {
                        model:Theme.presets
                        delegate:AbstractButton {
                            id:card
                            objectName:"themePreset_"+modelData.id
                            Layout.fillWidth:true;implicitHeight:68;padding:12;hoverEnabled:true
                            Accessible.name:modelData.name+" theme"
                            onClicked:{Theme.choose(modelData.id);panel.loadDraft()}
                            background:Rectangle {
                                radius:Math.min(Theme.corner,10)
                                color:Theme.selectedId===modelData.id?Theme.raised:Theme.surface
                                border.width:1;border.color:Theme.selectedId===modelData.id?Theme.accent:card.hovered?Theme.muted:Theme.border
                                Behavior on border.color {enabled:Theme.motionEnabled;ColorAnimation {duration:Theme.duration}}
                            }
                            contentItem:RowLayout {
                                spacing:12
                                Rectangle {
                                    Layout.preferredWidth:66;Layout.preferredHeight:42
                                    radius:6;color:modelData.palette.background;border.width:1;border.color:modelData.palette.border
                                    Rectangle {x:5;y:5;width:12;height:32;radius:3;color:modelData.palette.raised}
                                    Rectangle {x:22;y:8;width:36;height:7;radius:2;color:modelData.palette.accent}
                                    Rectangle {x:22;y:20;width:36;height:6;radius:2;color:modelData.palette.surface;border.width:1;border.color:modelData.palette.border}
                                    Rectangle {x:22;y:30;width:25;height:3;radius:1;color:modelData.palette.muted}
                                }
                                ColumnLayout {
                                    Layout.fillWidth:true;spacing:4
                                    Label {text:modelData.name;color:Theme.text;font.family:Theme.fontFamily;font.pixelSize:12;font.bold:true}
                                    Label {text:modelData.description;color:Theme.muted;font.family:Theme.fontFamily;font.pixelSize:10;Layout.fillWidth:true;elide:Text.ElideRight}
                                }
                                Rectangle {
                                    width:20;height:20;radius:10;color:Theme.selectedId===modelData.id?Theme.accent:"transparent";border.width:1;border.color:Theme.selectedId===modelData.id?Theme.accent:Theme.border
                                    StudioGlyph {anchors.centerIn:parent;width:12;height:12;kind:"check";tint:Theme.background;visible:Theme.selectedId===modelData.id}
                                }
                            }
                        }
                    }
                }
                Section {
                    Heading {title:"Personal colors";detail:"Adjust the palette below, then apply your colors."}
                    Repeater {
                        model:[{key:"background",title:"Background",detail:"Main window and page canvas"},{key:"surface",title:"Surface",detail:"Settings sections and dialogs"},{key:"raised",title:"Raised surface",detail:"Menus, hover and selected states"},{key:"border",title:"Borders",detail:"Subtle outlines and separators"},{key:"text",title:"Primary text",detail:"Titles and important labels"},{key:"muted",title:"Secondary text",detail:"Descriptions and inactive icons"},{key:"accent",title:"Accent",detail:"Selected controls and highlights"}]
                        delegate:ColumnLayout {
                            Layout.fillWidth:true;spacing:5
                            Label {text:modelData.title;color:Theme.text;font.family:Theme.fontFamily;font.pixelSize:12}
                            Label {text:modelData.detail;color:Theme.muted;font.family:Theme.fontFamily;font.pixelSize:10}
                            RowLayout {
                                Layout.fillWidth:true;spacing:10
                                Rectangle {width:32;height:34;radius:7;color:/^#[0-9a-fA-F]{6}$/.test(panel.draft[modelData.key]||"")?panel.draft[modelData.key]:Theme.background;border.width:1;border.color:Theme.border}
                                Field {
                                    objectName:"customColor_"+modelData.key
                                    Layout.fillWidth:true
                                    text:panel.draft[modelData.key]||"";placeholderText:"#000000"
                                    validator:RegularExpressionValidator {regularExpression:/#[0-9a-fA-F]{6}/}
                                    onTextEdited:{var next=Object.assign({},panel.draft);next[modelData.key]=text;panel.draft=next}
                                    Accessible.name:modelData.title+" color"
                                }
                            }
                        }
                    }
                }
                Section {
                    Heading {title:"Type & shape";detail:"Comfortable text and corners that suit your setup."}
                    Label {text:"Interface font";color:Theme.text;font.pixelSize:12}
                    Field {objectName:"themeFont";Layout.fillWidth:true;text:Theme.fontFamily;onEditingFinished:if(!Theme.setFont(text))panel.errorText="Enter an installed font name.";Accessible.name:"Interface font"}
                    Label {text:"Corner radius · "+Theme.corner+" px";color:Theme.text;font.pixelSize:12}
                    Slider {objectName:"themeCorners";Layout.fillWidth:true;from:0;to:24;stepSize:1;value:Theme.corner;onMoved:Theme.setCorner(value);palette.highlight:Theme.accent;palette.button:Theme.accent;Accessible.name:"Corner radius"}
                }
                Section {
                    Heading {title:"Motion";detail:"Subtle transitions, or an instant interface."}
                    Switch {objectName:"themeMotion";text:"Enable interface animations";checked:Theme.motionEnabled;onToggled:Theme.setMotion(checked);palette.windowText:Theme.text;palette.highlight:Theme.accent}
                    Label {text:"Transition duration · "+Theme.duration+" ms";color:Theme.muted;font.pixelSize:11}
                    Slider {objectName:"themeDuration";Layout.fillWidth:true;from:0;to:400;stepSize:10;value:Theme.duration;enabled:Theme.motionEnabled;onMoved:Theme.setDuration(value);palette.highlight:Theme.accent;Accessible.name:"Transition duration"}
                }
                Label {Layout.fillWidth:true;text:'Selected icons by <a href="https://www.magnific.com/author/magnific/icons/special-lineal_7">Magnific / Freepik</a> · Special Lineal';textFormat:Text.RichText;color:Theme.muted;linkColor:Theme.accent;font.pixelSize:10;onLinkActivated:function(link){Qt.openUrlExternally(link)}}
                Label {Layout.fillWidth:true;wrapMode:Text.Wrap;color:Theme.warning;visible:Theme.textContrast<4.5;text:"Low text contrast. Try a brighter primary text color."}
                Label {Layout.fillWidth:true;wrapMode:Text.Wrap;color:Theme.danger;visible:panel.errorText.length>0;text:panel.errorText}
            }
        }
        Rectangle {Layout.fillWidth:true;height:1;color:Theme.border}
        RowLayout {
            Layout.fillWidth:true;spacing:10
            ColumnLayout {
                Layout.fillWidth:true;spacing:3
                Label {text:Theme.name;color:Theme.text;font.pixelSize:12}
                Label {text:"Saved on this PC";color:Theme.muted;font.pixelSize:10;visible:Theme.persistenceAvailable}
            }
            Button {
                objectName:"resetAppearance";text:"Reset";onClicked:{Theme.reset();panel.loadDraft()}
                palette.buttonText:Theme.text
                background:Rectangle {radius:8;color:Theme.surface;border.width:1;border.color:Theme.border}
            }
            Button {
                objectName:"applyCustomTheme";text:"Apply colors";font.bold:true
                onClicked:{if(Theme.applyCustom(panel.draft))panel.errorText="";else panel.errorText="Use six-digit hex values for every color, e.g. #8cc8ff."}
                palette.buttonText:Theme.background
                background:Rectangle {radius:8;color:Theme.accent}
            }
        }
    }
}
