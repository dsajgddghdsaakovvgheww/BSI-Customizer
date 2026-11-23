#pragma once
#include "Parameters.h"
#include "RendererCapabilities.h"
#include "VulkanCapture.h"
#include "TextureQuality.h"
#include "GlTransferAudit.h"
#include <array>
#include <cstdint>
#include <mutex>
#include <string>

namespace BsiShaders {
// Pre-routing hook coverage only, NEVER ownership or successfully copied pixels.
enum class TextureHookKind { Draw, ColorTransfer, NamedImageCopy, NativeImageBinding };
struct Metrics {
    bool hooks=false, observed=false, active=false, deviceReady=false;
    Renderer renderer=Renderer::Unknown;
    RendererFamily requested=RendererFamily::Automatic;
    // Configuration is a hint, not proof of the host's game presentation API.
    RendererFamily configured=RendererFamily::Automatic;
    std::uint64_t processed=0;
    VulkanCapture vulkan;
    bool textureSupported=false;
    TextureDrawPolicy texturePolicy=TextureDrawPolicy::DepthQualifiedScene;
    // Since the latest texture setting/presentation-source change, not lifetime
    // success carried over from a different game context or an earlier toggle.
    // textureSceneDraws is a retained internal name: compatibility counts
    // calls reaching sampler inspection, NOT proof of semantic 3D scene use.
    std::uint64_t textureDraws=0,textureBindings=0,textureInspected=0,textureSceneDraws=0;
    std::uint64_t textureDepthBypasses=0,textureBlendBypasses=0,textureSamplerBypasses=0,textureOtherBypasses=0;
    std::array<std::uint64_t,static_cast<std::size_t>(TextureDepthFailure::Count)> textureDepthReasons{};
    std::uint64_t textureNestedInspected=0,textureNestedDraws=0;
    // Resource-link observations, not pixel consumption or successful effects.
    std::uint64_t textureConsumerDraws=0,textureConsumerInputs=0;
    std::uint64_t textureProducerInspected=0,textureProducerDraws=0,textureProducerBindings=0;
    std::uint64_t textureTransferCalls=0,textureTransferInputs=0;
    // Asynchronous reconciliation of submitted copies, NOT extra native calls
    // or private bindings. Kept separate from synchronous transfer observations.
    std::uint64_t textureDeferredCopies=0,textureDeferredInputs=0;
    // Non-injected hook entries anywhere in this process while a GL game source
    // is selected and Potato is active. May include aliases/foreign routes.
    std::uint64_t textureHookDrawCalls=0,textureHookTransferCalls=0,textureHookImageCopyCalls=0,textureHookNativeImageCalls=0;
    GlTransferAudit textureTransferAudit;
    std::string textureLastBypass;
    // Retain the actual producer's detail across later compositor inspections.
    // A final presentation-only refusal must not hide the upstream blocker.
    std::string textureProducerStatus;
    std::string textureStatus="Potato graphic is off; native texture sampling retained.";
    std::string status="Waiting for the game's presentation API. Multi-renderer support is not complete.";
};
// One mutex-consistent routing/settings snapshot. A draw cannot select a source
// itself. Revision changes on every settings/source reset, including Off->On.
struct TextureRoute {
    TextureConfiguration configuration;
    Renderer renderer=Renderer::Unknown;
    std::uintptr_t object=0,window=0;
    std::uint64_t revision=0;
};
// One state per HD-Player process; Qt objects never cross the render-thread
// boundary. The settings bridge copies bounded values through this mutex.
class Runtime {
public:
    static Runtime &instance();
    void start(std::uintptr_t playerWindow);
    void setWindow(std::uintptr_t playerWindow);
    void stop();
    void set(const Parameters &settings);
    // Explicit family selection is retained for isolated backend qualification.
    void selectRenderer(RendererFamily requested);
    // The player always uses automatic, presentation-proven routing. Its
    // graphics setting is retained separately for diagnostics, never enforced.
    void configureRenderer(RendererFamily configured);
    bool accepts(Renderer observed);
    // Call only inside an ownership-checked real presentation callback, BEFORE
    // touching pixels. Atomic arbitration prevents concurrent/nested providers
    // (including translation layers in the same API family) grading twice.
    bool claimPresentation(Renderer renderer,std::uintptr_t object,std::uintptr_t window);
    // A draw hook may use an already selected source, NEVER claim a route from
    // a draw/context alone. Also authorizes a scene context on its exact drawable.
    bool isPresentation(Renderer renderer,std::uintptr_t object,std::uintptr_t window);
    void releasePresentation(Renderer renderer,std::uintptr_t object);
    Parameters settings();
    void setTexture(const TextureConfiguration &settings);
    TextureConfiguration texture();
    TextureRoute textureRoute();
    // Replaces the callback's active-setting read (one mutex acquisition), and
    // separately counts pre-route coverage when a GL presentation is selected.
    bool textureHookEntry(TextureHookKind kind,std::uint64_t *revision=nullptr);
    void textureTransferAuditReport(const GlTransferAttempt &attempt);
    void textureConsumerReport(unsigned inputs,std::uint64_t revision);
    void textureTransferReport(unsigned inputs,std::uint64_t revision);
    void textureDeferredCopyReport(unsigned copies,unsigned inputs,std::uint64_t revision);
    void textureReport(unsigned bindings,const std::string &detail,bool supported,
                       TextureDrawReason reason=TextureDrawReason::NotInspected,
                       TextureDepthFailure depthFailure=TextureDepthFailure::None,bool nested=false,
                       bool producer=false,std::uint64_t revision=0);
    Metrics metrics();
    void waiting(bool hooks,const std::string &status);
    void vulkanEvent(VulkanEvent event,const std::string &detail={});
    std::string gameWindowDiagnostic(std::uintptr_t surface);
    void report(bool hooks,bool observed,bool active,const std::string &status,bool processed=false,
                Renderer renderer=Renderer::Unknown,bool deviceRequirementsMet=false);
    // Identity without a size gate: a game child may still be 1x1 while its
    // renderer creates a full-size swapchain, before Qt finishes layout.
    bool ownsGameWindow(std::uintptr_t surface);
    bool eligible(std::uintptr_t surface);
private:
    void resetTextureEvidence(); // caller holds mutex
    std::mutex mutex;
    Parameters current;
    TextureConfiguration currentTexture;
    Metrics state;
    std::uintptr_t host=0,presentationObject=0,presentationWindow=0;
    std::uint64_t textureRevision=0;
};
void startPresentationHooks();
void stopPresentationHooks();
} // namespace BsiShaders
