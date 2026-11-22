"""Render generated card edits using real native layout wrappers and input doubles.
Not a live OEM player/backend certification. Values are deliberately fixture-only.
"""
import importlib.util
import json
import os
from pathlib import Path
import tempfile
import time
os.environ.setdefault('QT_QPA_PLATFORM','offscreen')
os.environ.setdefault('QT_QUICK_BACKEND','software')
os.environ.setdefault('QT_QUICK_CONTROLS_STYLE','Basic')
from PySide6.QtCore import QMetaObject, QPoint, QPointF, Qt, QUrl
from PySide6.QtGui import QGuiApplication, QFontDatabase
from PySide6.QtQml import QQmlComponent, QQmlEngine
from PySide6.QtTest import QTest
from PySide6.QtQuick import QQuickWindow

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('sections_suite',ROOT/'tools/full-emulator-ui/suite.py')
S=importlib.util.module_from_spec(spec);spec.loader.exec_module(S);U=S.runtime()
app=QGuiApplication([])
if os.name=='nt':
    for name in ('segoeui.ttf','seguisb.ttf','seguisym.ttf'):
        font=Path(os.environ['WINDIR'])/'Fonts'/name
        if font.is_file():QFontDatabase.addApplicationFont(str(font))
engine=QQmlEngine();warnings=[]
engine.warnings.connect(lambda errors:warnings.extend(e.toString() for e in errors if 'Settings type from Qt.labs.settings is deprecated' not in e.toString()))
url=QUrl.fromLocalFile(str(ROOT/'assets/full-emulator-ui')).toString()+'/'

def settle():
    for _ in range(30):app.processEvents();time.sleep(.01)

def visual(item,name):
    if item.objectName()==name:return item
    for child in item.childItems():
        result=visual(child,name)
        if result is not None:return result
    return None

