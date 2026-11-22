"""Actual TopbarSkin first paint, OEM instance color override and brand regressions.
Native input/window doubles; not live OEM certification.
"""
import json
import os
from pathlib import Path
import shutil
import tempfile
import time
os.environ.setdefault('QT_QPA_PLATFORM','offscreen')
os.environ.setdefault('QT_QUICK_BACKEND','software')
os.environ.setdefault('QT_QUICK_CONTROLS_STYLE','Basic')
os.environ.setdefault('QSG_RENDER_LOOP','basic')
from PySide6.QtCore import QMetaObject, QPoint, Qt, QUrl
from PySide6.QtGui import QGuiApplication, QColor, QImage, QFontDatabase
from PySide6.QtQml import QQmlComponent, QQmlEngine
from PySide6.QtQuick import QQuickWindow
from PySide6.QtTest import QTest

ROOT=Path(__file__).resolve().parents[1]
app=QGuiApplication([]);warnings=[]
if os.name=='nt':
    font=Path(os.environ['WINDIR'])/'Fonts/segoeui.ttf'
    if font.is_file():QFontDatabase.addApplicationFont(str(font))
def settle():
    for _ in range(30):app.processEvents();time.sleep(.01)
def find(node,name):
    if node.objectName()==name:return node
    for child in node.childItems():
        found=find(child,name)
        if found is not None:return found
    return None
def track(engine):
    engine.warnings.connect(lambda errors:warnings.extend(e.toString() for e in errors if 'Settings type from Qt.labs.settings is deprecated' not in e.toString()))

