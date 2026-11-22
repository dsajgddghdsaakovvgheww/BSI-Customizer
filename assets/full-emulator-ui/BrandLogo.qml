import QtQuick 2.15

// Full-color user-supplied artwork. Intentionally NOT passed through theme masks.
Image {
    objectName:"voidReferenceBrandLogo"
    readonly property int imageStatus:status
    source:Qt.resolvedUrl("Branding/VOID-Mark.png")
    fillMode:Image.PreserveAspectFit
    asynchronous:false
    cache:true
    smooth:true
    mipmap:true
}
