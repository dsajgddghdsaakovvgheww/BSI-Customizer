# Potato: resource-linked color-FBO producers

## Failure addressed, not an invented game diagnosis

The latest installed report has **1,326 processed OpenGL grading frames,
2,652 depth-independent sampler inspections, 2,652 NoSampler, zero prepared
bindings/draws**, and no depth/blend/other or nested activity. Grading works;
texture Potato does not. The old depth gate is no longer the reported blocker.
See [renderer evidence](FREE-FIRE-RENDERER-ANALYSIS.md).

`NoSampler` combined many refusals. The report does not prove a missing mip
chain, an offscreen renderer, complete draw coverage or any permission to mutate
another context. This repair addresses an **independently reproducible** gap,
not an established installed-game root cause:

```text
material sampler2D + existing mips -> color texture T in an FBO
                                     |
selected game compositor <- single-level sampler2D of T
```

The compositor correctly has no coarser mip to select. Previously, if the FBO
producer used another drawable, its material samplers could never qualify,
**even in the same GL context**. An active presentation shader cannot repair
that by blurring the finished image or changing resolution.

## Production behavior

This extends the existing explicit `OwnedMipCompatibility` policy. No new UI,
preference key, driver profile, resolution write or restart is introduced.
`DepthQualifiedScene` retains its original exact-drawable control behavior.

1. **Keep real presentation arbitration.** Only a qualified owned swap selects
   the provider/display/game window and source. A draw/texture/pbuffer cannot
   claim a presentation. The root/player UI, Qt/Chrome classes and foreign
   windows remain excluded.
2. **Observe bindings, not pixels.** On unblended triangle submissions on that
   exact selected drawable, inspect active ordinary `sampler2D` uniforms and
   normalized RGB/RGBA inputs, including single-level textures. Read-only
   metadata inspection restores `ACTIVE_TEXTURE`; it never binds a source
   texture, allocates a GL object, reads pixels or consumes `glGetError`.
   Only default-framebuffer draws with buffer-0 color writes can seed roots.
   Active bindings are resource-link evidence, **not proof the shader actually
   sampled a texel, the draw rasterized, or game output changed**.
3. **Do not guess namespaces.** Numeric texture names are local identities.
   A separate context must be linked by an observed **successful native**
   `eglCreateContext(..., share, ...)`, `wglShareLists`, or
   `wglCreateContextAttribsARB(..., share, ...)`. Failed calls never join it.
   EGL displays/providers are independent. Pre-existing/unobserved sharing is
   not reconstructed from equal object names. A namespace merge clears old
   texture evidence and needs a fresh selected consumer.
4. **Require an output dependency too.** A nonselected-drawable candidate must
   write an exact, recently observed input texture from that namespace, through
   draw buffer zero of its current color FBO. Sharing alone is insufficient.
   Only another captured drawable on the **same selected game HWND**, or an
   **observed unprotected EGL pbuffer**, is a candidate. Unknown/pre-existing
   window surfaces, unmapped/surfaceless routes, unknown WGL DCs, other HWNDs,
   other APIs/processes, default framebuffer outputs, renderbuffers and
   unobserved relationships remain native. Protection is queried legally
   before optional GL initialization; protected textures never seed roots.
   Other MRT outputs are not authorized using buffer zero's write mask.
5. **Apply at the material draw, not the compositor.** A proven producer uses
   the existing complete-mip/color/descriptor/protection/driver-review checks.
   Only its qualifying input samplers get private queried/bounded desktop
   bias+floor or legal GLES floor-only descriptors. A one-level source still
   gets no replacement. No application texture/sampler/storage is edited,
   no mipmaps are generated and no allocations/resolution are reduced.
6. **Restore the exact borrowed bindings.** The native draw runs once with
   unchanged arguments; restoration precedes lock/recursion lease release.
   Zero-binding wrappers still release before their original so a later nested
   real draw can be inspected. Exceptions fail closed with pre-original
   active-unit/partial-binding rollback. No CPU/GPU command buffer is read.

## Bounded evidence and lifecycle

`GlTextureProvenance.h` is an allocation-free policy, serialized by the existing
render mutex: **64 contexts, 128 dependency objects, eight producer hops and a
two-present-frame root lifetime**. Propagated input evidence inherits the
root's expiry; producer chains/cycles cannot renew themselves. A setting/review/
policy or selected-source revision clears all texture evidence, including an
Off->On cycle which returns to identical settings and handles. Late/stale
reports cannot resurrect an old revision's counters.

