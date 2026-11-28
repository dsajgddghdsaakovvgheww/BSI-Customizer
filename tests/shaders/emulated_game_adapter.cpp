// C-ABI adapter that lets the authored emulated-game harness (Python + EGL)
// drive the REAL production Potato objects. Nothing is re-implemented here:
// every export forwards to BsiShaders::GlTextureOverride or
// BsiShaders::GlTextureProvenance, exactly as the Windows hooks do.
//
// Not part of the deployed module. POSIX-only because the harness renders
// through a software EGL driver instead of the player's Win32 presentation.
#ifndef _WIN32

#include "GlProducerRoute.h"
#include "GlTextureOverride.h"
#include "GlTextureProvenance.h"
#include "TextureQuality.h"

#include <cstring>
#include <memory>
#include <string>

namespace {
std::unique_ptr<BsiShaders::GlTextureOverride> potato;
std::unique_ptr<BsiShaders::GlTextureProvenance> provenance;
unsigned privateBindings = 0;
constexpr BsiShaders::GlContextIdentity gameContext{1, 1, 1};

void copyMessage(const std::string &text, char *message, int capacity) {
    if (!message || capacity <= 0) return;
    std::strncpy(message, text.c_str(), std::size_t(capacity - 1));
    message[capacity - 1] = 0;
}
}  // namespace

extern "C" {

// Same resolver contract as GlTextureOverride::initialize (ResolveGl).
int bsi_game_potato_initialize(void *(*resolve)(const char *, void *), void *user, char *message, int capacity) {
    if (potato) potato->release();
    potato = std::make_unique<BsiShaders::GlTextureOverride>();
    provenance = std::make_unique<BsiShaders::GlTextureProvenance>();
    privateBindings = 0;
    std::string error;
    const bool okay = potato->initialize(resolve, user, &error, false);
    copyMessage(error, message, capacity);
    return okay ? 1 : 0;
}

// Model Runtime::claimPresentation() for this fixture: the harness presents
// from the default framebuffer of its own context. On Windows this is gated by
// Runtime::ownsGameWindow() (process, child-HWND, class and size checks).
int bsi_game_claim_presentation(char *message, int capacity) {
    const auto route = BsiShaders::glProducerRoute(true, false, false, false, false);
    if (route == BsiShaders::GlProducerRoute::Refuse) {
        copyMessage("default framebuffer route refused", message, capacity);
        return 0;
    }
    provenance->synchronize(1, 1);
    copyMessage("selected", message, capacity);
    return 1;
}

// Read-only dependency capture of the game's current draw, through production
// usage() + observe(). Returns the number of color inputs the frame consumes.
unsigned bsi_game_observe_compositor(char *message, int capacity) {
    BsiShaders::GlTextureUsage usage;
    if (!potato || !potato->usage(&usage)) {
        copyMessage("the current game draw is not a captureable color write", message, capacity);
        return 0;
    }
    const unsigned inputs = provenance->observe(gameContext, usage, true);
    if (inputs) copyMessage("compositor consumption observed", message, capacity);
    return inputs;
}

// The draw-scoped replacement. `policy`: 0 = depth-qualified scene, 1 = owned-mip
// compatibility. Returns the number of borrowed private samplers (0 = native).
unsigned bsi_game_begin(int mode, int reviewed, unsigned primitive, int policy, int strength, char *message,
                        int capacity) {
    if (!potato) return 0;
    std::string detail;
    const auto count = potato->begin(static_cast<BsiShaders::TextureQuality>(mode), reviewed != 0, primitive, &detail,
                                     nullptr, nullptr,
                                     policy ? BsiShaders::TextureDrawPolicy::OwnedMipCompatibility
                                            : BsiShaders::TextureDrawPolicy::DepthQualifiedScene,
                                     strength);
    privateBindings += count;
    copyMessage(detail, message, capacity);
    return count;
}

void bsi_game_end() {
    if (potato) potato->end();
}

unsigned bsi_game_private_bindings() { return privateBindings; }

void bsi_game_next_frame() {
    if (provenance) provenance->nextFrame();
}

void bsi_game_release() {
    if (potato) potato->release();
    potato.reset();
    provenance.reset();
    privateBindings = 0;
}

}  // namespace

#endif