with tempfile.TemporaryDirectory() as folder:
    temp=Path(folder)
    for name in ('UiColumnLayout.qml','UiRowLayout.qml'):
        (temp/name).write_bytes((ROOT/'reference/full-emulator-ui/controls'/name).read_bytes())
    # Input/host doubles only; wrapper geometry and source transform are real.
    (temp/'UiButton.qml').write_text('''import QtQuick 2.15
import QtQuick.Controls 2.15
Button {
 id:button;property QtObject pColor:QtObject {property QtObject normal:QtObject {property color textColor:"magenta"}}
 contentItem:Text {text:button.text;color:button.pColor?button.pColor.normal.textColor:"magenta";verticalAlignment:Text.AlignVCenter;horizontalAlignment:Text.AlignHCenter;font.pixelSize:13}
}
''')
    page='''import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "SKIN" as Skin
Item {
 property bool pCompLoaded:true
 property int pCurrentCpu:4
 property int pMaxCpu:8
 property int saved:0
 UiColumnLayout {
  anchors.fill:parent;spacing:16
  ScrollView {
   Layout.fillWidth:true;Layout.fillHeight:true;clip:true
   contentWidth:availableWidth
   UiColumnLayout {
    id:iContentLayout;width:520;spacing:16
    UiColumnLayout {
     Component.onCompleted:objectName="fixtureCpuCard";spacing:12
     Text {text:"CPU allocation";color:Skin.Theme.text;font.pixelSize:16;font.bold:true}
     Text {text:"Allocate processing power for your games.";color:Skin.Theme.muted;font.pixelSize:12}
     ComboBox {id:iCpuCombobox;objectName:"fixtureCpu";model:["High (4 cores)","Medium (2 cores)"];Layout.preferredWidth:220}
    }
    UiColumnLayout {
     Component.onCompleted:objectName="fixtureRamCard";spacing:12
     Text {text:"Memory allocation";color:Skin.Theme.text;font.pixelSize:16;font.bold:true}
     Text {text:"Keep enough memory available for your other apps.";color:Skin.Theme.muted;font.pixelSize:12}
     ComboBox {id:iRamCombobox;model:[{text:"High (4 GB)",value:"4096"},{text:"Medium (2 GB)",value:"2048"}];textRole:"text";valueRole:"value";Layout.preferredWidth:220}
     TextField {id:iCustomRamTextField;text:"4096";visible:false}
    }
    UiColumnLayout {
     Component.onCompleted:objectName="fixtureModeCard";spacing:12
     Text {text:"Performance mode";color:Skin.Theme.text;font.pixelSize:16;font.bold:true}
     ComboBox {model:["High performance","Balanced","Low memory"];Layout.preferredWidth:220}
    }
    UiColumnLayout {
     Component.onCompleted:objectName="fixtureFpsCard";spacing:12
     Text {text:"Frame rate";color:Skin.Theme.text;font.pixelSize:16;font.bold:true}
     Text {text:Math.round(iFrameRateSlider.value)+" FPS";color:Skin.Theme.accent;font.pixelSize:14}
     Slider {id:iFrameRateSlider;objectName:"fixtureFps";from:1;to:240;value:120;stepSize:1;Layout.fillWidth:true}
     CheckBox {text:"Enable high frame rate";checked:true}
    }
   }
  }
  UiRowLayout {
   Layout.fillWidth:true
   Text {text:"Fixture values / NOT connected to a live emulator";color:Skin.Theme.muted;font.pixelSize:11;Layout.fillWidth:true}
   UiButton {objectName:"fixtureNativeSave";text:qsTranslate("QObject","Save changes");onClicked:saved++}
  }
 }
}'''.replace('SKIN',url)
    patched=S.native_section_sources(U,{'UiPerformanceSetting.qml':page.encode()},url)['UiPerformanceSetting.qml']
    assert patched.count(b'pVoidSectionCard: true')==4
    (temp/'Page.qml').write_bytes(patched)
    source='''import QtQuick 2.15
import QtQuick.Window 2.15
import "SKIN" as Skin
Window {
 id:host;visible:true;width:1440;height:960
 property int pCurrentTabIndex:0
 property string requested:"oled"
 function choose(){Skin.Theme.choose(requested)}
 function fVoidSelectNative(index){pCurrentTabIndex=index}
 Component.onCompleted:{Skin.Theme.preferencesFile="INI";Skin.Theme.reset()}
 property QtObject pSettingsItems:QtObject {property list<QtObject> pSettingElements:[
  QtObject {property string pButtonId:"engine";property string pButtonText:"Performance";property var pComponentObj:null},
  QtObject {property string pButtonId:"display";property string pButtonText:"Display";property var pComponentObj:null},
  QtObject {property string pButtonId:"graphics";property string pButtonText:"Graphics";property var pComponentObj:null}
 ]}
 Skin.SettingsShell {
  id:shell;anchors.fill:parent;bsiHost:host
  Component.onCompleted:{var page=pageFactory.createObject(shell.pageContainer);host.pSettingsItems.pSettingElements[0].pComponentObj=page;shell.stylePage(page)}
 }
 Component {id:pageFactory;Page {objectName:"fixtureSectionPage";anchors.fill:parent}}
 Text {z:100;anchors.bottom:parent.bottom;anchors.horizontalCenter:parent.horizontalCenter;text:"Qt native-layout + input-double fixture — NOT a live OEM screenshot";color:Skin.Theme.muted;font.pixelSize:10}
}'''.replace('SKIN',url).replace('INI',(temp/'theme.ini').as_posix())
    (temp/'Window.qml').write_text(source)
    component=QQmlComponent(engine,QUrl.fromLocalFile(str(temp/'Window.qml')))
    assert not component.isError(),'\n'.join(e.toString() for e in component.errors())
    window=component.create();assert window,'\n'.join(e.toString() for e in component.errors())
    settle()
    assert window.width()<=1000 and window.height()<=680,"Settings still opens oversized"
    # Render the normal 1000px design even on Qt offscreen's 800px virtual screen.
    window.setWidth(1000);window.setHeight(680);settle()
    root=window.contentItem();page=visual(root,'fixtureSectionPage')
    assert page is not None
    cards=[visual(root,name) for name in ('fixtureCpuCard','fixtureRamCard','fixtureModeCard','fixtureFpsCard')]
    assert all(card is not None and card.width()>330 and card.height()>65 for card in cards),[(c.width(),c.height()) if c else None for c in cards]
    points=[card.mapToScene(QPointF(0,0)) for card in cards]
    assert abs(points[0].y()-points[1].y())<1,'CPU/RAM are still the old vertical alignment'
    assert points[1].x()>=points[0].x()+cards[0].width()+11,'paired resources overlap'
    assert abs(cards[0].width()-cards[1].width())<2,'resource columns are unbalanced'
    assert points[2].y()>=points[0].y()+max(cards[0].height(),cards[1].height())+11
    assert points[3].y()>=points[2].y()+cards[2].height()+11
    assert cards[2].width()>cards[0].width()*1.9,'mode card must span both columns'
    assert visual(root,'voidNativeResourceSummary').isVisible(),'compact selections line missing'
    for preset in json.loads((ROOT/'assets/full-emulator-ui/themes.json').read_text()):
        window.setProperty('requested',preset['id']);QMetaObject.invokeMethod(window,'choose');settle()
        for card in cards:
            assert card.property('color').name()==preset['palette']['surface'],preset['id']
            assert card.property('radius')==preset['radius']
        button=visual(root,'fixtureNativeSave')
        assert button.property('pColor').property('normal').property('textColor').name()==preset['palette']['background']
    window.setProperty('requested','oled');QMetaObject.invokeMethod(window,'choose');settle()
    button=visual(root,'fixtureNativeSave');p=button.mapToScene(QPointF(button.width()/2,button.height()/2))
    QTest.mouseClick(window,Qt.LeftButton,Qt.NoModifier,QPoint(round(p.x()),round(p.y())));settle()
    assert page.property('saved')==1,'native Save callback lost under primary visuals'
    output=ROOT/'.cache/full-ui/sections-fixture.png';output.parent.mkdir(parents=True,exist_ok=True)
    assert window.grabWindow().save(str(output))
    window.setWidth(720);settle()
    assert not visual(root,'voidNativeResourceSummary').isVisible(),'summary crowded narrow controls'
    assert all(card.width()>500 for card in cards)
    points=[card.mapToScene(QPointF(0,0)) for card in cards]
    assert points[1].y()>=points[0].y()+cards[0].height()+11,"paired cards did not stack on a narrow screen"
    assert not warnings,'\n'.join(warnings)
    window.setVisible(False);window.deleteLater();settle()
print('PASS: generated native cards render without overlap using src-code layout wrappers; six palettes/corners, primary Save callback/contrast, compact real-selection line + responsive resource grid. Native input doubles, NOT live OEM certification.')