with tempfile.TemporaryDirectory() as folder:
    temp=Path(folder);assets=temp/'Skin';shutil.copytree(ROOT/'assets/full-emulator-ui',assets)
    ini=temp/'topbar.ini';theme=assets/'Theme.qml'
    theme.write_text(theme.read_text().replace('StandardPaths.writableLocation(StandardPaths.GenericConfigLocation) + "/VOID-Emulator-Themes-v9.ini"',json.dumps(ini.as_posix())))
    uri=QUrl.fromLocalFile(str(assets)).toString()
    magenta=QImage(16,16,QImage.Format_RGBA8888);magenta.fill(QColor('#ff00ff'));assert magenta.save(str(temp/'stock.png'))
    (temp/'UiImage.qml').write_text('''import QtQuick 2.15
Image {readonly property int testStatus:status;property string asset:"BlueStacksLogo";property int pImageWidth:20;property int pImageHeight:20;width:pImageWidth;height:pImageHeight;source:"stock.png";fillMode:Image.PreserveAspectFit}
''')
    (temp/'UiImageButton.qml').write_text('''import QtQuick 2.15
import QtQuick.Controls 2.15
Button {property string asset:"TitlebarClose";width:32;height:32;contentItem:Rectangle {color:"#ff00ff"}
background:Rectangle {color:"transparent"}}
''')
    source='''import QtQuick 2.15
import QtQuick.Window 2.15
import "SKIN" as Skin
Window {
 id:host;visible:true;width:600;height:240
 property string requested:"oled"
 property int clicks:0
 function choose(){Skin.Theme.choose(requested)}
 Rectangle {id:guest;objectName:"untouchedGuest";anchors.fill:parent;color:"#452600"}
 Rectangle {
  id:topbar;objectName:"nativeTopbar";width:parent.width;height:40
  // Exactly the missed contract: OEM color is assigned on the caller's instance.
  color:"#452600"
  gradient:Gradient {GradientStop {position:0;color:"#553311"} GradientStop {position:1;color:"#331100"}}
  UiImage {id:brand;objectName:"nativeProductLogo";x:10;y:8}
  UiImageButton {id:button;objectName:"nativeClose";x:556;y:4;onClicked:host.clicks++}
  Skin.TopbarSkin {host:topbar}
 }
 Component {id:lateFactory;UiImage {objectName:"lateProductLogo";x:50;y:8}}
 function addLogo(){lateFactory.createObject(topbar)}
 Text {x:20;y:80;text:"OEM topbar override / exact user artwork fixture";color:"white"}
 Text {x:20;y:110;text:"NOT a live emulator screenshot";color:"white"}
}'''.replace('SKIN',uri)
    (temp/'Host.qml').write_text(source)
    engine=QQmlEngine();track(engine)
    component=QQmlComponent(engine,QUrl.fromLocalFile(str(temp/'Host.qml')))
    assert not component.isError(),'\n'.join(e.toString() for e in component.errors())
    window=component.create();assert window,'\n'.join(e.toString() for e in component.errors())
    root=window.contentItem();topbar=find(root,'nativeTopbar');logo=find(root,'nativeProductLogo')
    first=[]
    def before_render():
        if not first:first.append((topbar.property('color').name(),logo.property('testStatus')))
    window.beforeRendering.connect(before_render,Qt.DirectConnection)
    for _ in range(100):
        if first:break
        app.processEvents();time.sleep(.002)
    assert first==[('#000000',1)],('topbar/brand not ready at first render',first,warnings)
    image=window.grabWindow();assert image.pixelColor(300,20).name()=='#000000','OEM topbar gradient still visible'
    assert sum(1 for x in range(10,38) for y in range(8,30) if image.pixelColor(x,y).blue()>60 and image.pixelColor(x,y).red()>30 and image.pixelColor(x,y).green()<70)>10,'user purple mark missing'
    assert 'Branding/VOID-Mark.png' in logo.property('source').toString()
    assert find(root,'untouchedGuest').property('color').name()=='#452600','topbar patch escaped its host'
    # A second QML engine with an independent INI: topbar-only hosts must relay
    # appearance too, including changes made while the main window is inactive.
    peer_engine=QQmlEngine();track(peer_engine);peer_component=QQmlComponent(peer_engine)
    peer_source='''import QtQuick 2.15
import QtQuick.Window 2.15
import "SKIN" as Skin
Window {
 id:peer;property string requested:"oled"
 function choose(){Skin.Theme.choose(requested)}
 Component.onCompleted:{Skin.Theme.preferencesFile="PEER_INI";Skin.Theme.reset()}
 Skin.ThemeRelay {hostWindow:peer}
}'''.replace('SKIN',uri).replace('PEER_INI',(temp/'separate.ini').as_posix())
    peer_component.setData(peer_source.encode(),QUrl.fromLocalFile(str(temp/'Peer.qml')))
    peer=peer_component.create();assert peer,'\n'.join(e.toString() for e in peer_component.errors())
    peer.setTransientParent(window);settle()
    original_brand=logo.property('source').toString()
    for preset in json.loads((assets/'themes.json').read_text()):
        peer.setProperty('requested',preset['id']);QMetaObject.invokeMethod(peer,'choose');settle()
        assert topbar.property('color').name()==preset['palette']['background'],preset['id']
        assert logo.property('source').toString()==original_brand,'brand was tinted/replaced with theme'
        assert window.grabWindow().pixelColor(300,20).name()==preset['palette']['background'],'OEM gradient returned'
    QMetaObject.invokeMethod(window,'addLogo');settle()
    assert 'Branding/VOID-Mark.png' in find(root,'lateProductLogo').property('source').toString()
    QTest.mouseClick(window,Qt.LeftButton,Qt.NoModifier,QPoint(572,20));settle()
    assert window.property('clicks')==1,'native topbar callback blocked by skin'
    output=ROOT/'.cache/full-ui/topbar-fixture.png';output.parent.mkdir(parents=True,exist_ok=True);assert image.save(str(output))
    assert not warnings,'\n'.join(warnings)
    peer.setTransientParent(None);peer.deleteLater();settle();peer_engine.deleteLater();settle()
    window.setVisible(False);window.deleteLater();settle()
print('PASS: first-render OEM topbar background + gradient override, exact purple user logo, six cross-engine themes, late logo and native action; guest background untouched. NOT live OEM certification.')
