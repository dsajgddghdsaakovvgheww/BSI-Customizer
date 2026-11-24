#include "ShaderBridge.h"
#include "ShaderPreferences.h"
#include "Runtime.h"
#include <QCoreApplication>
#include <QClipboard>
#include <QGuiApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QPlatformSurfaceEvent>
#include <QPointer>
#include <QSet>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>
#include <QWindow>

#ifndef BSI_SHADERS_BUILD_REVISION
#define BSI_SHADERS_BUILD_REVISION "unversioned"
#endif
namespace BsiShaders {
namespace {
// A child Bootstrap completes before its QML Window necessarily does. Do not
// force QWindow::create()/winId() in that partially constructed Quick window
// (notably Qt 5). Install API hooks early, and attach HWND ownership when Qt
// has actually created its platform surface. Also revoke it on recreation.
class SurfaceObserver final : public QObject {
public:
    explicit SurfaceObserver(QWindow *window):QObject(window),window(window){window->installEventFilter(this);}
    bool eventFilter(QObject *object,QEvent *event) override {
        if(object==window&&event->type()==QEvent::PlatformSurface){
            const auto *surface=static_cast<QPlatformSurfaceEvent*>(event);
            if(surface->surfaceEventType()==QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed)Runtime::instance().setWindow(0);
            else if(window&&window->handle())Runtime::instance().setWindow(std::uintptr_t(window->winId()));
        }
        return false;
    }
private:
    QPointer<QWindow> window;
};
struct State {
    ShaderPreferences preferences;
    QString instance,error;
    QSet<ShaderBridge*> bridges;
    QPointer<SurfaceObserver> surfaceObserver;
    bool attached=false,connected=false;
};
State &state(){static auto *s=new State;return *s;}
QString fileFor(const QString &instance){
    const auto key=QCryptographicHash::hash((QFileInfo(QCoreApplication::applicationFilePath()).canonicalFilePath()+QLatin1Char('\n')+instance).toUtf8(),QCryptographicHash::Sha256).toHex();
    return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation))
        .filePath(QStringLiteral("BSICustomizer/Shaders/")+QString::fromLatin1(key)+QStringLiteral(".json"));
}
template<std::size_t N> QVariantMap auditCounters(const std::array<const char*,N> &labels,const std::array<std::uint64_t,N> &values){
    QVariantMap result;
    for(std::size_t i=0;i<N;++i)result.insert(QString::fromLatin1(labels[i]),QVariant::fromValue<qulonglong>(values[i]));
    return result;
}
QString auditLine(const QVariant &value){
    QString result;const auto counters=value.toMap();
    for(auto it=counters.cbegin();it!=counters.cend();++it)if(it.value().toULongLong()){
        if(!result.isEmpty())result+=QStringLiteral("; ");
        result+=it.key()+QLatin1Char('=')+it.value().toString();
    }
    return result.isEmpty()?QStringLiteral("none"):result;
}
void notify(){for(auto *bridge:state().bridges){emit bridge->settingsChanged();emit bridge->metricsChanged();}}
}
ShaderBridge::ShaderBridge(QObject *parent):QObject(parent){
    state().bridges.insert(this);
    auto *timer=new QTimer(this);timer->setInterval(1000);
    connect(timer,&QTimer::timeout,this,&ShaderBridge::metricsChanged);timer->start();
}
ShaderBridge::~ShaderBridge(){state().bridges.remove(this);}
QVariantMap ShaderBridge::saved()const{return state().preferences.values();}
QString ShaderBridge::error()const{return state().error.isEmpty()?state().preferences.error():state().error;}
QVariantMap ShaderBridge::display()const{return state().preferences.displayStatus();}
QVariantMap ShaderBridge::previewPotato(int mode)const{return state().preferences.previewPotato(mode);}
bool ShaderBridge::bindNativeDisplay(QObject *backend){
    auto &s=state();
    if(!s.attached)return false;
    const bool okay=s.preferences.bindDisplay(backend);
    Runtime::instance().set(s.preferences.parameters());Runtime::instance().setTexture(s.preferences.texture());notify();return okay;
}
bool ShaderBridge::bindNativePlayer(QObject *backend){
    if(!state().attached)return false;
    const bool okay=state().preferences.bindPlayer(backend);notify();return okay;
}
QVariantMap ShaderBridge::restartPlayer(){
    const auto response=state().preferences.requestRestart();notify();return response;
}
QVariantMap ShaderBridge::bridgeInfo()const{
    return {{"moduleQtVersion",QString::fromLatin1(QT_VERSION_STR)},
            {"runtimeQtVersion",QString::fromLatin1(qVersion())},{"publicQtApi",true},
            {"moduleBuildRevision",QStringLiteral(BSI_SHADERS_BUILD_REVISION)},
            {"moduleCaptureVersion",QStringLiteral("gl-draw-write-footprint-v7-edge2")}};
}
QVariantList ShaderBridge::controlList()const{
    QVariantList out;
    for(const auto &c:controls)out<<QVariantMap{{"key",QString::fromLatin1(c.key)},{"label",QString::fromLatin1(c.label)},{"minimum",c.minimum},{"maximum",c.maximum}};
    return out;
}
QVariantMap ShaderBridge::metrics()const{
    const auto m=Runtime::instance().metrics();
    const auto texture=Runtime::instance().texture();
    const auto number=[](std::uint64_t value){return QVariant::fromValue<qulonglong>(value);};
    const auto depth=[&](TextureDepthFailure reason){return number(m.textureDepthReasons[static_cast<std::size_t>(reason)]);};
    std::uint64_t deferredRetiredOther=0;
    for(std::size_t i=0;i<m.textureDeferredAudit.exits.size();++i)
        if(i!=static_cast<std::size_t>(GlDeferredCopyExit::Expired))deferredRetiredOther+=m.textureDeferredAudit.exits[i];
    const auto label=[](RendererFamily f){return f==RendererFamily::Vulkan?"Vulkan":f==RendererFamily::DirectX?"DirectX":f==RendererFamily::OpenGL?"OpenGL":"Automatic / unknown";};
    return {{"hooksAttached",m.hooks},{"gameSurfaceObserved",m.observed},{"active",m.active},
            {"requestedRenderer",m.requested==RendererFamily::Automatic?QStringLiteral("Automatic"):QString::fromLatin1(label(m.requested))},
            {"configuredRenderer",QString::fromLatin1(label(m.configured))},
            {"renderer",QString::fromLatin1(capability(m.renderer).label)},
            {"backendImplemented",capability(m.renderer).pipelineImplemented},{"deviceRequirementsMet",m.deviceReady},
            {"processedFrames",number(m.processed)},{"status",QString::fromStdString(m.status)},
            {"potatoGraphicEnabled",texture.enabled},{"potatoStrength",texture.strength},{"textureSupported",m.textureSupported},
            {"textureDepthIndependent",m.texturePolicy==TextureDrawPolicy::OwnedMipCompatibility},
            {"texturePolicy",m.texturePolicy==TextureDrawPolicy::OwnedMipCompatibility?QStringLiteral("Owned mip compatibility (depth independent)"):QStringLiteral("Depth-qualified scene")},
            {"textureEligibleDraws",number(m.textureSceneDraws)},
            {"textureDraws",number(m.textureDraws)},{"textureBindings",number(m.textureBindings)},{"textureStatus",QString::fromStdString(m.textureStatus)},
            {"textureInspected",number(m.textureInspected)},{"textureSceneDraws",number(m.textureSceneDraws)},
            {"textureDepthBypasses",number(m.textureDepthBypasses)},{"textureBlendBypasses",number(m.textureBlendBypasses)},
            {"textureSamplerBypasses",number(m.textureSamplerBypasses)},{"textureOtherBypasses",number(m.textureOtherBypasses)},
            {"textureDepthTestDisabled",depth(TextureDepthFailure::TestDisabled)},{"textureDepthWritesDisabled",depth(TextureDepthFailure::WritesDisabled)},
            {"textureDepthNever",depth(TextureDepthFailure::Never)},{"textureDepthAlways",depth(TextureDepthFailure::Always)},
            {"textureDepthMissingAttachment",depth(TextureDepthFailure::MissingAttachment)},{"textureDepthInsufficientPrecision",depth(TextureDepthFailure::InsufficientPrecision)},
            {"textureDepthUnspecified",depth(TextureDepthFailure::None)},
            {"textureNestedInspected",number(m.textureNestedInspected)},{"textureNestedDraws",number(m.textureNestedDraws)},
            {"textureConsumerDraws",number(m.textureConsumerDraws)},{"textureConsumerInputs",number(m.textureConsumerInputs)},
            {"textureProducerInspected",number(m.textureProducerInspected)},{"textureProducerDraws",number(m.textureProducerDraws)},
            {"textureProducerBindings",number(m.textureProducerBindings)},{"textureProducerStatus",QString::fromStdString(m.textureProducerStatus)},
            {"textureTransferCalls",number(m.textureTransferCalls)},{"textureTransferInputs",number(m.textureTransferInputs)},
            {"textureDeferredCopies",number(m.textureDeferredCopies)},{"textureDeferredInputs",number(m.textureDeferredInputs)},
            {"textureDeferredQueued",number(m.textureDeferredAudit.queued)},{"textureDeferredPending",number(m.textureDeferredAudit.pending)},
            {"textureDeferredPeak",number(m.textureDeferredAudit.peak)},{"textureDeferredAuditSequence",number(m.textureDeferredAudit.sequence)},
            {"textureDeferredAuditBalanced",m.textureDeferredAudit.balanced()},
            {"textureDeferredExpired",number(m.textureDeferredAudit.exits[static_cast<std::size_t>(GlDeferredCopyExit::Expired)])},
            {"textureDeferredRetiredOther",number(deferredRetiredOther)},
            {"textureDeferredExits",auditCounters(glDeferredExitLabels,m.textureDeferredAudit.exits)},
            {"textureDeferredConsumers",auditCounters(glDeferredConsumerLabels,m.textureDeferredAudit.consumers)},
            {"textureDeferredDiscardSites",auditCounters(glDeferredDiscardLabels,m.textureDeferredAudit.discardSites)},
            {"textureDeferredSources",auditCounters(glDeferredSourceLabels,m.textureDeferredAudit.sources)},
            {"texturePresentedInputObservations",number(m.textureDeferredAudit.presentedInputObservations)},
            {"texturePresentedInputDistinct",number(m.textureDeferredAudit.presentedInputDistinct)},
            {"texturePresentedInputUntracked",number(m.textureDeferredAudit.presentedInputUntracked)},
            {"textureCopyObservations",number(m.textureDeferredAudit.copyObservations)},
            {"textureCopyInputRecentPresentationMatches",number(m.textureDeferredAudit.copyInputRecentPresentedMatches)},
            {"textureCopyOutputRecentPresentationMatches",number(m.textureDeferredAudit.copyOutputRecentPresentedMatches)},
            {"texturePresentationRecentCopyInputMatches",number(m.textureDeferredAudit.presentationInputsRecentCopyInputMatches)},
            {"texturePresentationRecentCopyOutputMatches",number(m.textureDeferredAudit.presentationInputsRecentCopyOutputMatches)},
            {"textureCopyInputUntracked",number(m.textureDeferredAudit.copyInputUntracked)},
            {"textureCopyOutputDistinct",number(m.textureDeferredAudit.copyOutputDistinct)},
            {"textureCopyOutputUntracked",number(m.textureDeferredAudit.copyOutputUntracked)},
            {"textureCopyOutputFrames",number(m.textureDeferredAudit.copyOutputFrames)},
            {"textureCopyOutputMaxFramesPerName",number(m.textureDeferredAudit.copyOutputMaxFramesPerName)},
            {"textureDeferredHopBlocked",number(m.textureDeferredAudit.hopBlocked)},{"textureDeferredResourceBlocked",number(m.textureDeferredAudit.resourceBlocked)},
            {"textureHookDrawCalls",number(m.textureHookDrawCalls)},{"textureHookTransferCalls",number(m.textureHookTransferCalls)},
            {"textureHookImageCopyCalls",number(m.textureHookImageCopyCalls)},{"textureHookNativeImageCalls",number(m.textureHookNativeImageCalls)},
            {"textureTransferApis",auditCounters(glTransferApiLabels,m.textureTransferAudit.calls)},
            {"textureTransferStops",auditCounters(glTransferLabels,m.textureTransferAudit.stops)},
            {"textureTransferObserveStops",auditCounters(glObserveLabels,m.textureTransferAudit.observeStops)},
            {"textureTransferObserveReached",static_cast<qint64>(m.textureTransferAudit.observeReached)},
            {"textureTransferHandlerEntries",static_cast<qint64>(m.textureTransferAudit.outerHandlerEntries)},
            {"textureTransferCandidates",auditCounters(glCandidateLabels,m.textureTransferAudit.candidates)},
            {"textureTransferDescriptors",auditCounters(glDescriptorLabels,m.textureTransferAudit.descriptors)},
            {"textureTransferCodeProviders",auditCounters(std::array<const char*,3>{{"wgl-only","egl-only","mixed-or-unattributed"}},m.textureTransferAudit.codeProviders)},
            {"textureLastBypass",QString::fromStdString(m.textureLastBypass)},
            {"vulkanLoaders",number(m.vulkan.loaders)},{"vulkanInstances",number(m.vulkan.instances)},
            {"vulkanDevices",number(m.vulkan.devices)},{"vulkanSurfaces",number(m.vulkan.surfaces)},
            {"vulkanSwapchains",number(m.vulkan.swapchains)},{"vulkanPresents",number(m.vulkan.presents)},
            {"vulkanMissedQueues",number(m.vulkan.missedQueues)},{"vulkanRejectedWindows",number(m.vulkan.rejectedWindows)},
            {"vulkanLoader",QString::fromStdString(m.vulkan.loader)}};
}
QString ShaderBridge::diagnostics()const{
    // User-triggered clipboard output only; no upload, persistent log or private
    // process/module path. Lifecycle counters are NOT confirmed game frames.
    const auto m=metrics();
    const auto depthReport=QStringLiteral("Depth rejection first blocker (samplers not inspected): test disabled=%1; writes disabled=%2; NEVER=%3; ALWAYS=%4; missing attachment=%5; under 16 bits=%6; unspecified=%7\nNested API calls: inspected=%8; prepared=%9. Nested/alias calls are not additional game frames or proof of game-resource ownership.")
        .arg(m.value("textureDepthTestDisabled").toString(),m.value("textureDepthWritesDisabled").toString(),
             m.value("textureDepthNever").toString(),m.value("textureDepthAlways").toString(),
             m.value("textureDepthMissingAttachment").toString(),m.value("textureDepthInsufficientPrecision").toString(),
             m.value("textureDepthUnspecified").toString(),m.value("textureNestedInspected").toString(),m.value("textureNestedDraws").toString());
    const auto producerReport=QStringLiteral("Presentation-input observations: draws=%1; color bindings=%2. Resource-linked FBO/pbuffer producers: inspected=%3; prepared=%4; private bindings=%5. Immediate color transfer observations: calls=%6; linked inputs=%7. GL texture/RBO names require the same or observed-successfully-shared namespace. An exact recent native pbuffer color link from a successful image binding or captured copy can reach its nonsharing renderer without merging GL names; pbuffer existence alone is not ownership. Unknown/foreign/protected routes stay native. Bindings/submitted copies/native image links are not measured texture pixels or FPS.")
        .arg(m.value("textureConsumerDraws").toString(),m.value("textureConsumerInputs").toString(),
             m.value("textureProducerInspected").toString(),m.value("textureProducerDraws").toString(),m.value("textureProducerBindings").toString(),
             m.value("textureTransferCalls").toString(),m.value("textureTransferInputs").toString())
        +QStringLiteral("\nLatest resource-linked producer inspection: %1")
             .arg(m.value("textureProducerStatus").toString().isEmpty()?QStringLiteral("no detail recorded since the latest setting/source change"):m.value("textureProducerStatus").toString());
    const auto captureReport=QStringLiteral("Pre-route GL hook coverage since the latest setting/source change: draws=%1; transfers=%2 (named image copies=%3; native image bindings=%4). These are process-wide non-injected callback entries while Potato and a selected GL game source are active, NOT owned/inspected draws, qualified transfers, successful pixels or FPS. Foreign routes and nested/alias entries can be included; zero qualified transfers does not mean no native copy was seen.")
        .arg(m.value("textureHookDrawCalls").toString(),m.value("textureHookTransferCalls").toString(),
             m.value("textureHookImageCopyCalls").toString(),m.value("textureHookNativeImageCalls").toString())
        +QStringLiteral("\nCopy/blit routing ledger (native image bindings excluded): APIs: %1\nTerminal outcomes: %2\nProvider-candidate checks: %3\nDescriptor refusals: %4\nCode-target provider masks (NOT resource ownership): %5\nOne outcome per completed audited callback; candidate checks may overlap across shared-code providers. In-flight/reset-stale callbacks can be absent. Recorded inputs are submitted metadata, NOT driver success, changed pixels or a working Potato effect. No extra diagnostic GL query, raw handle/name, persistent log or upload.")
             .arg(auditLine(m.value("textureTransferApis")),auditLine(m.value("textureTransferStops")),
                  auditLine(m.value("textureTransferCandidates")),auditLine(m.value("textureTransferDescriptors")),
                  auditLine(m.value("textureTransferCodeProviders")))
        +QStringLiteral("\nObserve-call diagnostics (outermost callbacks): stop reasons: %1\nHandler entries: %2; reached observe: %3; not reached = entries minus reached. The not-observed stop is a caller-side category, not an observe refusal.")
             .arg(auditLine(m.value("textureTransferObserveStops")),
                  m.value("textureTransferHandlerEntries").toString(),m.value("textureTransferObserveReached").toString())
        +QStringLiteral("\nDeferred copy-subimage reconciliation: edges=%1; linked inputs=%2. Awaiting-exact-consumer callbacks are NOT ownership or qualified links. Only later exact consumption in the same/observed-share namespace can reconcile submitted metadata, within the ORIGINAL two-frame capture deadline and eight-hop bound. Reconciliation is separate from native call totals and NOT proof of driver success, private bindings or game pixels.")
             .arg(m.value("textureDeferredCopies").toString(),m.value("textureDeferredInputs").toString())
        +QStringLiteral("\nPending-copy lifetime ledger (same source/settings interval): queued=%1; pending=%2; peak=%3; balanced=%4.\nPending retirements (one reason per removed queued edge): %5\nSelected default-FBO consumer checks: %6\nUncaptured-output retirement sites: %7\nQueued copy source kinds: %8")
             .arg(m.value("textureDeferredQueued").toString(),m.value("textureDeferredPending").toString(),m.value("textureDeferredPeak").toString(),
                  m.value("textureDeferredAuditBalanced").toBool()?QStringLiteral("yes"):QStringLiteral("no"),
                  auditLine(m.value("textureDeferredExits")),auditLine(m.value("textureDeferredConsumers")),
                  auditLine(m.value("textureDeferredDiscardSites")),auditLine(m.value("textureDeferredSources")))
        +QStringLiteral("\nCopy exits: expired=%1; retired for other reasons=%2.")
             .arg(m.value("textureDeferredExpired").toString(),m.value("textureDeferredRetiredOther").toString())
        +QStringLiteral("\nCPU-only copy/presentation correlation (typed names compared only in the same observed namespace; recent window=%1 provenance frames): presented input observations=%2; distinct tracked inputs=%3; untracked input observations=%4; copy observations=%5.")
             .arg(static_cast<qulonglong>(GlDeferredCopyAudit::correlationWindowFrames))
             .arg(m.value("texturePresentedInputObservations").toString(),m.value("texturePresentedInputDistinct").toString(),
                  m.value("texturePresentedInputUntracked").toString(),m.value("textureCopyObservations").toString())
        +QStringLiteral(" Copies matching recent presented inputs: input=%1; output=%2. Presentation inputs matching recent copy names: copy-input=%3; copy-output=%4. Distinct tracked copy outputs=%5; untracked copy-input observations=%6; untracked copy-output observations=%7.")
             .arg(m.value("textureCopyInputRecentPresentationMatches").toString(),m.value("textureCopyOutputRecentPresentationMatches").toString(),
                  m.value("texturePresentationRecentCopyInputMatches").toString(),m.value("texturePresentationRecentCopyOutputMatches").toString(),
                  m.value("textureCopyOutputDistinct").toString(),m.value("textureCopyInputUntracked").toString(),
                  m.value("textureCopyOutputUntracked").toString())
        +QStringLiteral(" Frames with copy outputs=%1; most-used output seen in distinct copy-output frames=%2. No names or handles are reported; capacities are bounded at 32 presentation names and 16 output names.")
             .arg(m.value("textureCopyOutputFrames").toString(),m.value("textureCopyOutputMaxFramesPerName").toString())
        +QStringLiteral("\nReconciliation retry bounds: hops=%1; resource-budget=%2. Retries can repeat for one pending edge; they are not retirements or extra native calls. Queued = pending + all retirements within one interval. Source/settings/Off reset the whole ledger; snapshots can exclude in-flight events. Consumer checks compare destinations ONLY in the same/observed-share namespace; other namespaces is not an equal-name match. This only explains existing metadata retention: no extra GL query, raw identity, readback, ownership, driver-success or game-material proof, and no relaxed expiry/retirement check.")
             .arg(m.value("textureDeferredHopBlocked").toString(),m.value("textureDeferredResourceBlocked").toString());
    const auto inspectionReport=QStringLiteral("Texture inspection since the latest setting/source change: calls=%1; sampler-inspection calls=%2; conservative depth bypass=%3; blended bypass=%4; no eligible mip sampler=%5; other bypass=%6\nLatest bypass: %7\nOffscreen/pbuffer/foreign scene contexts are not inferred from presentation or a share group.")
        .arg(m.value("textureInspected").toString(),m.value("textureEligibleDraws").toString(),
             m.value("textureDepthBypasses").toString(),m.value("textureBlendBypasses").toString(),
             m.value("textureSamplerBypasses").toString(),m.value("textureOtherBypasses").toString(),
             m.value("textureLastBypass").toString())+QLatin1Char('\n')+depthReport+QLatin1Char('\n')+producerReport+QLatin1Char('\n')+captureReport;
    const auto policyReport=QStringLiteral("Texture sampling policy: %1. Compatibility inspects mipmapped color without a depth-test requirement; opaque mipmapped in-game HUD may also change, as may RGB-packed data maps. Saved Potato strength: %2% (0 = native, 100 = up to 3 mip levels). Strength does not make rejected resources eligible; prepared bindings are not measured game texture changes.")
        .arg(m.value("texturePolicy").toString(),m.value("potatoStrength").toString());
    const auto build=bridgeInfo();
    return QStringLiteral("Native shader module: %1; capture=%2\n")
        .arg(build.value("moduleBuildRevision").toString(),build.value("moduleCaptureVersion").toString())
        +QStringLiteral("Requested API: %1\nObserved game API: %2\n%3\nProcessed game frames: %4\nGame surface observed: %5; processing active: %6\nVulkan capture: loaders=%7; instances=%8; devices=%9; Win32 surfaces=%10; swapchains=%11; presents=%12; missing queues=%13; excluded windows=%14\nLast hooked loader: %15\nQt bridge: %16; host: %17\n%18")
        .arg(m.value("requestedRenderer").toString(),m.value("renderer").toString(),m.value("status").toString())
        .arg(m.value("processedFrames").toString(),m.value("gameSurfaceObserved").toBool()?QStringLiteral("yes"):QStringLiteral("no"),m.value("active").toBool()?QStringLiteral("yes"):QStringLiteral("no"))
        .arg(m.value("vulkanLoaders").toString(),m.value("vulkanInstances").toString(),m.value("vulkanDevices").toString(),m.value("vulkanSurfaces").toString(),m.value("vulkanSwapchains").toString(),m.value("vulkanPresents").toString(),m.value("vulkanMissedQueues").toString(),m.value("vulkanRejectedWindows").toString(),m.value("vulkanLoader").toString())
        .arg(QString::fromLatin1(QT_VERSION_STR),QString::fromLatin1(qVersion()),
             QStringLiteral("Configured emulator renderer (hint only): %1\nDetection uses owned host game presentation, not the Android guest API or Qt UI.\n%2")
                 .arg(m.value("configuredRenderer").toString(),
                      QStringLiteral("Prepared Potato sampler bindings: %1 across %2 draw calls; inspection armed: %3 (not measured game pixels/FPS)\n%4\n%5\n%6")
                          .arg(m.value("textureBindings").toString(),m.value("textureDraws").toString(),
                               m.value("textureSupported").toBool()?QStringLiteral("yes"):QStringLiteral("no"),
                               m.value("textureStatus").toString(),
                               policyReport+QLatin1Char('\n')+inspectionReport,error())));
}
void ShaderBridge::copyDiagnostics()const{if(auto *clipboard=QGuiApplication::clipboard())clipboard->setText(diagnostics());}
void ShaderBridge::attach(QObject *object,const QString &instance,const QString &graphicsRenderer){
    if(!QCoreApplication::instance()||QThread::currentThread()!=QCoreApplication::instance()->thread())return;
    auto *window=qobject_cast<QWindow*>(object);
    auto &s=state();
    if(!window||instance.trimmed().isEmpty()||instance.size()>128){s.error=QStringLiteral("The native player window/instance could not be identified. Shaders remain disabled.");notify();return;}
    if(s.attached&&s.instance!=instance){s.error=QStringLiteral("A different instance is already attached in this process; shaders remain unchanged.");notify();return;}
    s.instance=instance;s.error.clear();s.preferences.load(fileFor(instance));
    auto requested=RendererFamily::Automatic;
    if(graphicsRenderer==QLatin1String("gl"))requested=RendererFamily::OpenGL;
    else if(graphicsRenderer==QLatin1String("dx"))requested=RendererFamily::DirectX;
    else if(graphicsRenderer==QLatin1String("vlcn"))requested=RendererFamily::Vulkan;
    // Unknown/new combined settings must not disable capture either: only an
    // owned game presentation can identify the processing backend. Keep the
    // setting for diagnostics; never call another API "Vulkan" to hide a miss.
    s.attached=true;Runtime::instance().configureRenderer(requested);Runtime::instance().set(s.preferences.parameters());Runtime::instance().setTexture(s.preferences.texture());
    if(s.surfaceObserver)delete s.surfaceObserver;
    s.surfaceObserver=new SurfaceObserver(window);
    Runtime::instance().start(window->handle()?std::uintptr_t(window->winId()):0);
    if(!s.connected){s.connected=true;QObject::connect(QCoreApplication::instance(),&QCoreApplication::aboutToQuit,[] {Runtime::instance().stop();});}
    notify();
}
QVariantMap ShaderBridge::apply(const QVariantMap &values,bool keepCurrentDisplay){
    return applyConfiguration(values,keepCurrentDisplay,{});
}
QVariantMap ShaderBridge::applyReviewed(const QVariantMap &values,const QString &reviewKey){
    if(reviewKey.size()!=64)return {{"ok",false},{"msg",QStringLiteral("A current Display preview is required before confirming the restart dialog. Nothing was saved or restarted.")}};
    return applyConfiguration(values,false,reviewKey);
}
QVariantMap ShaderBridge::applyConfiguration(const QVariantMap &values,bool keepCurrentDisplay,const QString &reviewKey){
    auto &s=state();
    if(!QCoreApplication::instance()||QThread::currentThread()!=QCoreApplication::instance()->thread())
        return {{"ok",false},{"msg",QStringLiteral("Shader settings must be saved on the player GUI thread.")}};
    if(!s.attached){s.error=QStringLiteral("No native player instance is attached.");notify();return {{"ok",false},{"msg",s.error}};}
    auto response=s.preferences.apply(values,keepCurrentDisplay,reviewKey);
    if(response.value(QStringLiteral("ok")).toBool()){s.error.clear();Runtime::instance().set(s.preferences.parameters());Runtime::instance().setTexture(s.preferences.texture());}
    notify();return response;
}
} // namespace BsiShaders
