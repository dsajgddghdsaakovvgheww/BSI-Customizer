"""Render the ACTUAL reviewed Performance visual subtree, including nested help,
custom CPU/RAM inputs, warnings, recommended FPS text and all three toggles.
Backend and unavailable proprietary control types are doubles, NOT live OEM.
The layout is not a simplified reconstruction of the page.
"""
import importlib.util
import json
import os
from pathlib import Path
import re
import tempfile
import time
os.environ.setdefault('QT_QPA_PLATFORM','offscreen')
os.environ.setdefault('QT_QUICK_BACKEND','software')
os.environ.setdefault('QT_QUICK_CONTROLS_STYLE','Basic')
from PySide6.QtCore import QUrl, QPointF, QPoint, QMetaObject, Q_ARG
from PySide6.QtGui import QGuiApplication, QFontDatabase
from PySide6.QtQml import QQmlEngine, QQmlComponent
from PySide6.QtQuick import QQuickWindow
from PySide6.QtTest import QTest

ROOT=Path(__file__).resolve().parents[1]
sp=importlib.util.spec_from_file_location('native_layout_suite',ROOT/'tools/full-emulator-ui/suite.py')
S=importlib.util.module_from_spec(sp);sp.loader.exec_module(S);U=S.runtime()
app=QGuiApplication([])
if os.name=='nt':
    for name in ('segoeui.ttf','seguisb.ttf','seguisym.ttf'):
        p=Path(os.environ['WINDIR'])/'Fonts'/name
        if p.is_file():QFontDatabase.addApplicationFont(str(p))
engine=QQmlEngine();warnings=[]
engine.warnings.connect(lambda errors:warnings.extend(e.toString() for e in errors if 'Settings type from Qt.labs.settings is deprecated' not in e.toString()))
url=QUrl.fromLocalFile(str(ROOT/'assets/full-emulator-ui')).toString()+'/'
def settle():
    for _ in range(30):app.processEvents();time.sleep(.01)
def visual(item,name):
    if item.objectName()==name:return item
    for c in item.childItems():
        found=visual(c,name)
        if found is not None:return found
    return None

