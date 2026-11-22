"""Real GL/GLES pixels through production private-sampler code on owned resources.

The Windows detour/HWND contracts are separate; this is not emulator evidence.
"""
import ctypes as C
import hashlib
import json
import math
import os
from pathlib import Path
import re
import struct
import zlib

I, U, F, P = C.c_int, C.c_uint, C.c_float, C.c_void_p


def save_rgba(path, rgba, size):
    """Optional authored-fixture evidence only; no application image capture."""
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
    # GL rows are bottom-up. PNG rows are top-down.
    rows = b"".join(b"\0" + rgba[y * size * 4:(y + 1) * size * 4] for y in reversed(range(size)))
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0))
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(png + chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b""))


def exercise(gl, policy, desktop, size, resolver):
    version = gl("glGetString", C.c_char_p, U)(0x1F02).decode()
    match = re.search(r"(\d+)\.(\d+)", version)
    assert match, version
    actual = tuple(map(int, match.groups()))
    assert actual >= ((3, 3) if desktop else (3, 1)), "The production quality fixture requires GL 3.3/GLES 3.1 texture-level queries and private sampler objects; never skip-pass"
    policy.bsi_test_quality_minimum.argtypes = [I, I, I, U, F, F, F, I, C.POINTER(F)]
    policy.bsi_test_quality_minimum.restype = I

    def plan(mode, *, supported=1, mips=7, minimum=-1000, maximum=6, bias=0, reviewed=1):
        out = F(-99)
        changed = policy.bsi_test_quality_minimum(mode, not desktop, supported, mips, minimum, maximum, bias, reviewed, C.byref(out))
        return bool(changed), out.value

    assert plan(0) == (False, -1000)
    assert plan(1) == (True, 1.5) and plan(2) == (True, 3)
    assert plan(2, minimum=0) == (True, 3)
    assert plan(2, mips=2) == (True, 1)
    assert plan(2, mips=1) == (False, -1000)
    assert plan(2, supported=0) == (False, -1000), "GL/GLES without a private sampler path must bypass"
    assert plan(2, reviewed=0) == (False, -1000), "Unknown external profile is not qualified by GL queries"
    assert plan(2, minimum=.5) == (False, .5)
    assert plan(2, minimum=-2) == (False, -2)
    assert plan(2, bias=2) == (False, -1000), "Never stack a quality bias on an existing override"

    get_error = gl("glGetError", U)
    create_shader = gl("glCreateShader", U, U)
    shader_source = gl("glShaderSource", None, U, I, C.POINTER(C.c_char_p), C.POINTER(I))
    compile_shader = gl("glCompileShader", None, U)
    get_shader = gl("glGetShaderiv", None, U, U, C.POINTER(I))
    shader_log = gl("glGetShaderInfoLog", None, U, I, C.POINTER(I), P)
    create_program = gl("glCreateProgram", U)
    attach = gl("glAttachShader", None, U, U)
    link = gl("glLinkProgram", None, U)
    get_program = gl("glGetProgramiv", None, U, U, C.POINTER(I))
    delete_shader = gl("glDeleteShader", None, U)
    prefix = "#version 330 core\n" if desktop else "#version 300 es\nprecision highp float;\n"

    def shader(kind, body):
        obj = create_shader(kind)
        text = C.c_char_p((prefix + body).encode())
        shader_source(obj, 1, C.byref(text), None)
        compile_shader(obj)
        status = I()
        get_shader(obj, 0x8B81, C.byref(status))
        log = C.create_string_buffer(4096)
        shader_log(obj, len(log), None, log)
        assert status.value, log.value.decode()
        return obj

    vertex = shader(0x8B31, "out vec2 uv;uniform float sampleScale;void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);uv=p*sampleScale;gl_Position=vec4(p*2.-1.,0.,1.);}")
    fragment = shader(0x8B30, "in vec2 uv;uniform sampler2D sourceTexture;out vec4 color;void main(){color=texture(sourceTexture,uv);}")
    program = create_program()
    attach(program, vertex)
    attach(program, fragment)
    link(program)
    status = I()
    get_program(program, 0x8B82, C.byref(status))
    assert status.value, "mip-selection test program did not link"
    delete_shader(vertex)
    delete_shader(fragment)
    gl("glUseProgram", None, U)(program)
    location = gl("glGetUniformLocation", I, U, C.c_char_p)(program, b"sourceTexture")
    assert location >= 0
    gl("glUniform1i", None, I, I)(location, 0)
    scale_location = gl("glGetUniformLocation", I, U, C.c_char_p)(program, b"sampleScale")
    assert scale_location >= 0
    sample_scale = gl("glUniform1f", None, I, F)
    sample_scale(scale_location, 1)
    maximum_bias = F(0)
    if desktop:
        gl("glGetFloatv", None, U, C.POINTER(F))(0x84FD, C.byref(maximum_bias))
        assert math.isfinite(maximum_bias.value) and maximum_bias.value > 0, "desktop bias limit must be real, not assumed"
    vao = U()
    gl("glGenVertexArrays", None, I, C.POINTER(U))(1, C.byref(vao))
    gl("glBindVertexArray", None, U)(vao)
    gl("glActiveTexture", None, U)(0x84C0)
    texture = U()
    gl("glGenTextures", None, I, C.POINTER(U))(1, C.byref(texture))
    gl("glBindTexture", None, U, U)(0x0DE1, texture)
    upload = gl("glTexImage2D", None, U, I, I, I, I, I, U, U, P)
    colors = [(255, 0, 0, 173), (0, 255, 0, 173), (0, 0, 255, 173), (255, 255, 0, 173),
              (255, 0, 255, 173), (0, 255, 255, 173), (255, 255, 255, 173)]
    for level, rgba in enumerate(colors):
        extent = 64 >> level
        data = (C.c_ubyte * (extent * extent * 4)).from_buffer_copy(bytes(rgba) * (extent * extent))
        upload(0x0DE1, level, 0x8058, extent, extent, 0, 0x1908, 0x1401, data)
    gen_sampler = gl("glGenSamplers", None, I, C.POINTER(U))
    sampler_i = gl("glSamplerParameteri", None, U, U, I)
    sampler_f = gl("glSamplerParameterf", None, U, U, F)
    get_sampler = gl("glGetSamplerParameterfv", None, U, U, C.POINTER(F))
    bind_sampler = gl("glBindSampler", None, U, U)
    samplers = (U * 3)()
    gen_sampler(3, samplers)
    default_min = F()
    get_sampler(samplers[0], 0x813A, C.byref(default_min))
    assert default_min.value == -1000, "exercise the actual GL/GLES sampler default"
    for mode, sampler in enumerate(samplers):
        sampler_i(sampler, 0x2801, 0x2703)  # trilinear, the complete seven-level view
        sampler_i(sampler, 0x2800, 0x2600)
        sampler_i(sampler, 0x2802, 0x812F)
        sampler_i(sampler, 0x2803, 0x812F)
        sampler_f(sampler, 0x813A, plan(mode, minimum=default_min.value)[1])
        sampler_f(sampler, 0x813B, 6)

    for cap in (0x0BE2, 0x0B71, 0x0B90, 0x0B44, 0x0C11, 0x0BD0, 0x8C89):
        gl("glDisable", None, U)(cap)
    if desktop:
        gl("glDisable", None, U)(0x8DB9)
    gl("glColorMask", None, C.c_ubyte, C.c_ubyte, C.c_ubyte, C.c_ubyte)(1, 1, 1, 1)
    gl("glViewport", None, I, I, I, I)(0, 0, size, size)
    draw = gl("glDrawArrays", None, U, I, I)
    read = gl("glReadPixels", None, I, I, I, I, U, U, P)

    def render(sampler):
        bind_sampler(0, sampler)
        draw(0x0004, 0, 3)
        pixel = (C.c_ubyte * 4)()
        read(size // 2, size // 2, 1, 1, 0x1908, 0x1401, pixel)
        assert get_error() == 0, "quality proof left a GL API error"
        return tuple(pixel)

    native = render(samplers[0])
    assert native == colors[0], ("native mip", native)
    light, strong = render(samplers[1]), render(samplers[2])
    assert all(abs(a - b) <= 2 for a, b in zip(light, (0, 128, 128, 173))), ("light mip interpolation", light)
    assert strong == colors[3], ("strong mip", strong)
    assert render(samplers[0]) == native, "rebinding the original sampler must restore exact original pixels"
    original_min, original_max = F(), F()
    get_sampler(samplers[0], 0x813A, C.byref(original_min))
    get_sampler(samplers[0], 0x813B, C.byref(original_max))
    assert (original_min.value, original_max.value) == (-1000, 6), "quality plan mutated the original sampler"
    # Exercise the ACTUAL production draw-scoped implementation, not just the
    # descriptor planner or a Python replacement. Its changed sampler must be
    # bound during the real draw, then restore the original binding and pixels.
    policy.bsi_test_quality_initialize.argtypes = [type(resolver), P, C.c_char_p, I]
    policy.bsi_test_quality_initialize.restype = I
    policy.bsi_test_quality_begin.argtypes = [I, I, U, C.c_char_p, I]
    policy.bsi_test_quality_begin.restype = U
    policy.bsi_test_quality_begin_compatibility.argtypes = policy.bsi_test_quality_begin.argtypes
    policy.bsi_test_quality_begin_compatibility.restype = U
    policy.bsi_test_quality_begin_strength.argtypes = [I, I, U, I, I, C.c_char_p, I]
    policy.bsi_test_quality_begin_strength.restype = U
    message = C.create_string_buffer(2048)
    assert policy.bsi_test_quality_initialize(resolver, None, message, len(message)), message.value.decode()
    bind_sampler(0, samplers[0])
    gl("glEnable", None, U)(0x0B71)
    gl("glDepthMask", None, C.c_ubyte)(1)
    gl("glDepthFunc", None, U)(0x0203)  # LEQUAL: repeated real depth-tested scene draws
    if desktop:
        gl("glClearDepth", None, C.c_double)(1)
    else:
        gl("glClearDepthf", None, F)(1)
    gl("glClear", None, U)(0x0100)  # clear the test-owned depth attachment
    active = gl("glActiveTexture", None, U)
    get_int = gl("glGetIntegerv", None, U, C.POINTER(I))
    def get_binding(value):
        # GLES exposes SAMPLER_BINDING through the active unit's GetIntegerv,
        # not the desktop-only indexed query for this enum.
        active(0x84C0)
        get_int(0x8919, C.byref(value))
        active(0x84C3)
    active(0x84C3)
    native_binding = samplers[0]

    def production(mode, reviewed=1, primitive=0x0004, compatibility=False, expected_floor=None, strength=100):
        before, after = I(), I()
        get_binding(before)
        count = policy.bsi_test_quality_begin_strength(mode, reviewed, primitive, compatibility, strength, message, len(message))
        current = I()
        get_int(0x84E0, C.byref(current))
        assert current.value == 0x84C3, "inspection must restore ACTIVE_TEXTURE before the actual draw"
        get_binding(after)
        assert (after.value != before.value) == bool(count), message.value.decode()
        if count:
            floor = (1.5 if mode == 1 else 3) * strength / 100 if expected_floor is None else expected_floor
            actual_floor = F()
            get_sampler(after.value, 0x813A, C.byref(actual_floor))
            assert actual_floor.value == floor, "production private floor is bounded by the actual exposed view"
            if desktop:
                actual_bias = F()
                get_sampler(after.value, 0x8501, C.byref(actual_bias))
                assert actual_bias.value == min(floor, maximum_bias.value), "production private bias is bounded by the queried device limit AND actual exposed view"
        draw(primitive, 0, 3)
        policy.bsi_test_quality_end()
        get_binding(after)
        assert before.value == after.value == native_binding, "draw-scoped binding did not restore exactly"
        pixel = (C.c_ubyte * 4)()
        read(size // 2, size // 2, 1, 1, 0x1908, 0x1401, pixel)
        return count, tuple(pixel)

    assert production(0) == (0, native)
    assert production(2) == (1, colors[3]), message.value.decode()
    assert production(2, reviewed=0) == (0, native), "no driver review must leave original sampler/pixels untouched"
    count, actual_light = production(1)
    assert count == 1 and all(abs(a - b) <= 2 for a, b in zip(actual_light, (0, 128, 128, 173)))
    assert production(0) == (0, native), "switch off restores exact native pixels without recreating texture resources"
    gl("glDisable", None, U)(0x0B71)
    assert production(2) == (0, native), "depth-disabled/UI draws must stay untouched"
    gl("glEnable", None, U)(0x0B71)
    gl("glDepthFunc", None, U)(0x0207)
    assert production(2) == (0, native), "always-pass UI-like depth state is not scene qualification"
    gl("glDepthFunc", None, U)(0x0203)
    gl("glDepthMask", None, C.c_ubyte)(0)
    assert production(2) == (0, native), "non-depth-writing passes must stay untouched"
    gl("glDepthMask", None, C.c_ubyte)(1)
    gl("glEnable", None, U)(0x0BE2)
    assert production(2) == (0, native), "blended/UI draws must stay untouched"
    gl("glDisable", None, U)(0x0BE2)
    sampler_f(samplers[0], 0x813A, .5)
    assert production(2)[0] == 0, "existing sampler minimum-LOD overrides must not be stacked/reset"
    sampler_f(samplers[0], 0x813A, -1000)
    active(0x84C0)
    gl("glTexParameteri", None, U, U, I)(0x0DE1, 0x813D, 0)
    active(0x84C3)
    assert production(2) == (0, native), "a one-level view is not a reducible mip chain"
    active(0x84C0)
    gl("glTexParameteri", None, U, U, I)(0x0DE1, 0x813D, 1000)
    active(0x84C3)
    assert get_error() == 0, message.value.decode()

    # Native minification distinguishes an additive bias from a floor. Exercise
    # the ACTUAL production begin/draw/end for BOTH source sampling modes, not
    # just the descriptor adapter or a manually authored replacement sampler.
    active(0x84C0)
    texture_i = gl("glTexParameteri", None, U, U, I)
    texture_f = gl("glTexParameterf", None, U, U, F)
    for key, value in ((0x2801, 0x2703), (0x2800, 0x2600), (0x2802, 0x812F), (0x2803, 0x812F)):
        texture_i(0x0DE1, key, value)
    texture_f(0x0DE1, 0x813A, -1000)
    texture_f(0x0DE1, 0x813B, 6)
    active(0x84C3)

    def source_lods():
        state = []
        active(0x84C0)
        keys = (0x813A, 0x813B, 0x8501) if desktop else (0x813A, 0x813B)
        for key in keys:
            value = F()
            gl("glGetTexParameterfv", None, U, U, C.POINTER(F))(0x0DE1, key, C.byref(value))
            state.append(value.value)
            get_sampler(samplers[0], key, C.byref(value))
            state.append(value.value)
        active(0x84C3)
        return tuple(state)

    def interpolated(lod):
        lod = min(6, lod)
        lower, upper = math.floor(lod), min(6, math.ceil(lod))
        amount = lod - lower
        return tuple(round(a * (1 - amount) + b * amount) for a, b in zip(colors[lower], colors[upper]))

    saved_source_lods = source_lods()
    cycles = 0
    mip_evidence = []
    for compatibility in (False, True):
        # The real application now explicitly selects compatibility. Prove its
        # output changes with hardware depth testing AND writes disabled, while
        # retaining the independently tested conservative policy as a control.
        gl("glDisable" if compatibility else "glEnable", None, U)(0x0B71)
        gl("glDepthMask", None, C.c_ubyte)(not compatibility)
        depth_mask = C.c_ubyte()
        gl("glGetBooleanv", None, U, C.POINTER(C.c_ubyte))(0x0B72, C.byref(depth_mask))
        assert bool(gl("glIsEnabled", C.c_ubyte, U)(0x0B71)) == (not compatibility)
        assert bool(depth_mask.value) == (not compatibility)
        for native_binding in (samplers[0], 0):
            bind_sampler(0, native_binding)
            for scale, native_lod in ((1, 0), (16, 4)):
                sample_scale(scale_location, scale)
                assert production(0, compatibility=compatibility) == (0, colors[native_lod]), "test gradient must really select native mip 0/4"
                for mode, floor in ((1, 1.5), (2, 3)):
                    bias = min(floor, maximum_bias.value) if desktop else 0
                    expected = interpolated(max(floor, native_lod + bias))
                    for repeat in range(8):
                        count, observed = production(mode, compatibility=compatibility)
                        assert count == 1 and all(abs(a - b) <= 2 for a, b in zip(observed, expected)), ("actual production minification", desktop, compatibility, native_binding, scale, mode, observed, expected)
                        off = production(0, compatibility=compatibility)
                        assert off == (0, colors[native_lod]), "Off must restore exact pixels at BOTH gradients/source modes/policies"
                        if compatibility and repeat == 0:
                            mip_evidence.append({"source": "sampler" if native_binding else "texture", "lambda": native_lod,
                                                 "mode": mode, "rgba": observed, "off": off[1]})
                        assert source_lods() == saved_source_lods, "source texture/sampler LOD state must remain unchanged on every cycle"
                        cycles += 1
    assert cycles == 128
    sample_scale(scale_location, 1)
    native_binding = samplers[0]
    bind_sampler(0, native_binding)

    # The saved percentage drives REAL draw-scoped sampling, not final-frame
    # blur/resizing. Exercise both source modes and both native gradients.
    def strength_frame():
        image = (C.c_ubyte * (size * size * 4))()
        read(0, 0, size, size, 0x1908, 0x1401, image)
        return bytes(image)

    strength_pixels = []
    for native_binding in (samplers[0], 0):
        bind_sampler(0, native_binding)
        for scale, native_lod in ((1, 0), (16, 4)):
            sample_scale(scale_location, scale)
            assert production(0, compatibility=True) == (0, colors[native_lod])
            native_image = strength_frame()
            for percent in (0, 25, 50, 75, 100):
                floor = 3 * percent / 100
                bias = min(floor, maximum_bias.value) if desktop else 0
                expected = interpolated(max(floor, native_lod + bias))
                gl("glEnable", None, U)(0xDEADBEEF)
                count, observed = production(2, compatibility=True, strength=percent)
                actual_image = strength_frame()
                assert count == int(percent > 0) and all(abs(a - b) <= 2 for a, b in zip(observed, expected)), ("actual strength pixels", percent, observed, expected)
                assert get_error() == 0x0500 and get_error() == 0, "percentage changes/cache recycling must preserve pending application errors"
                assert production(0, compatibility=True) == (0, colors[native_lod]) and strength_frame() == native_image, "ENTIRE native Off image must restore at every saved percentage"
                assert (actual_image != native_image) == (observed != colors[native_lod])
                assert source_lods() == saved_source_lods and get_error() == 0, "percentage never edits source texture/sampler values"
                if native_binding and native_lod == 0:
                    strength_pixels.append(list(observed))
    assert len({tuple(rgba) for rgba in strength_pixels}) == 5, "five strengths must really produce five different sampled mip results, including exact native zero"
    sample_scale(scale_location, 1)
    native_binding = samplers[0]
    bind_sampler(0, native_binding)

    # A valid immutable texture need NOT allocate down to 1x1. MAX_LEVEL
    # remains its API default (1000); native sampling clamps to storage. The
    # old production inspector demanded the nonexistent tail and rejected it.
    extension_count = I()
    get_int(0x821D, C.byref(extension_count))
    extensions = {gl("glGetStringi", C.c_char_p, U, U)(0x1F03, n).decode() for n in range(extension_count.value)}
    assert not desktop or actual >= (4, 2) or "GL_ARB_texture_storage" in extensions, "this software regression requires real immutable texture storage"
    view_entry = ("glTextureView" if actual >= (4, 3) or "GL_ARB_texture_view" in extensions else None) if desktop else (
        "glTextureViewOES" if "GL_OES_texture_view" in extensions else "glTextureViewEXT" if "GL_EXT_texture_view" in extensions else None)
    count_query = not desktop or view_entry is not None
    storage = gl("glTexStorage2D", None, U, I, U, I, I)
    subimage = gl("glTexSubImage2D", None, U, I, I, I, I, I, U, U, P)
    texture_param = gl("glGetTexParameteriv", None, U, U, C.POINTER(I))

    def configure_immutable():
        for key, value in ((0x2801, 0x2703), (0x2800, 0x2600), (0x2802, 0x812F), (0x2803, 0x812F)):
            texture_i(0x0DE1, key, value)
        texture_f(0x0DE1, 0x813A, -1000)
        texture_f(0x0DE1, 0x813B, 6)

    def immutable_texture(levels, extent=64):
        obj = U()
        active(0x84C0)
        gl("glGenTextures", None, I, C.POINTER(U))(1, C.byref(obj))
        gl("glBindTexture", None, U, U)(0x0DE1, obj)
        storage(0x0DE1, levels, 0x8058, extent, extent)
        for level in range(levels):
            width = max(1, extent >> level)
            data = (C.c_ubyte * (width * width * 4)).from_buffer_copy(bytes(colors[level]) * (width * width))
            subimage(0x0DE1, level, 0, 0, width, width, 0x1908, 0x1401, data)
        configure_immutable()
        return obj

    def immutable_state():
        active(0x84C0)
        result = []
        for key in (0x813C, 0x813D, 0x912F) + ((0x82DF,) if count_query else ()) + ((0x82DC,) if view_entry else ()):
            value = I(-1)
            texture_param(0x0DE1, key, C.byref(value))
            result.append(value.value)
        active(0x84C3)
        return tuple(result)

    def immutable_frame():
        image = (C.c_ubyte * (size * size * 4))()
        read(0, 0, size, size, 0x1908, 0x1401, image)
        return bytes(image)

    immutable_evidence = []
    for label, levels, base, last, extent in (("storage4", 4, 0, 1000, 64), ("storage2", 2, 0, 1000, 64),
                                               ("base1", 4, 1, 1000, 128), ("max1", 4, 0, 1, 64),
                                               ("single", 1, 0, 1000, 64), ("view3of7", 7, 0, 1000, 64)):
        if label == "view3of7" and not view_entry:
            continue  # optional extension only; immutable storage cases above are mandatory
        obj = immutable_texture(levels, extent)
        view = U()
        visible = levels
        if label == "view3of7":
            gl("glGenTextures", None, I, C.POINTER(U))(1, C.byref(view))
            gl(view_entry, None, U, U, U, U, U, U, U, U)(view, 0x0DE1, obj, 0x8058, 0, 3, 0, 1)
            gl("glBindTexture", None, U, U)(0x0DE1, view)
            configure_immutable()
            visible = 3
        texture_i(0x0DE1, 0x813C, base)
        texture_i(0x0DE1, 0x813D, last)
        state_before = immutable_state()
        assert state_before[:3] == (base, last, 1) and (not count_query or state_before[3] == levels) and get_error() == 0
        if view.value:
            assert state_before[-1] == 3 and levels == 7, "the view must really expose fewer levels than its parent's inherited IMMUTABLE_LEVELS"
        floor = min(3, min(last, visible - 1) - base)
        for native_binding in (samplers[0], 0):
            bind_sampler(0, native_binding)
            assert production(0, compatibility=True) == (0, colors[base]), "the driver must really sample native truncated immutable/view storage"
            native_image = immutable_frame()
            gl("glEnable", None, U)(0xDEADBEEF)  # preserve the application's pending error through our new query path
            immutable_result = production(2, compatibility=True, expected_floor=floor)
            assert immutable_result == (int(floor > 0), colors[base + floor]), "VALID immutable partial mip chains must reach real coarser material pixels, not NoSampler"
            strong_image = immutable_frame()
            assert get_error() == 0x0500 and get_error() == 0, "immutable/view inspection must preserve, not consume or add to, application errors"
            assert production(0, compatibility=True) == (0, colors[base])
            off_image = immutable_frame()
            assert off_image == native_image and (strong_image != native_image) == (floor > 0), "ENTIRE immutable/view Off frame must match native, not only a center pixel"
            assert immutable_state() == state_before and source_lods() == saved_source_lods and get_error() == 0, "inspection must not repair MAX_LEVEL or mutate any source texture/sampler state"
            artifacts = os.environ.get("BSI_TEXTURE_PIXEL_ARTIFACT_DIR")
            if artifacts and native_binding and label in ("storage4", "view3of7"):
                folder = Path(artifacts) / ("desktop" if desktop else "gles")
                for suffix, rgba in (("native", native_image), ("strong", strong_image), ("off", off_image)):
                    save_rgba(folder / ("immutable-" + label + "-" + suffix + ".png"), rgba, size)
        immutable_evidence.append({"case": label, "floor": floor, "native": list(colors[base]), "strong": list(colors[base + floor]), "offExact": True})
        active(0x84C0)
        gl("glBindTexture", None, U, U)(0x0DE1, texture)
        if view.value:
            gl("glDeleteTextures", None, I, C.POINTER(U))(1, C.byref(view))
        gl("glDeleteTextures", None, I, C.POINTER(U))(1, C.byref(obj))
    native_binding = samplers[0]
    bind_sampler(0, native_binding)
    active(0x84C3)
    assert get_error() == 0

    assert production(2, reviewed=0, compatibility=True) == (0, native), "compatibility still requires driver review"
    gl("glEnable", None, U)(0x0BE2)
    assert production(2, compatibility=True) == (0, native), "compatibility still excludes blended UI"
    gl("glDisable", None, U)(0x0BE2)
    gl("glDepthMask", None, C.c_ubyte)(1)
    gl("glEnable", None, U)(0x0B71)

    # An explicitly test-owned COLOR-ONLY FBO: no depth attachment exists at
    # all. Legacy rejects it, compatibility changes real pixels and restores
    # Off, without changing the window/context route or reading foreign data.
    active(0x84C0)
    color_target, color_fbo = U(), U()
    gl("glGenTextures", None, I, C.POINTER(U))(1, C.byref(color_target))
    gl("glBindTexture", None, U, U)(0x0DE1, color_target)
    upload(0x0DE1, 0, 0x8058, size, size, 0, 0x1908, 0x1401, None)
    texture_i(0x0DE1, 0x813D, 0)  # a real single-level compositor input, not a material mip chain
    gl("glGenFramebuffers", None, I, C.POINTER(U))(1, C.byref(color_fbo))
    bind_framebuffer = gl("glBindFramebuffer", None, U, U)
    bind_framebuffer(0x8D40, color_fbo)
    gl("glFramebufferTexture2D", None, U, U, U, U, I)(0x8D40, 0x8CE0, 0x0DE1, color_target, 0)
    assert gl("glCheckFramebufferStatus", U, U)(0x8D40) == 0x8CD5
    depth_type = I(-1)
    gl("glGetFramebufferAttachmentParameteriv", None, U, U, U, C.POINTER(I))(0x8CA9, 0x8D00, 0x8CD0, C.byref(depth_type))
    assert depth_type.value == 0, "this compatibility proof must really have NO depth attachment"
    gl("glBindTexture", None, U, U)(0x0DE1, texture)
    active(0x84C3)
    assert production(2) == (0, native), "conservative control rejects a color-only target"
    color_only = production(2, compatibility=True)
    assert color_only == (1, colors[3]), "compatibility coarsens real pixels with NO depth attachment"
    assert production(0, compatibility=True) == (0, native), "color-only compatibility Off restores exact pixels"
    assert source_lods() == saved_source_lods

    # Reproduce compositor NoSampler while its material producer IS reducible:
    # the presented input has ONE mip. The production read-only observer must
    # learn that exact texture, then qualify only the FBO writing that output.
    # Apply the private sampler to the upstream material draw, never a frame
    # blur/resolution change or an override of the one-level compositor input.
    policy.bsi_test_dependency_reset.argtypes = [U]
    policy.bsi_test_dependency_consumer.restype = U
    policy.bsi_test_dependency_producer_begin.argtypes = [I]
    policy.bsi_test_dependency_producer_begin.restype = U
    gl("glDisable", None, U)(0x0B71)
    gl("glDepthMask", None, C.c_ubyte)(0)

    def bind_input(name):
        active(0x84C0)
        gl("glBindTexture", None, U, U)(0x0DE1, name)
        active(0x84C3)

    def linked_producer(mode, target=None, pbuffer=False):
        bind_framebuffer(0x8D40, color_fbo if target is None else target)
        bind_input(texture)
        begin = policy.bsi_test_dependency_pbuffer_begin if pbuffer else policy.bsi_test_dependency_producer_begin
        count = begin(mode)
        current = I()
        get_int(0x84E0, C.byref(current))
        assert current.value == 0x84C3, "read-only lineage observation must restore ACTIVE_TEXTURE before the original"
        binding = I()
        get_binding(binding)
        assert (binding.value != native_binding) == bool(count)
        draw(0x0004, 0, 3)
        policy.bsi_test_quality_end()
        get_binding(binding)
        assert binding.value == native_binding, "a resource-linked producer must restore its exact source sampler"
        pixel = (C.c_ubyte * 4)()
        read(size // 2, size // 2, 1, 1, 0x1908, 0x1401, pixel)
        assert source_lods() == saved_source_lods and get_error() == 0
        return count, tuple(pixel)

    def composite():
        bind_framebuffer(0x8D40, 0)
        bind_input(color_target)
        return production(2, compatibility=True)

    def root_pixels():
        raw = (C.c_ubyte * (size * size * 4))()
        read(0, 0, size, size, 0x1908, 0x1401, raw)
        assert get_error() == 0
        return bytes(raw)

    policy.bsi_test_dependency_reset(1)
    linked_native = linked_producer(2)
    assert linked_native == (0, native), "a known context/output alone is not a resource link"
    assert composite() == (0, native), "the single-level compositor really has NoSampler, not a reducible material"
    root_native = root_pixels()
    assert policy.bsi_test_dependency_consumer() == 1, "capture a real single-level input from its active sampler2D uniform"
    linked_strong = linked_producer(2)
    assert linked_strong == (1, colors[3]), "actual production capture + provenance must coarsen the producer's existing material mips"
    assert composite() == (0, colors[3]), "changed producer pixels must propagate through an UNMODIFIED single-level compositor"
    root_strong = root_pixels()
    assert root_strong != root_native
    assert policy.bsi_test_dependency_producer_begin(2) == 0, "a default framebuffer is not an offscreen texture producer"
    assert linked_producer(0) == (0, native) and composite() == (0, native)
    root_off = root_pixels()
    assert root_off == root_native, "Off must restore the entire presented result byte for byte, not just a material-center pixel"
    policy.bsi_test_dependency_reset(2)
    assert linked_producer(2) == (0, native), "a setting/source revision revokes old producer evidence"
    assert composite() == (0, native) and policy.bsi_test_dependency_consumer() == 1
    for _ in range(3):
        policy.bsi_test_dependency_frame()
    assert linked_producer(2) == (0, native), "unrefreshed consumer evidence expires instead of permanently adopting its producers"
    assert composite() == (0, native) and policy.bsi_test_dependency_consumer() == 1
    assert linked_producer(2) == (1, colors[3]), "fresh presented input bindings can requalify the exact output"
    bind_framebuffer(0x8D40, color_fbo)
    gl("glColorMask", None, C.c_ubyte, C.c_ubyte, C.c_ubyte, C.c_ubyte)(0, 0, 0, 0)
    assert policy.bsi_test_dependency_producer_begin(2) == 0, "a zero-color-mask draw must not qualify on stale FBO attachment names"
    gl("glColorMask", None, C.c_ubyte, C.c_ubyte, C.c_ubyte, C.c_ubyte)(1, 1, 1, 1)
    producer_evidence = {"singleLevelCompositorBindings": 0, "linkedProducerBindings": linked_strong[0],
                         "nativeRGBA": linked_native[1], "strongRGBA": linked_strong[1],
                         "presentedOffExact": root_off == root_native, "revisionExpiryMaskBypass": True,
                         "nativeSha256": hashlib.sha256(root_native).hexdigest(),
                         "strongSha256": hashlib.sha256(root_strong).hexdigest()}

    # New independent missing-path reproductions. No material draw writes the
    # consumed texture directly: its output is a renderbuffer resolved by blit,
    # or this authored native pbuffer's DEFAULT framebuffer copied into it.
    # The C bridge supplies this fixture's native identity only; production
    # Windows hooks/lifecycle/serial guards execute in separate native tests.
    policy.bsi_test_dependency_pbuffer_begin.argtypes = [I]
    policy.bsi_test_dependency_pbuffer_begin.restype = U
    policy.bsi_test_dependency_transfer_capture.argtypes = [I, I]
    policy.bsi_test_dependency_transfer_capture.restype = U
    policy.bsi_test_dependency_transfer_complete.restype = U
    stage_fbo, stage_rbo = U(), U()
    gl("glGenFramebuffers", None, I, C.POINTER(U))(1, C.byref(stage_fbo))
    gl("glGenRenderbuffers", None, I, C.POINTER(U))(1, C.byref(stage_rbo))
    gl("glBindRenderbuffer", None, U, U)(0x8D41, stage_rbo)
    gl("glRenderbufferStorage", None, U, U, I, I)(0x8D41, 0x8058, size, size)
    bind_framebuffer(0x8D40, stage_fbo)
    gl("glFramebufferRenderbuffer", None, U, U, U, U)(0x8D40, 0x8CE0, 0x8D41, stage_rbo)
    assert gl("glCheckFramebufferStatus", U, U)(0x8D40) == 0x8CD5
    blit = gl("glBlitFramebuffer", None, I, I, I, I, I, I, I, I, U, U)
    copy_subimage = gl("glCopyTexSubImage2D", None, U, I, I, I, I, I, I, I)
    transfer_evidence = []
    for copy in (False, True):
        source_fbo = 0 if copy else stage_fbo
        policy.bsi_test_dependency_reset(100 + int(copy))

        def source(mode):
            return linked_producer(mode, source_fbo, pbuffer=copy)

        def transfer(*, pending_error=False, qualify=True):
            bind_framebuffer(0x8CA8, source_fbo)
            bind_framebuffer(0x8CA9, color_fbo)
            bind_input(color_target)
            active(0x84C0)  # CopyTexSubImage writes the actual active 2D binding.
            gl("glColorMask", None, C.c_ubyte, C.c_ubyte, C.c_ubyte, C.c_ubyte)(0, 0, 0, 0)
            gl("glEnable", None, U)(0x0BE2)
            if pending_error:
                gl("glEnable", None, U)(0xDEADBEEF)
            assert policy.bsi_test_dependency_transfer_capture(copy, qualify) == int(not copy or qualify)
            # Metadata observation cannot change ANY relevant copy state.
            actual_active, actual_texture = I(), I()
            get_int(0x84E0, C.byref(actual_active))
            get_int(0x8069, C.byref(actual_texture))
            assert actual_active.value == 0x84C0 and actual_texture.value == color_target.value
            if copy:
                copy_subimage(0x0DE1, 0, 0, 0, 0, 0, size, size)
            else:
                blit(0, 0, size, size, 0, 0, size, size, 0x4000, 0x2600)
            links = policy.bsi_test_dependency_transfer_complete()
            gl("glColorMask", None, C.c_ubyte, C.c_ubyte, C.c_ubyte, C.c_ubyte)(1, 1, 1, 1)
            gl("glDisable", None, U)(0x0BE2)
            active(0x84C3)
            if pending_error:
                assert get_error() == 0x0500, "transfer observation must preserve the application's pending error"
            assert get_error() == 0, "read-only transfer metadata and the unchanged native original must be legal on BOTH real APIs"
            return links

        assert source(2) == (0, native), "a known pbuffer/RBO is NOT itself ownership"
        assert transfer() == 0 and composite() == (0, native)
        transfer_native = root_pixels()
        assert policy.bsi_test_dependency_consumer() == 1
        assert source(2) == (0, native), "presented texture consumption alone must not invent the missing copy/resolve"
        if copy:
            assert transfer(qualify=False) == 0 and source(2) == (0, native), "an unqualified native READ pbuffer cannot become a resource dependency"
        assert transfer(pending_error=True) == 1, "a real submitted color transfer must link the exact source output"
        assert source(2) == (1, colors[3]), "the NEW transfer path must reach real material mips, not the single-level compositor"
        assert transfer() == 1 and composite() == (0, colors[3])
        transfer_strong = root_pixels()
        assert source(0) == (0, native) and transfer() == 1 and composite() == (0, native)
        transfer_off = root_pixels()
        assert transfer_off == transfer_native and transfer_strong != transfer_native, "full-frame native/Strong/exact-Off must survive the unchanged copy+composition path"
        for _ in range(3):
            policy.bsi_test_dependency_frame()
        assert source(2) == (0, native), "a submitted copy cannot renew its own inherited producer lifetime"
        label = "default-pbuffer-copy" if copy else "renderbuffer-blit"
        transfer_evidence.append({"path": label, "producerBindings": 1, "compositorBindings": 0,
                                  "nativeRGBA": list(transfer_native[:4]), "strongRGBA": list(transfer_strong[:4]),
                                  "offExact": transfer_off == transfer_native, "expiryAndPendingError": True})
        artifacts = os.environ.get("BSI_TEXTURE_PIXEL_ARTIFACT_DIR")
        if artifacts:
            folder = Path(artifacts) / ("desktop" if desktop else "gles")
            for tag, rgba in (("native", transfer_native), ("strong", transfer_strong), ("off", transfer_off)):
                save_rgba(folder / (label + "-" + tag + ".png"), rgba, size)
    bind_framebuffer(0x8D40, 0)
    gl("glBindRenderbuffer", None, U, U)(0x8D41, 0)
    gl("glDeleteRenderbuffers", None, I, C.POINTER(U))(1, C.byref(stage_rbo))
    gl("glDeleteFramebuffers", None, I, C.POINTER(U))(1, C.byref(stage_fbo))
    bind_input(texture)
    gl("glDepthMask", None, C.c_ubyte)(1)
    gl("glEnable", None, U)(0x0B71)
    bind_framebuffer(0x8D40, 0)
    gl("glDeleteFramebuffers", None, I, C.POINTER(U))(1, C.byref(color_fbo))
    gl("glDeleteTextures", None, I, C.POINTER(U))(1, C.byref(color_target))
    assert get_error() == 0, message.value.decode()
    for compatibility in (False, True):
        gl("glEnable", None, U)(0xDEADBEEF)
        assert production(2, compatibility=compatibility) == (1, colors[3])
        assert get_error() == 0x0500 and get_error() == 0, "neither policy may consume the application's pending API error"

    # A second, spatial-detail fixture: upload an authored checker material and
    # its actual box-filtered mips. No final-image grading or blur is involved.
    # Production compatibility must remove high-frequency detail by sampling
    # those existing mips, with depth disabled, at UNCHANGED resolution, then
    # restore the full native image byte for byte in BOTH source modes.
    assert size == 64, "spatial fixture is authored at the same 64x64 texture/target size"
    data = [tuple((210, 160, 90, 173) if (x // 8 + y // 8) % 2 else (50, 100, 150, 173))
            if (x + y) % 2 else (12, 24, 36, 173) for y in range(size) for x in range(size)]
    active(0x84C0)
    extent = size
    for level in range(7):
        raw = bytes(channel for pixel in data for channel in pixel)
        upload(0x0DE1, level, 0x8058, extent, extent, 0, 0x1908, 0x1401, C.c_char_p(raw))
        if extent > 1:
            data = [tuple(sum(data[(2 * y + dy) * extent + 2 * x + dx][c] for dy in (0, 1) for dx in (0, 1)) // 4
                          for c in range(4)) for y in range(extent // 2) for x in range(extent // 2)]
            extent //= 2
    active(0x84C3)
    gl("glDisable", None, U)(0x0B71)
    gl("glDepthMask", None, C.c_ubyte)(0)
    material_evidence = []

    def frame():
        rgba = (C.c_ubyte * (size * size * 4))()
        read(0, 0, size, size, 0x1908, 0x1401, rgba)
        return bytes(rgba)

    def edge_energy(rgba):
        return sum(abs(rgba[(y * size + x) * 4 + c] - rgba[(yy * size + xx) * 4 + c])
                   for y in range(size) for x in range(size) for yy, xx in ((y, x + 1), (y + 1, x))
                   if xx < size and yy < size for c in range(3))

    for native_binding in (samplers[0], 0):
        bind_sampler(0, native_binding)
        assert production(0, compatibility=True)[0] == 0
        native_image = frame()
        assert production(2, compatibility=True)[0] == 1
        coarse_image = frame()
        assert production(0, compatibility=True)[0] == 0
        off_image = frame()
        assert off_image == native_image and native_image != coarse_image, "compatibility must restore the ENTIRE native material image, not just a center pixel"
        native_energy, coarse_energy = edge_energy(native_image), edge_energy(coarse_image)
        assert 0 < coarse_energy < native_energy * .6, ("real material detail did not reduce", native_energy, coarse_energy)
        assert native_image[3::4] == coarse_image[3::4] == bytes([173]) * (size * size)
        assert source_lods() == saved_source_lods and get_error() == 0
        source = "sampler" if native_binding else "texture"
        material_evidence.append({"source": source, "nativeEdgeEnergy": native_energy, "strongEdgeEnergy": coarse_energy,
                                  "nativeSha256": hashlib.sha256(native_image).hexdigest(), "strongSha256": hashlib.sha256(coarse_image).hexdigest(),
                                  "offExact": off_image == native_image})
        artifacts = os.environ.get("BSI_TEXTURE_PIXEL_ARTIFACT_DIR")
        if artifacts:
            folder = Path(artifacts) / ("desktop" if desktop else "gles")
            for label, rgba in (("native", native_image), ("strong", coarse_image), ("off", off_image)):
                save_rgba(folder / (source + "-" + label + ".png"), rgba, size)
    gl("glDepthMask", None, C.c_ubyte)(1)
    policy.bsi_test_quality_release()
    active(0x84C0)
    bind_sampler(0, 0)
    gl("glDeleteSamplers", None, I, C.POINTER(U))(3, samplers)
    gl("glBindTexture", None, U, U)(0x0DE1, 0)
    gl("glDeleteTextures", None, I, C.POINTER(U))(1, C.byref(texture))
    gl("glBindVertexArray", None, U)(0)
    gl("glDeleteVertexArrays", None, I, C.POINTER(U))(1, C.byref(vao))
    gl("glUseProgram", None, U)(0)
    gl("glDeleteProgram", None, U)(program)
    assert get_error() == 0
    evidence = {"api": "desktop" if desktop else "gles", "version": version,
                "renderer": gl("glGetString", C.c_char_p, U)(0x1F01).decode(), "maxBias": maximum_bias.value,
                "offCycles": cycles, "depthIndependentCycles": 64, "depthTestAndWritesDisabled": True,
                "noDepthAttachmentRGBA": color_only[1], "mipPixels": mip_evidence, "material": material_evidence,
                "resourceLinkedProducer": producer_evidence, "colorTransfers": transfer_evidence,
                "immutableMips": {"sourceModes": 2, "cases": immutable_evidence, "pendingErrorAndSourceState": True},
                "strength": {"percent": [0, 25, 50, 75, 100], "rgba": strength_pixels, "sourceModes": 2, "gradients": 2, "fullOffAndSourceExact": True},
                "limit": "authored software resources; NOT player, physical GPU, allocated-memory or FPS evidence"}
    report = json.dumps(evidence, separators=(",", ":"))
    if os.environ.get("BSI_TEXTURE_PIXEL_ARTIFACT_DIR"):
        (folder / "evidence.json").write_text(json.dumps(evidence, indent=2) + "\n")
    print("PASS: production draw-scoped " + ("desktop OpenGL queried bias+floor" if desktop else "GLES 3 floor-only")
          + " selects real 0/25/50/75/100-percent strength pixels with exact full-frame Off and pending errors; test-owned mip colors at native lambda 0 AND 4 in both texture/sampler source modes; 128 exact Off/source-LOD cycles (64 depth-independent); valid partial immutable chains/nonzero base/max/view bounds with exact full-frame Off and pending errors; color-only/no-depth-attachment pixels, direct FBO plus renderbuffer-blit and default-pbuffer-copy material producers -> unchanged single-level compositor with exact full-frame Off/expiry, and full material-detail images; legacy depth/blend/no-mip/override/review bypass preserved")
    print("Texture pixel evidence:", report)
    if os.environ.get("GITHUB_ACTIONS") == "true":
        # Keep measured SUCCESS evidence readable through GitHub's check API,
        # not just a job conclusion or an inaccessible signed log/artifact URL.
        assert len(report.encode()) < 3500, "bounded evidence must not be silently truncated by check annotations"
        escaped = report.replace("%", "%25").replace("\r", "%0D").replace("\n", "%0A")
        print("::notice title=Production depth-independent texture pixels (software)::" + escaped)
    print("LIMIT: owned software-context sampler proof, not game interception, physical GPU, profile compatibility or FPS evidence")
