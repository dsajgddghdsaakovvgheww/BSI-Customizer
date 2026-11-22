"""Actual EGL render-to-texture pixels across NONSHARING GLES contexts.

Only authored resources: not BlueStacks, WGL, protected memory or GPU hardware.
Native HWND/lifecycle detours execute separately in the Windows contract tests.
"""
import ctypes as C
import hashlib
import json
import os
from pathlib import Path

I, U, P, F = C.c_int, C.c_uint, C.c_void_p, C.c_float


def exercise(gl, policy, resolver, egl, display, consumer, consumer_surface, make_current, size):
    from shaders.texture_quality_gl_pixels import save_rgba

    choose = egl("eglChooseConfig", U, P, C.POINTER(I), C.POINTER(P), I, C.POINTER(I))
    create_context = egl("eglCreateContext", P, P, P, P, C.POINTER(I))
    create_surface = egl("eglCreatePbufferSurface", P, P, P, C.POINTER(I))
    bind_image = egl("eglBindTexImage", U, P, P, I)
    release_image = egl("eglReleaseTexImage", U, P, P, I)
    config, count = P(), I()
    attrs = (I * 13)(0x3033, 1, 0x3040, 0x40, 0x3024, 8, 0x3023, 8, 0x3021, 8, 0x303A, 1, 0x3038)
    assert choose(display, attrs, C.byref(config), 1, C.byref(count)) and count.value, "real GLES native-image test requires a texture-capable RGBA pbuffer config, never a skipped pass"
    producer = create_context(display, config, None, (I * 3)(0x3098, 3, 0x3038))
    pbuffer = create_surface(display, config, (I * 9)(0x3057, size, 0x3056, size, 0x3081, 0x305F, 0x3080, 0x305E, 0x3038))
    assert producer and pbuffer, "create a real NONSHARING producer and native image pbuffer"
    get_error = gl("glGetError", U)
    active = gl("glActiveTexture", None, U)
    bind_texture = gl("glBindTexture", None, U, U)
    tex_i = gl("glTexParameteri", None, U, U, I)
    sampler_i = gl("glSamplerParameteri", None, U, U, I)
    bind_sampler = gl("glBindSampler", None, U, U)
    get_int = gl("glGetIntegerv", None, U, C.POINTER(I))
    read = gl("glReadPixels", None, I, I, I, I, U, U, P)
    draw = gl("glDrawArrays", None, U, I, I)
    colors = [(255, 0, 0, 173), (0, 255, 0, 173), (0, 0, 255, 173), (255, 255, 0, 173),
              (255, 0, 255, 173), (0, 255, 255, 173), (255, 255, 255, 173)]
    policy.bsi_test_native_producer_initialize.argtypes = [type(resolver), P]
    policy.bsi_test_native_producer_initialize.restype = I
    policy.bsi_test_native_producer_begin.argtypes = [I, I]
    policy.bsi_test_native_producer_begin.restype = U
    policy.bsi_test_native_binding_capture.restype = U
    policy.bsi_test_native_binding_complete.argtypes = [I]
    policy.bsi_test_native_binding_complete.restype = U

    def current(context, surface):
        assert make_current(display, surface, surface, context)

    def image():
        raw = (C.c_ubyte * (size * size * 4))()
        read(0, 0, size, size, 0x1908, 0x1401, raw)
        assert get_error() == 0
        return bytes(raw)

    def resources(material):
        def shader(kind, body):
            obj = gl("glCreateShader", U, U)(kind)
            text = C.c_char_p(("#version 300 es\nprecision highp float;\n" + body).encode())
            gl("glShaderSource", None, U, I, C.POINTER(C.c_char_p), C.POINTER(I))(obj, 1, C.byref(text), None)
            gl("glCompileShader", None, U)(obj)
            status = I()
            gl("glGetShaderiv", None, U, U, C.POINTER(I))(obj, 0x8B81, C.byref(status))
            assert status.value, "native image fixture shader compilation"
            return obj
        program = gl("glCreateProgram", U)()
        vertex = shader(0x8B31, "out vec2 uv;void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);uv=p;gl_Position=vec4(p*2.-1.,0.,1.);}")
        fragment = shader(0x8B30, "in vec2 uv;uniform sampler2D sourceTexture;out vec4 color;void main(){color=texture(sourceTexture,uv);}")
        for obj in (vertex, fragment):
            gl("glAttachShader", None, U, U)(program, obj)
        gl("glLinkProgram", None, U)(program)
        status = I()
        gl("glGetProgramiv", None, U, U, C.POINTER(I))(program, 0x8B82, C.byref(status))
        assert status.value
        for obj in (vertex, fragment):
            gl("glDeleteShader", None, U)(obj)
        gl("glUseProgram", None, U)(program)
        location = gl("glGetUniformLocation", I, U, C.c_char_p)(program, b"sourceTexture")
        assert location >= 0
        gl("glUniform1i", None, I, I)(location, 0)
        vao, texture, sampler = U(), U(), U()
        gl("glGenVertexArrays", None, I, C.POINTER(U))(1, C.byref(vao))
        gl("glBindVertexArray", None, U)(vao)
        active(0x84C0)
        gl("glGenTextures", None, I, C.POINTER(U))(1, C.byref(texture))
        bind_texture(0x0DE1, texture)
        tex_i(0x0DE1, 0x2801, 0x2703 if material else 0x2601)
        tex_i(0x0DE1, 0x2800, 0x2600)
        # Deliberately leave MAX_LEVEL at its stored default 1000, just like
        # the installed report. LINEAR on the consumer does NOT require mip 1.
        if material:
            for level, rgba in enumerate(colors):
                extent = size >> level
                gl("glTexImage2D", None, U, I, I, I, I, I, U, U, P)(0x0DE1, level, 0x8058, extent, extent, 0, 0x1908, 0x1401,
                                                                                C.c_char_p(bytes(rgba) * (extent * extent)))
        gl("glGenSamplers", None, I, C.POINTER(U))(1, C.byref(sampler))
        for name, value in ((0x2801, 0x2703 if material else 0x2601), (0x2800, 0x2600), (0x2802, 0x812F), (0x2803, 0x812F)):
            sampler_i(sampler, name, value)
        bind_sampler(0, sampler)
        gl("glBindFramebuffer", None, U, U)(0x8D40, 0)
        gl("glViewport", None, I, I, I, I)(0, 0, size, size)
        for cap in (0x0BE2, 0x0B71, 0x0B90, 0x0B44, 0x0C11, 0x8C89):
            gl("glDisable", None, U)(cap)
        gl("glColorMask", None, C.c_ubyte, C.c_ubyte, C.c_ubyte, C.c_ubyte)(1, 1, 1, 1)
        active(0x84C3)
        assert get_error() == 0
        return vao, texture, sampler, program

    current(producer, pbuffer)
    source_objects = resources(True)
    current(consumer, consumer_surface)
    consumer_objects = resources(False)

    # First require an actual RAW driver round trip, BEFORE either production
    # inspector is initialized or any provenance/override call is made. Mesa
    # 25.2.8's surfaceless software loader returns success + a defined image but
    # exports transparent black even without BSI (also with Finish/WaitClient).
    # The mandatory CI native-image matrix uses ANGLE, not a skipped assertion
    # or a production copy/wait workaround for that independent driver path.
    current(producer, pbuffer)
    draw(4, 0, 3)
    raw_material = image()
    gl("glFlush", None)()
    current(consumer, consumer_surface)
    active(0x84C0)
    assert bind_image(display, pbuffer, 0x3084), "RAW native driver image binding must succeed before production is initialized"
    active(0x84C3)
    draw(4, 0, 3)
    raw_presented = image()
    assert raw_presented == raw_material and tuple(raw_material[:4]) == colors[0], (
        "RAW native driver image pixels differ BEFORE production observation/overrides",
        list(raw_material[:4]), list(raw_presented[:4]),
        sum(a != b for a, b in zip(raw_material, raw_presented)))
    assert release_image(display, pbuffer, 0x3084)
    current(producer, pbuffer)
    assert policy.bsi_test_native_producer_initialize(resolver, None)
    current(consumer, consumer_surface)
    message = C.create_string_buffer(2048)
    assert policy.bsi_test_quality_initialize(resolver, None, message, len(message)), message.value.decode()

    def source(mode, strength=100, pending_error=False, texture_source=False):
        current(producer, pbuffer)
        native_sampler = 0 if texture_source else source_objects[2].value
        bind_sampler(0, native_sampler)
        if pending_error:
            gl("glEnable", None, U)(0xDEADBEEF)
        bindings = policy.bsi_test_native_producer_begin(mode, strength)
        actual, unit = I(), I()
        get_int(0x84E0, C.byref(unit))
        assert unit.value == 0x84C3
        active(0x84C0)
        get_int(0x8919, C.byref(actual))
        assert (actual.value != native_sampler) == bool(bindings)
        active(0x84C3)
        draw(4, 0, 3)
        policy.bsi_test_native_producer_end()
        active(0x84C0)
        get_int(0x8919, C.byref(actual))
        assert actual.value == native_sampler, "exact native producer binding restored before any following draw"
        # Query both the application's sampler and texture: neither may be
        # persistently changed, unlike the supplied original DLL.
        floor = F()
        gl("glGetSamplerParameterfv", None, U, U, C.POINTER(F))(source_objects[2], 0x813A, C.byref(floor))
        assert floor.value == -1000
        gl("glGetTexParameterfv", None, U, U, C.POINTER(F))(0x0DE1, 0x813A, C.byref(floor))
        assert floor.value == -1000
        active(0x84C3)
        if pending_error:
            assert get_error() == 0x0500, "native producer inspection must preserve a pending application error"
        rgba = image()
        # The application fixture, NOT production, submits its own work before
        # switching context; there are no production waits/context migrations.
        gl("glFlush", None)()
        return bindings, rgba

    def composite(expected_links, pending_error=False):
        current(consumer, consumer_surface)
        active(0x84C0)
        if pending_error:
            gl("glEnable", None, U)(0xDEADBEEF)
        assert policy.bsi_test_native_binding_capture() == 1
        success = bind_image(display, pbuffer, 0x3084)
        assert success, "the actual native EGL image binding must succeed"
        for name, expected in ((0x3081, 0x305F), (0x3080, 0x305E), (0x3083, 0)):
            value = I(-1)
            assert egl("eglQuerySurface", U, P, P, I, C.POINTER(I))(display, pbuffer, name, C.byref(value)) and value.value == expected
        assert policy.bsi_test_native_binding_complete(success) == expected_links
        if pending_error:
            assert get_error() == 0x0500, "native binding observation must not consume a pending application error"
        active(0x84C3)
        assert policy.bsi_test_quality_begin_compatibility(2, 1, 4, message, len(message)) == 0
        assert b"non-mip-filter=1" in message.value and b"incomplete-required-level=0" in message.value, message.value.decode()
        draw(4, 0, 3)
        policy.bsi_test_quality_end()
        rgba = image()
        assert policy.bsi_test_dependency_consumer() == 1
        assert release_image(display, pbuffer, 0x3084), "application releases the native image before rendering the next pbuffer frame"
        return rgba

    policy.bsi_test_dependency_reset(300)
    count, before = source(2)
    assert count == 0 and tuple(before[:4]) == colors[0]
    assert composite(0) == before, "pbuffer existence/binding without a consumer cannot invent ownership"
    assert source(2)[0] == 0, "texture consumption without the missing native binding link is still insufficient"
    native = composite(1, pending_error=True)
    results = []
    for texture_source in (False, True):
        for strength, expected in ((0, colors[0]), (50, (0, 128, 128, 173)), (100, colors[3])):
            count, material = source(2, strength, pending_error=True, texture_source=texture_source)
            assert count == int(strength > 0) and tuple(material[:4]) == expected, ("actual native-image material draw", strength, count, material[:4])
            presented = composite(1)
            assert presented == material, "UNCHANGED one-level compositor must present the upstream material pixels exactly"
            if not texture_source:
                results.append(presented)
        count, off = source(0, texture_source=texture_source)
        assert count == 0 and composite(1) == off == native, "ENTIRE native image-binding Off frame must match native byte-for-byte in both source modes"
    for _ in range(3):
        policy.bsi_test_dependency_frame()
    assert source(2)[0] == 0, "native output evidence inherits its consumer TTL, not permanent context authorization"
    policy.bsi_test_dependency_reset(301)
    assert source(2)[0] == 0, "a setting/source revision invalidates native image bindings too"

    def cleanup(objects):
        vao, texture, sampler, program = objects
        active(0x84C0)
        bind_sampler(0, 0)
        bind_texture(0x0DE1, 0)
        gl("glDeleteSamplers", None, I, C.POINTER(U))(1, C.byref(sampler))
        gl("glDeleteTextures", None, I, C.POINTER(U))(1, C.byref(texture))
        gl("glBindVertexArray", None, U)(0)
        gl("glDeleteVertexArrays", None, I, C.POINTER(U))(1, C.byref(vao))
        gl("glUseProgram", None, U)(0)
        gl("glDeleteProgram", None, U)(program)
        assert get_error() == 0

    current(producer, pbuffer)
    policy.bsi_test_native_producer_release()
    cleanup(source_objects)
    current(consumer, consumer_surface)
    policy.bsi_test_quality_release()
    cleanup(consumer_objects)
    assert egl("eglDestroyContext", U, P, P)(display, producer)
    assert egl("eglDestroySurface", U, P, P)(display, pbuffer)
    evidence = {"api": "gles", "path": "actual-eglBindTexImage", "nonsharingContexts": True,
                "version": gl("glGetString", C.c_char_p, U)(0x1F02).decode(),
                "renderer": gl("glGetString", C.c_char_p, U)(0x1F01).decode(),
                "rawNativeBindingExactBeforeProduction": True,
                "strength": [0, 50, 100], "rgba": [list(rgba[:4]) for rgba in results],
                "sourceModes": 2, "producerBindings": 1, "compositorBindings": 0,
                "offFullFrameExact": True, "sourceAndPendingErrorsExact": True, "revisionAndExpiry": True,
                "nativeSha256": hashlib.sha256(native).hexdigest(), "strongSha256": hashlib.sha256(results[2]).hexdigest(),
                "limit": "authored software EGL, not WGL/player/hardware/FPS"}
    folder = os.environ.get("BSI_TEXTURE_PIXEL_ARTIFACT_DIR")
    if folder:
        folder = Path(folder) / "gles" / "native-image"
        folder.mkdir(parents=True, exist_ok=True)
        for name, rgba in (("native", native), ("strong", results[2]), ("off", off)):
            save_rgba(folder / (name + ".png"), rgba, size)
        (folder / "evidence.json").write_text(json.dumps(evidence, indent=2) + "\n")
    report = json.dumps(evidence, separators=(",", ":"))
    print("PASS: actual EGL native pbuffer image binding across NONSHARING GLES contexts; upstream material mip/strength pixels, zero one-level compositor replacements, both source modes, full-frame exact Off/source/pending errors and bounded revision/expiry")
    print("Native image pixel evidence:", report)
    if os.environ.get("GITHUB_ACTIONS") == "true":
        assert len(report.encode()) < 2000
        print("::notice title=Native pbuffer image binding pixels (software)::" + report.replace("%", "%25").replace("\r", "%0D").replace("\n", "%0A"))