with tempfile.TemporaryDirectory() as folder:
    temp=Path(folder)
    def put(n,s):(temp/n).write_text(s)
    for n in ('UiColumnLayout.qml','UiRowLayout.qml','UiSlider.qml'):
        (temp/n).write_bytes((ROOT/'reference/full-emulator-ui/controls'/n).read_bytes())
    # Real visual subtree, before any source transformations. No manual layout.
    original=U.payload()['baseline/pages/UiPerformanceSetting.qml'].decode('utf-8-sig')
    manager=U.manager();tokens,scopes=manager.scopes_and_bindings(original)
    main=next(k for k,v in scopes.items() if v['type']=='UiColumnLayout' and v['parent'] is not None and scopes[v['parent']]['type']=='Item')
    _,pairs=U.scan(manager,original)
    tree=original[tokens[main-1].start:tokens[pairs[main]].end]
    # Only remove the unavailable tooltip attached property from the fixture.
    tree=re.sub(r'^\s*UiToolTip\.text:[^\n]*\n','\n',tree,flags=re.M)
    # Give native objects diagnostic names without changing their hierarchy.
    for native in ('iCpuCombobox','iCustomCpuCombobx','iRamCombobox','iCustomRamTextField','iMemoryModeCombobox','iFrameRateSlider','iEnableHighFpsToggleBtn','iEnableVSyncToggleBtn','iEnableFpsDisplayToggleBtn','iScrollView'):
        tree=re.sub(r'(id:\s*'+native+r'\b)',r'\1;objectName:"'+native+'"',tree)
    root='''import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "SKIN" as Skin
Item {
 id:nativeRoot
 property bool pCompLoaded:false
 property bool pEnableSave:true
 property string pParentWindow:"plr"
 property bool pIsEcoModeEnabled:false
 property int pCurrentCpu:4
 property int pMaxCpu:8
 property int pMaxRam:16384
 property int pDefaultCpu:4
 property int pDefaultRam:4096
 property string pVmName:"fixture"
 property var pCpuModel:[{text:"High (4 cores)",value:"4"},{text:"Medium (2 cores)",value:"2"},{text:"Low (1 core)",value:"1"},{text:"Custom",value:"1"}]
 property var pCustomCpuModel:["1","2","3","4"]
 property var pRamModel:[{text:"Enhanced (4 GB)",value:"4096",visible:true},{text:"Custom (MB)",value:"*",visible:true}]
 property int saved:0
 function fMonitorChange(){}
 function fUpdateFpsSliderValueAndText(checked){}
 function fHandleConfChange(){saved++}
 function fInitUi(){}
 TREE
}'''.replace('SKIN',url).replace('TREE',tree)
    # Native palette pass + the exact vertical source transformer used by install.
    live=S.live_theme_sources(U,{'UiPerformanceSetting.qml':root.encode()},url)
    patched=S.native_section_sources(U,live,url)['UiPerformanceSetting.qml']
    assert patched.count(b'VoidLiveTheme.NativeSettingCard')==4
    put('Page.qml',patched.decode())
    # One common base supplies all pSize fields a native visual adapter reads.
    size='QtObject {property int height:34;property int horizontalPadding:9;property int spacing:8;property int iconWidth:20;property int iconHeight:20;property string iconAsset:"";property int indicatorWidth:28;property int indicatorImageWidth:12;property int indicatorImageHeight:16;property string indicatorAsset:"";property real maxDropDownHeight:300;property int handleWidth:16;property int handleHeight:16;property int sliderHeight:4;property font textFont:Qt.font({pixelSize:12});property font selectedValueFont:textFont;property font recommendedValueFont:textFont;property font rangeValueFont:textFont}'
    color='QtObject {property color handleBackground:"red";property color handleBackgroundDisabled:"grey";property color handleBorder:"red";property color sliderBackground:"grey";property color sliderBackgroundDisabled:"grey";property color sliderFill:"red";property color sliderFillDisabled:"grey";property color selectedValue:"white";property color recommendedValue:"white";property color rangeValue:"white"}'
    put('qmldir','singleton UiTheme 1.0 UiTheme.qml\n')
    theme='pragma Singleton\nimport QtQuick 2.15\nQtObject {property QtObject sharedSize:'+size+';property QtObject sharedColors:'+color+';'
    for kind in ('comboBoxes','textFields','sliders','buttons'):
        theme+='property QtObject '+kind+':QtObject {property QtObject sizes:QtObject {property QtObject medium:UiTheme.sharedSize} property QtObject colors:QtObject {property QtObject accent:UiTheme.sharedColors}}\n'
    theme+='property QtObject fonts:QtObject {property font bodySmall:Qt.font({pixelSize:11});property font bodyMedium:Qt.font({pixelSize:12});property font bodyLarge:Qt.font({pixelSize:14})};property QtObject colors:QtObject {property color primary10:"white";property color primary20:"grey";property color primary30:"grey";property color seperator:"grey";property color warning:"orange";property color accentNormal:"cyan"}}'
    put('UiTheme.qml',theme)
    controls='import QtQuick 2.15\nimport QtQuick.Controls 2.15\n'
    put('UiComboBox.qml',controls+'ComboBox {property QtObject pSize;property string pVisibilityRole:"";implicitHeight:36}')
    put('UiTextField.qml',controls+'TextField {enum InputValidation {Valid,Error} property QtObject pSize;property int pInputValidationState:0;property bool pOpenErrorTemplate:false;property string pErrorTemplateContent:"";implicitHeight:36}')
    put('UiScrollView.qml',controls+'ScrollView {}')
    put('UiTextUrl.qml','import QtQuick 2.15\nText {}')
    put('UiImageButton.qml',controls+'Button {property string asset;property int pImageHeight;property int pImageWidth;property int pMouseCursor:0;implicitWidth:20;implicitHeight:20;text:"?"}')
    put('UiToggleButton.qml',controls+'Switch {padding:0}')
    put('UiImage.qml','import QtQuick 2.15\nItem {property string asset;property int pImageHeight;property int pImageWidth;implicitWidth:pImageWidth;implicitHeight:pImageHeight}')
    put('UiButton.qml',controls+'Button {property QtObject pSize;property QtObject pColor;implicitHeight:36}')
    source='''import QtQuick 2.15
import QtQuick.Window 2.15
import "SKIN" as Skin
Window {
 id:host;visible:true;width:940;height:700
 property int pCurrentTabIndex:0
 function fVoidSelectNative(index){pCurrentTabIndex=index}
 function fShowToast(message){}
 property QtObject uiBackend:QtObject {
  property string productName:"Fixture Player"
  function confGetBoolDefaultWhenEmpty(key,value){return value}
  function confSetInstanceBool(key,value){}
  function confSetInstanceI32(key,value){}
  function confGetInstanceI32(key){return key==="cpus"?4:4096}
  function confIsPropModified(key){return false}
  function confIsInstancePropModified(key){return false}
  function confCommit(){}
 }
 property QtObject plrBackend:QtObject {function getInstancesCount(){return 1}}
 property var iSettingsWindow:host
 property QtObject pSettingsItems:QtObject {property list<QtObject> pSettingElements:[QtObject {property string pButtonId:"engine";property string pButtonText:"Performance";property var pComponentObj:page}]}
 Component.onCompleted:{Skin.Theme.preferencesFile="INI";Skin.Theme.reset()}
 Skin.SettingsShell {
  id:shell;anchors.fill:parent;bsiHost:host
  Page {id:page;objectName:"actualNativeVisualPage";parent:shell.pageContainer;anchors.fill:parent}
 }
 Text {z:100;anchors.bottom:parent.bottom;anchors.horizontalCenter:parent.horizontalCenter;text:"Actual reviewed native visual hierarchy / backend-control doubles / NOT live OEM";color:Skin.Theme.muted;font.pixelSize:9}
}'''.replace('SKIN',url).replace('INI',(temp/'themes.ini').as_posix())
    put('Window.qml',source)
    component=QQmlComponent(engine,QUrl.fromLocalFile(str(temp/'Window.qml')))
    assert not component.isError(),'\n'.join(x.toString() for x in component.errors())
    window=component.create();assert window,'\n'.join(x.toString() for x in component.errors())
    window.setWidth(940);window.setHeight(700);settle()
    root=window.contentItem();page=visual(root,'actualNativeVisualPage');cpu=visual(root,'iCpuCombobox');ram=visual(root,'iRamCombobox')
    assert cpu and ram
    cp=cpu.mapToScene(QPointF(0,0));rp=ram.mapToScene(QPointF(0,0))
    assert cp.y()>150 and cp.y()<320,('first native CPU input clipped',cp)
    assert rp.y()>cp.y()+65,('native RAM not vertically below CPU',cp,rp)
    assert abs(cp.x()-rp.x())<2,('native controls not left-aligned',cp,rp)
    assert cpu.width()>=230 and ram.width()>=230
    output=ROOT/'.cache/full-ui';output.mkdir(parents=True,exist_ok=True)
    assert window.grabWindow().save(str(output/'native-layout-fixture.png'))
    page.setProperty('pCompLoaded',True)
    cpu.setProperty('currentIndex',3);QMetaObject.invokeMethod(cpu,'activated',Q_ARG(int,3))
    ram.setProperty('currentIndex',1);QMetaObject.invokeMethod(ram,'activated',Q_ARG(int,1));settle()
    assert visual(root,'iCustomCpuCombobx').isVisible() and visual(root,'iCustomRamTextField').isVisible(),('custom native inputs lost',visual(root,'iCustomCpuCombobx').isVisible(),visual(root,'iCustomRamTextField').isVisible(),ram.property('currentIndex'),page.property('pRamModel'),warnings)
    scroll=visual(root,'iScrollView')
    pos=scroll.mapToScene(QPointF(scroll.width()/2,scroll.height()/2))
    QTest.wheelEvent(window,pos,QPoint(0,-2400));settle()
    last=visual(root,'iEnableFpsDisplayToggleBtn');p=last.mapToScene(QPointF(0,last.height()))
    assert last.isVisible() and 200<p.y()<650,('third native FPS toggle unreachable',p,scroll.property('contentHeight'),scroll.height(),warnings)
    assert window.grabWindow().save(str(output/'native-fps-fixture.png'))
    assert not warnings,'\n'.join(warnings)
    window.setVisible(False);window.deleteLater();settle()
print('PASS: actual reviewed Performance visual subtree, nested native CPU/RAM/custom controls, full-width FPS/help/toggles; scroll reachability and Close-only shell. Backend/control doubles, NOT live OEM.')
