"""Qt component/input tests with native-page DOUBLES, never a live-player certification.
Requires PySide6 6.8.3 and normal Qt platform dependencies. No helper is launched.
Optional --screenshot PATH writes a clearly labelled fixture capture, not a product preview.
"""
import argparse
import os
from pathlib import Path
import tempfile
import time
os.environ.setdefault('QT_QPA_PLATFORM','offscreen')
os.environ.setdefault('QT_QUICK_BACKEND','software')
os.environ.setdefault('QT_QUICK_CONTROLS_STYLE','Basic')
if os.name == 'nt':
    os.environ.setdefault('QT_QPA_FONTDIR',str(Path(os.environ['WINDIR'])/'Fonts'))
from PySide6.QtCore import QObject, QPoint, QPointF, Qt, QUrl
from PySide6.QtGui import QGuiApplication, QFontDatabase, QFont, QRawFont
from PySide6.QtQml import QQmlEngine, QQmlComponent
from PySide6.QtQuick import QQuickWindow
from PySide6.QtTest import QTest

ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser();parser.add_argument('--screenshot',type=Path,default=ROOT/'.cache/full-ui/suite07-fixture.png');args=parser.parse_args()
app=QGuiApplication([])
if os.name == 'nt':
    for name in ('segoeui.ttf','seguisb.ttf','seguisym.ttf','cambria.ttc'):
        font=Path(os.environ['WINDIR'])/'Fonts'/name
        if font.is_file():assert QFontDatabase.addApplicationFont(str(font))>=0
    assert QRawFont.fromFont(QFont('Segoe UI')).supportsCharacter(ord('A'))
engine=QQmlEngine();warnings=[]
engine.warnings.connect(lambda errors:warnings.extend(e.toString() for e in errors if "Settings type from Qt.labs.settings is deprecated" not in e.toString()))
url=QUrl.fromLocalFile(str(ROOT/'assets/full-emulator-ui')).toString()

def settle():
    for _ in range(30):app.processEvents();time.sleep(.01)

def visual(item,name):
    if item.objectName()==name:return item
    for child in item.childItems():
        result=visual(child,name)
        if result is not None:return result
    return None

def click(item):
    assert item is not None
    p=item.mapToScene(QPointF(item.width()/2,item.height()/2))
    QTest.mouseClick(window,Qt.LeftButton,Qt.NoModifier,QPoint(round(p.x()),round(p.y())))
    settle()