Deletion of a tracked texture invalidates that provider's root **and inherited**
links before a later draw can reuse the numeric name. Context destruction revokes
its namespace identity; last-context destruction cannot revive by handle reuse.
Native sharing/creation and texture-delete detours preserve return values and
arguments. Texture deletion tracing must install successfully before dependency
capture is enabled. GL deletion uses the same public dispatch targets the caller
resolves; it is not a complete interop/driver-internal object-lifetime oracle.

New creation/sharing/deletion callbacks do not wait on the render mutex. They
publish **32 fixed events**, consumed under the render lock. Queue contention,
oversized deletion lists or overflow fail closed by forgetting that provider's
namespaces/dependencies; no unbounded queue or name-based reconstruction is used.
A lifecycle change during an observation prevents it from becoming new evidence.
This relies on the host's normal GL shared-object synchronization; no extra
cross-context GPU fences, context migration, ownership by share-group membership,
or reconstruction of unobserved past events is claimed.

Copy rendering status now separates **presentation input observations** from
**resource-linked producer inspected/prepared/binding counts**. These are not
pixels, FPS, semantic albedo classification or additional graded frames. The
existing NoSampler status also reports sampler2D slots, bound objects, feedback,
ineligible mip/color/protection, sampler-policy and private-sampler refusals;
zero output is not relabelled a successful effect.

## Validation executed for this repair

- Production GLES 3.1 on **ANGLE/SwiftShader**, real authored pixels:
  - one-level compositor: **0 replacements** and a captured input texture;
  - no consumer evidence: **0** producer replacements;
  - matching FBO/material producer: **1** private replacement, real RGBA changes
    from **(255,0,0,173)** to **(255,255,0,173)** before composition;
  - unchanged compositor remains at **0** replacements but presents that change;
  - **entire presented Off frame equals native byte-for-byte**, not merely a
    center pixel. At unchanged 64x64 resolution the native/Strong SHA-256 values
    are `cdc6776c7860f67df2707bd0585c1c7eb02d91e4d09a70e890b39c59e3ea2fd0`
    and `032050fa57217bb47afbfc2e20c9b8c7fc718e7cf690f359fd90f536d6c0eeeb`;
  - revision/expiry/zero-color-mask refusals and fresh-consumer renewal pass.
- **Negative control:** disabling only production `producer()` authorization
  in a disposable source copy fails the expected upstream material-pixel
  assertion. Compilation, GL initialization, direct mip controls and consumer
  capture still run; a missing dependency/API is not counted as this negative.
- Existing real-pixel controls still pass: all ten presentation effects,
  **128 exact Off/source-state cycles** across both texture/sampler modes and
  both draw policies (64 depth-independent), native lambda 0/4, source alpha,
  full authored material detail reduction, no-depth FBO and pending errors.
- Portable CTest: **7/7**, including required real software Vulkan pixels;
  **ASan/UBSan/leak detection 7/7**. These are the locally available non-Qt
  tests, not a claim that the full Qt/Windows matrix executed locally.
- Production read-only API doubles exercise single-level capture without
  depth, exact output selection, feedback/protection/color-mask/buffer-NONE/
  blending exclusions and every observation allocation failure's restoration.
  Existing desktop/GLES sampler rollback, bias-capability and descriptor tests
  remain. The new allocation-free policy tests namespace isolation, successful
  sharing plus fresh consumption, unrelated outputs, inherited expiry/deletion,
  capacities/hop bounds and source/settings/context/texture teardown/reuse.
- Strict Windows x64 cross-compilation passes for the production hook/runtime
  and extended real-MinHook EGL lifecycle/GL dispatch-double tests. The Windows
  regression requires same-context and explicitly shared pbuffer producers,
  all 46 draw signatures/aliases, native once-only argument forwarding, private
  bindings **inside** the original, unrelated/protected/unmapped/Qt rejection,
  deletion, expiry, Off and selected-surface teardown. **Compilation is not
  execution**: the existing shader CI must execute this Windows contract and
  actual desktop/GLES pixels before those results may be claimed.

No original DLL payload was redistributed or blindly deployed. No installed
Free Fire texture change, exact in-game Off, physical Intel/AMD/NVIDIA matrix,
FPS gain, allocation saving, universal API coverage or account safety is
established. PR121 remains draft, unmerged and unreleased. This is a tested
behavioral candidate for the capture gap, not a declaration that the latest
installed-player failure is solved.
