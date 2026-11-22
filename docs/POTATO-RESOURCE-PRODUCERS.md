# Potato: resource-linked color-FBO producers

## Newest installed result and native-image follow-up

The saved-strength candidate still has **2,107 grading frames / 1,456 NoSampler /
zero prepared bindings, linked producer inspections or captured transfers**.
Its inspected image is mutable RGB 256x256, first refused at level 1; the prior
immutable repair is not its diagnosis. The [native image binding follow-up](POTATO-NATIVE-IMAGE-BINDINGS.md)
corrects effective-filter reporting and adds actual successful render-to-texture
links without a GL copy/blit. Native lifetime/color links can cross nonsharing
contexts without merging their GL object names. The older evidence/limits below
remain historical, not a claim that the new game result worked.

## Latest result and follow-up

After the subsequent `ab8b27e` transfer/pbuffer candidate the latest game report
is **1,812 grading frames, 3,624 sampler inspections/all NoSampler, 3,622 selected
inputs, TWO linked producer inspections but zero replacements, and zero qualified
transfers**. This is still a negative texture result. The [current mip qualification
repair](POTATO-MIP-QUALIFICATION.md) reproduces and fixes a valid immutable-storage
false rejection; it does not establish this game's storage/path or bypass the
resource proof. Producer detail now survives later presentation refusals.

The first resource-producer candidate, `e68e030`, passed shader/software-pixel
and Windows hook/Qt CI. The next installed report nevertheless has **1,794
grading frames, 1,657 sampler inspections/NoSampler, 1,657 presentation-input
draws/bindings, and ZERO linked producer inspections/preparations**. Its latest
bound sampler is rejected by the combined mip/color/protection bucket, not by
descriptor planning. No absent-mip, depth, stale-install or texture-success
diagnosis follows. The [color-transfer/pbuffer repair](POTATO-COLOR-TRANSFERS.md)
adds supported missing output/transfer paths; this page retains the original
texture-FBO mechanism and its separately tested evidence.

## Failure originally addressed, not an invented game diagnosis

The preceding report had **1,326 processed OpenGL grading frames,
2,652 depth-independent sampler inspections, 2,652 NoSampler, zero prepared
bindings/draws**, and no depth/blend/other or nested activity. Grading worked;
texture Potato did not. The old depth gate was no longer the reported blocker.
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
   In the first repair, only another captured drawable on the **same selected
   game HWND**, or an **observed unprotected EGL pbuffer**, was a candidate.
   The current [transfer follow-up](POTATO-COLOR-TRANSFERS.md) additionally
   recognizes exact successful WGL pbuffer DC acquisitions (including driver
   hidden-window DCs) and typed renderbuffer/default-pbuffer dependencies reached
   through an actual submitted color copy/resolve. None can seed its own
   ownership. Unknown window/DC/surfaceless routes, other APIs/processes and
   unobserved sharing remain native. Protection is queried legally before
   optional GL initialization; protected textures never seed roots. Unqueryable
   attachment protection fails closed for transfers. Other MRT outputs are not
   authorized using buffer zero's write mask.
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

Creation/sharing/deletion callbacks do not wait on the render mutex. They
publish **32 fixed events**, consumed under the render lock. The follow-up
separates lost object retirement from lost context topology: deletion/retirement
contention, oversized lists or overflow clear resource evidence while retaining
already observed successful sharing. Missing context creation/share events
still forget namespaces as well, since handle reuse may have been missed. No
unbounded queue or name-based reconstruction is used. Captures require the
current serial to equal the last **drained** lifecycle serial, not merely an
unprocessed newer snapshot. Changes during observation/original transfer prevent
it from becoming new evidence.
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
  execution**: that first repair subsequently passed all six jobs in
  [shader run 44974822410](https://github.com/dsajgddghdsaakovvgheww/BSI-Customizer/actions/runs/44974822410),
  including actual Windows hooks/lifecycle/Qt fixtures and Mesa desktop/GLES
  production pixels. The current transfer follow-up requires its own exact-head
  execution; historical CI is not a pass for a later patch.

No original DLL payload was redistributed or blindly deployed. No installed
Free Fire texture change, exact in-game Off, physical Intel/AMD/NVIDIA matrix,
FPS gain, allocation saving, universal API coverage or account safety is
established. PR121 remains draft, unmerged and unreleased. This is a tested
behavioral candidate for the capture gap, not a declaration that the latest
installed-player failure is solved.