with tempfile.TemporaryDirectory() as temporary:
    temp=Path(temporary)
    (temp/'UiImageButton.qml').write_text('''import QtQuick 2.15
import QtQuick.Controls 2.15
Button { property string asset:"SidebarEcoMode"; property string pVoidElementId:"settings"; property bool pImageAnimationRunning:false; width:40; height:40 }
''')
    for name in ['UiSlider.qml','UiRowLayout.qml','UiColumnLayout.qml']:
        (temp/name).write_bytes((ROOT/'reference/full-emulator-ui/controls'/name).read_bytes())
    source='''import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15
import "ASSETS" as Skin
Window {
 id:host; width:940; height:540; visible:true
 property int pCurrentTabIndex:0
 property bool pVoidOptimizerActive:false
 property bool blockSwitch:false
 property int requested:-1
 property bool closeRequested:false
 property int saved:0
 property string requestedTheme:"oled"
 function chooseRequestedTheme(){Skin.Theme.choose(requestedTheme)}
 property string themeName:Skin.Theme.name
 property color themeSurface:Skin.Theme.surface
 function prepareAppearance(){Skin.Theme.preferencesFile="PREFERENCES";Skin.Theme.reset()}
 function forestAppearance(){Skin.Theme.choose("forest")}
 function customAppearance(){Skin.Theme.applyCustom({background:"#010203",surface:"#101820",raised:"#202830",border:"#405060",text:"#f0f0f0",muted:"#a0b0c0",accent:"#70ddb0"});Skin.Theme.setCorner(21);Skin.Theme.setMotion(false)}
 function resetAppearance(){Skin.Theme.reset()}

 QtObject { id: initialSliderSize
   property int handleWidth:20; property int handleHeight:20; property int sliderHeight:4
   property font selectedValueFont:Qt.font({pixelSize:16})
   property font recommendedValueFont:Qt.font({pixelSize:14})
   property font rangeValueFont:Qt.font({pixelSize:14})
 }
 QtObject { id: initialSliderColor
   property color handleBackground:"purple";property color handleBackgroundDisabled:"grey";property color handleBorder:"purple"
   property color sliderBackground:"grey";property color sliderBackgroundDisabled:"grey";property color sliderFill:"purple";property color sliderFillDisabled:"grey"
   property color selectedValue:"white";property color recommendedValue:"white";property color rangeValue:"white"
 }
 property var pKebabMenuWin:null
 property QtObject pSettingsItems: QtObject { property list<QtObject> pSettingElements:[
 QtObject{property string pButtonId:"engine";property string pButtonText:"Performance";property bool pShowRedDot:false},
 QtObject{property string pButtonId:"display";property string pButtonText:"Display";property bool pShowRedDot:false},
 QtObject{property string pButtonId:"graphics";property string pButtonText:"Graphics";property bool pShowRedDot:false}
 ] }
 function fVoidSelectOptimizer(){requested=-2;if(!blockSwitch)pVoidOptimizerActive=true}
 function fVoidSelectNative(index){requested=index;if(!blockSwitch){pVoidOptimizerActive=false;pCurrentTabIndex=index}}
 onClosing:function(event){event.accepted=false;closeRequested=true}
 Skin.SettingsShell {id:shell; anchors.fill:parent; bsiHost:host
   Component.onCompleted: {var page=pageFactory.createObject(shell.pageContainer);shell.stylePage(page)}
 }
 Component { id:pageFactory
   Item { width:parent.width; height:parent.height; objectName:"testNativePage"
     Column {spacing:15
       Text{text:"Native input fixture — NOT a live OEM screenshot";color:"#aaaabb"}
       ComboBox{property bool testPopupOpen:popup.opened;objectName:"testCombo";width:174;height:27;model:["High (4 Cores)","Medium (2 Cores)"]}
       UiSlider{objectName:"testSlider";width:460;from:1;to:240;value:120;pSize:initialSliderSize;pColor:initialSliderColor;pSelectedValueText:"Frame rate: "+Math.round(value);pRangeValueVisibility:true}
       Switch{objectName:"testSwitch";text:"Native switch"}
       CheckBox{objectName:"testCheck";text:"Native checkbox"}
       TextField{property color testInk:palette.text;objectName:"testField";width:174;placeholderText:"Native field"}
       Button{property color testInk:palette.buttonText;objectName:"testSave";width:112;height:29;text:"Save changes";onClicked:host.saved++}
     }
   }
 }
 // Separate passive-sidebar fixture, initially empty to exercise late delegate discovery.
 Item{id:sidebar;objectName:"testSidebar";x:880;y:180;width:40;height:100;property var pKebabMenuWin:null}
 Skin.SidebarSkin{id:sidebarSkin;host:sidebar}
 Component{id:lateButton;UiImageButton{objectName:"testLateSidebar"}}
 function addLateSidebarButton(){lateButton.createObject(sidebar)}
 Text{z:1000;anchors.bottom:parent.bottom;anchors.horizontalCenter:parent.horizontalCenter;text:"Qt component test — native-page doubles; NOT live MSI";color:"#aaaaaa";font.pixelSize:9}
}'''.replace('ASSETS',url).replace('PREFERENCES',str(temp/'appearance.ini').replace('\\','/'))
    (temp/'Window.qml').write_text(source)
    component=QQmlComponent(engine,QUrl.fromLocalFile(str(temp/'Window.qml')))
    assert not component.isError(),'\n'.join(e.toString() for e in component.errors())
    window=component.create();assert window,'\n'.join(e.toString() for e in component.errors())
    from PySide6.QtCore import QMetaObject
    QMetaObject.invokeMethod(window,'prepareAppearance');settle()
    assert isinstance(window,QQuickWindow),type(window)
    root=window.contentItem();shell=visual(root,'bsiSettingsShell');page=visual(root,'testNativePage')
    assert page is not None
    assert window.width()<=1000 and window.height()<=700,"Settings inflated beyond compact bounds"
    p=page.mapToScene(QPointF(0,0));assert abs(p.x()-(12+shell.property("bsiRailWidth")+16))<1 and abs(p.y()-134)<1,(p.x(),p.y())
    click(visual(root,'testSave'));assert window.property('saved')==1
    switch=visual(root,'testSwitch');click(switch);assert switch.property('checked')
    check=visual(root,'testCheck');click(check);assert check.property('checked')
    slider=visual(root,'testSlider')
    assert slider.property('pSize').property('handleWidth')==14
    assert slider.property('contentItem').property('spacing')==12
    assert visual(slider,'voidReferenceVisualTrack') is None, 'A second track must not overlay native UiSlider captions'
    before=slider.property('value');click(slider);assert slider.property('value')!=before
    combo=visual(root,'testCombo');click(combo);assert combo.property('testPopupOpen')
    QTest.keyClick(window,Qt.Key_Escape);settle()
    field=visual(root,'testField');click(field);[QTest.keyClick(window,key) for key in (Qt.Key_N,Qt.Key_A,Qt.Key_T,Qt.Key_I,Qt.Key_V,Qt.Key_E)];assert field.property('text')=='native'
    style=visual(root,'voidNativeControlStyle');assert style.property('styledControls')>=6
    # No extra optimizer tab/page or shortcut. Native guard still owns switching.
    assert visual(root,'bsiSettingsTab_3') is None
    assert visual(root,'voidSettingsOptimizerLoader') is None
    window.setProperty('blockSwitch',True);click(visual(root,'bsiSettingsTab_1'))
    assert window.property('requested')==1 and window.property('pCurrentTabIndex')==0
    window.setProperty('blockSwitch',False);click(visual(root,'bsiSettingsTab_1'))
    assert window.property('pCurrentTabIndex')==1
    from PySide6.QtCore import QMetaObject
    QMetaObject.invokeMethod(window,'addLateSidebarButton');settle()
    sidebar=visual(root,'testLateSidebar');face=visual(sidebar,'voidReferenceSidebarFace')
    assert face is not None, 'late sidebar face missing'
    assert face.property('iconKey')=='advanced', face.property('iconKey')
    assert face.property('known'), str(warnings)
    assert sidebar.property('contentItem').property('opacity')==0
    import json
    for preset in json.loads((ROOT/'assets/full-emulator-ui/themes.json').read_text()):
        window.setProperty('requestedTheme',preset['id'])
        QMetaObject.invokeMethod(window,'chooseRequestedTheme');settle()
        assert window.property('themeName')==preset['name']
        assert window.property('themeSurface').name()==preset['palette']['surface']
        assert slider.property('pColor').property('sliderFill').name()==preset['palette']['accent']
    # Sidebar colors must follow every palette without relying on hover/recreation.
        selected=visual(root,'bsiSettingsTab_1')
        assert visual(selected,'voidSettingsNavIcon').property('tint').name()==preset['palette']['accent']
    # Reference footer picker is a real theme dropdown, not a decorative label.
    click(visual(root,'voidSidebarThemePicker'))
    QTest.keyClick(window,Qt.Key_Home)
    for _ in range(3):QTest.keyClick(window,Qt.Key_Down)
    QTest.keyClick(window,Qt.Key_Return);settle()
    assert window.property('themeName')=='Forest','sidebar theme picker did not apply its selection'
    # Embedded Appearance never touches the backend tab index or its pending edits.
    assert visual(root,'openAppearance') is None
    window.setProperty('blockSwitch',True)
    requested=window.property('requested');index=window.property('pCurrentTabIndex')
    click(visual(root,'bsiSettingsAppearance'))
    assert shell.property('appearanceOpen') and window.property('requested')==requested
    assert window.property('pCurrentTabIndex')==index and not page.isVisible()

    card=visual(root,'themePreset_forest');assert card is not None,str(warnings)
    click(card);assert window.property('themeName')=='Forest'
    assert slider.property('pColor').property('sliderFill').name()=='#82dfb1'
    appearance_capture=ROOT/'.cache/full-ui/appearance-fixture.png';appearance_capture.parent.mkdir(parents=True,exist_ok=True)
    assert window.grabWindow().save(str(appearance_capture))
    # Test the redesigned, vertically scrolled color editor with real typing,
    # not only direct Theme singleton calls. Apply stays outside the scroller.
    scroller=visual(root,'voidAppearanceScroll')
    accent=visual(root,'customColor_accent')
    flick=scroller.property('contentItem')
    offset=accent.mapToScene(QPointF(0,0)).y()-scroller.mapToScene(QPointF(0,0)).y()-12
    flick.setProperty('contentY',flick.property('contentY')+offset);settle()
    click(accent);QTest.keyClick(window,Qt.Key_A,Qt.ControlModifier)
    for character in '#aaccff':QTest.keyClick(window,character)
    settle()
    click(visual(root,'applyCustomTheme'))
    assert window.property('themeName')=='My theme','vertical custom-color editor did not apply'
    assert slider.property('pColor').property('sliderFill').name()=='#aaccff'
    assert window.grabWindow().save(str(ROOT/'.cache/full-ui/appearance-customization-fixture.png'))
    click(visual(root,'bsiSettingsTab_0'))
    assert not shell.property('appearanceOpen') and page.isVisible()
    assert window.property('pCurrentTabIndex')==index,'unsaved-change guard bypassed returning from Appearance'
    assert field.property('text')=='native','Appearance destroyed native pending edits'
    window.setProperty('blockSwitch',False)
    QMetaObject.invokeMethod(window,'customAppearance');settle()
    assert window.property('themeName')=='My theme'
    assert slider.property('pColor').property('sliderFill').name()=='#70ddb0'
    assert window.property('themeSurface').name()=='#101820'
    assert field.property('testInk').name()=='#f0f0f0'
    assert visual(root,'testSave').property('testInk').name()=='#f0f0f0'
    assert visual(field,'voidReferenceVisualBackground').property('radius')==18
    # Persistence is tested in a fresh QML engine, not just the same singleton.
    probe_engine=QQmlEngine()
    probe_engine.warnings.connect(lambda errors:warnings.extend(e.toString() for e in errors if "Settings type from Qt.labs.settings is deprecated" not in e.toString()))
    probe_component=QQmlComponent(probe_engine)
    probe_source='''import QtQuick 2.15
import "ASSETS" as Skin
QtObject {
 property string selected:Skin.Theme.selectedId
 property color surface:Skin.Theme.surface
 property int corner:Skin.Theme.corner
 property bool motion:Skin.Theme.motionEnabled
 Component.onCompleted:{Skin.Theme.preferencesFile="PREFERENCES";Skin.Theme.sync()}
}'''.replace('ASSETS',url).replace('PREFERENCES',str(temp/'appearance.ini').replace('\\','/'))
    probe_component.setData(probe_source.encode(),QUrl.fromLocalFile(str(temp/'Probe.qml')))
    probe=probe_component.create();assert probe,'\n'.join(e.toString() for e in probe_component.errors())
    settle()
    assert probe.property('selected')=='custom'
    assert probe.property('surface').name()=='#101820'
    assert probe.property('corner')==21 and not probe.property('motion')
    probe.deleteLater();settle();probe_engine.deleteLater();settle()
    # A real second QQmlEngine, separate settings file, and transient window:
    # state must propagate live through QObject signals, not incidental shared disk.
    peer_engine=QQmlEngine()
    peer_engine.warnings.connect(lambda errors:warnings.extend(e.toString() for e in errors if "Settings type from Qt.labs.settings is deprecated" not in e.toString()))
    peer_component=QQmlComponent(peer_engine)
    peer_source='''import QtQuick 2.15
import QtQuick.Window 2.15
import "ASSETS" as Skin
Window {
 id:peerHost;width:400;height:300
 property string selected:Skin.Theme.selectedId
 property color accent:Skin.Theme.accent
 property string requestedTheme:"ember"
 function choose(){Skin.Theme.choose(requestedTheme)}
 Component.onCompleted:{Skin.Theme.preferencesFile="PEER_INI";Skin.Theme.reset()}
 Skin.ThemeRelay {hostWindow:peerHost}
}'''.replace('ASSETS',url).replace('PEER_INI',str(temp/'separate-peer.ini').replace('\\','/'))
    peer_component.setData(peer_source.encode(),QUrl.fromLocalFile(str(temp/'Peer.qml')))
    peer=peer_component.create();assert peer,'\n'.join(e.toString() for e in peer_component.errors())
    peer.setTransientParent(window);settle()
    assert peer.property('selected')=='custom','transient engine failed initial theme handshake'
    for preset in json.loads((ROOT/'assets/full-emulator-ui/themes.json').read_text()):
        peer.setProperty('requestedTheme',preset['id']);QMetaObject.invokeMethod(peer,'choose');settle()
        assert window.property('themeName')==preset['name'],'separate engine did not update main theme'
        assert visual(face,'bsiOutlineGlyph').property('tint').name()==preset['palette']['accent']
        assert visual(visual(root,'bsiSettingsTab_1'),'voidSettingsNavIcon').property('tint').name()==preset['palette']['accent']
    QMetaObject.invokeMethod(window,'forestAppearance');settle()
    assert peer.property('selected')=='forest' and peer.property('accent').name()=='#82dfb1'
    peer.setTransientParent(None);peer.deleteLater();settle();peer_engine.deleteLater();settle()
    fallback_dir=temp/'without-settings-plugin';fallback_dir.mkdir()
    (fallback_dir/'Theme.qml').write_bytes((ROOT/'assets/full-emulator-ui/Theme.qml').read_bytes())
    (fallback_dir/'qmldir').write_text('singleton Theme 1.0 Theme.qml\n')
    (fallback_dir/'PreferencesStore.qml').write_text('import VoidMissingSettingsPlugin 1.0\nQtObject {}')
    fallback_engine=QQmlEngine();fallback_component=QQmlComponent(fallback_engine)
    fallback_source='''import QtQuick 2.15
import "FALLBACK" as Skin
QtObject {
 property bool available:Skin.Theme.persistenceAvailable
 property string selected:Skin.Theme.selectedId
 property string notice:Skin.Theme.persistenceNotice
 Component.onCompleted:Skin.Theme.choose("midnight")
}'''.replace('FALLBACK',QUrl.fromLocalFile(str(fallback_dir)).toString())
    fallback_component.setData(fallback_source.encode(),QUrl.fromLocalFile(str(temp/'Fallback.qml')))
    fallback=fallback_component.create();assert fallback,'\n'.join(e.toString() for e in fallback_component.errors())
    settle()
    assert not fallback.property('available') and fallback.property('selected')=='midnight'
    assert 'Session-only' in fallback.property('notice')
    fallback.deleteLater();settle();fallback_engine.deleteLater();settle()
    QMetaObject.invokeMethod(window,'resetAppearance');settle()
    if args.screenshot:
        args.screenshot.parent.mkdir(parents=True,exist_ok=True)
        assert window.grabWindow().save(str(args.screenshot))
    click(visual(root,'bsiSettingsClose'));assert window.property('closeRequested')
    assert not warnings,'\n'.join(warnings)
    window.setProperty('visible',False)
    window.deleteLater();settle()
print('PASS: Qt component/input fixture: reference offsets, native Save/toggle/checkbox/field/dropdown input and ACTUAL src-code UiSlider/row/column rendering, guarded native tab, no optimizer, late sidebar glyph. NOT live emulator testing.')
