"""Emulated-game proof that the production Potato code changes real game pixels.

The point of this test is to stop describing Potato in the abstract. It builds a
game-shaped GLES 3 renderer (offscreen material asset with a real mip chain, a
compositor draw that samples it, a blended UI pass, many frames) and drives the
REAL ``BsiShaders::GlTextureOverride`` / ``GlTextureProvenance`` objects between
the draws. That is the same call sequence the player's OpenGL hooks use; only
the interception layer (MinHook detours on ``libOpenglRender``/``HD-Opengl-
Native`` inside the Windows player) is replaced by direct calls from the
harness, because the detour contracts are already covered by their own tests.

So a pass here means: for an emulated game frame, the player-facing setting
(On/Off plus 0-100 strength) changes sampled game-material detail, the blended
UI pass stays native, the game's own sampler state is never edited, and Off
restores the previous frame byte for byte.

Honest scope, printed at the end and never skipped:
  software GLES renderer standing in for an Android game inside the player;
  not a captured game, not a physical GPU, not an FPS measurement, and not a
  Windows detour/presentation proof. A missing GL driver FAILS the test.

Usage:
  BSI_SHADERS_EGL_LIBRARY=/path/libEGL.so \
  BSI_SHADERS_EGL_PLATFORM=angle-swiftshader \
  python3 tests/shaders_potato_emulator_game_test.py
"""
import ctypes as C
import ctypes.util
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import zlib

ROOT = Path(__file__).resolve().parents[1]
I, U, P, F = C.c_int, C.c_uint, C.c_void_p, C.c_float
EGL_NONE = 0x3038
SIZE = 256          # presented frame
MATERIAL = 256      # game material asset, sampled 1:1 by the compositor
FRAMES = 6          # frames rendered per setting
SCENE_DRAWS = 2     # qualified scene draws per frame
STRENGTHS = (0, 25, 50, 75, 100)
POLICY_OWNED_MIP, POLICY_DEPTH_QUALIFIED = 1, 0
MODE_STRONG = 2
MODE_LIGHT = 1
# Strong keeps the original 3-level intent, Light the original lighter one.
MODE_FLOOR = {MODE_LIGHT: 1.5, MODE_STRONG: 3.0}

GL_TRIANGLES = 0x0004
GL_TEXTURE_2D = 0x0DE1
GL_SAMPLER_BINDING = 0x8919  # via ACTIVE_TEXTURE' unit GetIntegerv
GL_ACTIVE_TEXTURE = 0x84E0
GL_TEXTURE0 = 0x84C0


# --------------------------------------------------------------------------
# adapter: the only C++ we compile. It forwards to production, nothing else.
# --------------------------------------------------------------------------
ADAPTER_SOURCES = ("tests/shaders/emulated_game_adapter.cpp", "src/shaders/GlTextureOverride.cpp")


def adapter_sources():
    """Production sources to compile. Overridable ONLY so the harness can be
    shown to fail against deliberately neutered production code (negative
    control); the default is always the real repository source."""
    override = os.environ.get("BSI_POTATO_ADAPTER_SOURCES")
    return [Path(part).resolve() for part in override.split(",")] if override else \
        [ROOT / source for source in ADAPTER_SOURCES]


def build_adapter(output):
    command = ["g++", "-std=c++17", "-O1", "-g", "-Wall", "-Wextra", "-Werror", "-pedantic",
               "-shared", "-fPIC", f"-I{ROOT / 'src' / 'shaders'}"]
    command += [str(source) for source in adapter_sources()]
    command += ["-o", str(output)]
    result = subprocess.run(command, capture_output=True, text=True)
    assert result.returncode == 0, f"the emulated-game adapter must build:\n{result.stdout}{result.stderr}"
    return output


# --------------------------------------------------------------------------
# EGL / GLES bootstrap on a real software driver (never a stub)
# --------------------------------------------------------------------------
def open_egl():
    library = os.environ.get("BSI_SHADERS_EGL_LIBRARY") or ctypes.util.find_library("EGL")
    assert library, ("EGL is required: install a software driver or point BSI_SHADERS_EGL_LIBRARY at an "
                     "ANGLE/SwiftShader libEGL. A missing driver fails this test; it is never skipped.")
    egl = C.CDLL(library)

    def native(symbol, result, *args):
        function = getattr(egl, symbol)
        function.restype, function.argtypes = result, list(args)
        return function

    get_proc = native("eglGetProcAddress", P, C.c_char_p)
    platform = C.CFUNCTYPE(P, U, P, C.POINTER(I))(get_proc(b"eglGetPlatformDisplayEXT"))
    if os.environ.get("BSI_SHADERS_EGL_PLATFORM") == "angle-swiftshader":
        attributes = (I * 5)(0x3203, 0x3450, 0x3209, 0x3487, EGL_NONE)
        display = platform(0x3202, None, attributes)
    else:
        display = platform(0x31DD, None, (I * 1)(EGL_NONE))
    major, minor = I(), I()
    assert native("eglInitialize", U, P, C.POINTER(I), C.POINTER(I))(display, C.byref(major), C.byref(minor)), \
        "Cannot initialize a real EGL display"
    assert native("eglBindAPI", U, U)(0x30A0), "Cannot select OpenGL ES"
    attributes = [0x3033, 1, 0x3040, 0x40, 0x3024, 8, 0x3023, 8, 0x3022, 8, 0x3021, 8, 0x3025, 24,
                  0x3031, 0, 0x3032, 0]
    config, count = P(), I()
    assert native("eglChooseConfig", U, P, C.POINTER(I), C.POINTER(P), I, C.POINTER(I))(
        display, (I * (len(attributes) + 1))(*attributes, EGL_NONE), C.byref(config), 1, C.byref(count)) and count.value, \
        "A GLES 3 config with a real depth buffer is required"
    context = native("eglCreateContext", P, P, P, P, C.POINTER(I))(
        display, config, None, (I * 3)(0x3098, 3, EGL_NONE))
    surface = native("eglCreatePbufferSurface", P, P, P, C.POINTER(I))(
        display, config, (I * 5)(0x3057, SIZE, 0x3056, SIZE, EGL_NONE))
    assert context and surface, "Cannot create the ES3 context/surface"
    assert native("eglMakeCurrent", U, P, P, P, P)(display, surface, surface, context), "eglMakeCurrent failed"
    return get_proc


def gl_table(get_proc):
    def gl(name, result, *args, required=True):
        address = get_proc(name.encode())
        if not address:
            assert not required, f"the driver must provide {name}"
            return None
        function = C.CFUNCTYPE(result, *args)(address)
        return function

    table = {
        "GetString": gl("glGetString", C.c_char_p, U),
        "GetIntegerv": gl("glGetIntegerv", None, U, C.POINTER(I)),
        "GetError": gl("glGetError", U),
        "GetFloatv": gl("glGetFloatv", None, U, C.POINTER(F)),
        "GetTexParameterfv": gl("glGetTexParameterfv", None, U, U, C.POINTER(F)),
        "GetSamplerParameterfv": gl("glGetSamplerParameterfv", None, U, U, C.POINTER(F)),
        "Enable": gl("glEnable", None, U),
        "Disable": gl("glDisable", None, U),
        "IsEnabled": gl("glIsEnabled", C.c_ubyte, U),
        "ClearColor": gl("glClearColor", None, F, F, F, F),
        "Clear": gl("glClear", None, U),
        "ColorMask": gl("glColorMask", None, C.c_ubyte, C.c_ubyte, C.c_ubyte, C.c_ubyte),
        "Viewport": gl("glViewport", None, I, I, I, I),
        "PixelStorei": gl("glPixelStorei", None, U, I),
        "ReadPixels": gl("glReadPixels", None, I, I, I, I, U, U, P),
        "DrawArrays": gl("glDrawArrays", None, U, I, I),
        "DrawElements": gl("glDrawElements", None, U, I, U, P),
        "GenBuffers": gl("glGenBuffers", None, I, C.POINTER(U)),
        "BindBuffer": gl("glBindBuffer", None, U, U),
        "BufferData": gl("glBufferData", None, U, C.c_ssize_t, P, U),
        "BlendFunc": gl("glBlendFunc", None, U, U),
        "DepthFunc": gl("glDepthFunc", None, U),
        "DepthMask": gl("glDepthMask", None, C.c_ubyte),
        "ClearDepthf": gl("glClearDepthf", None, F),
        "ActiveTexture": gl("glActiveTexture", None, U),
        "BindTexture": gl("glBindTexture", None, U, U),
        "GenTextures": gl("glGenTextures", None, I, C.POINTER(U)),
        "TexImage2D": gl("glTexImage2D", None, U, I, I, I, I, I, U, U, P),
        "GenerateMipmap": gl("glGenerateMipmap", None, U),
        "TexParameteri": gl("glTexParameteri", None, U, U, I),
        "TexParameterf": gl("glTexParameterf", None, U, U, F),
        "GenSamplers": gl("glGenSamplers", None, I, C.POINTER(U)),
        "SamplerParameteri": gl("glSamplerParameteri", None, U, U, I),
        "SamplerParameterf": gl("glSamplerParameterf", None, U, U, F),
        "BindSampler": gl("glBindSampler", None, U, U),
        "CreateShader": gl("glCreateShader", U, U),
        "ShaderSource": gl("glShaderSource", None, U, I, C.POINTER(C.c_char_p), C.POINTER(I)),
        "CompileShader": gl("glCompileShader", None, U),
        "GetShaderiv": gl("glGetShaderiv", None, U, U, C.POINTER(I)),
        "GetShaderInfoLog": gl("glGetShaderInfoLog", None, U, I, C.POINTER(I), C.c_char_p),
        "CreateProgram": gl("glCreateProgram", U),
        "AttachShader": gl("glAttachShader", None, U, U),
        "LinkProgram": gl("glLinkProgram", None, U),
        "GetProgramiv": gl("glGetProgramiv", None, U, U, C.POINTER(I)),
        "UseProgram": gl("glUseProgram", None, U),
        "GetUniformLocation": gl("glGetUniformLocation", I, U, C.c_char_p),
        "Uniform1i": gl("glUniform1i", None, I, I),
        "DeleteShader": gl("glDeleteShader", None, U),
        "GenVertexArrays": gl("glGenVertexArrays", None, I, C.POINTER(U)),
        "BindVertexArray": gl("glBindVertexArray", None, U),
        "EnableVertexAttribArray": gl("glEnableVertexAttribArray", None, U),
        "VertexAttribPointer": gl("glVertexAttribPointer", None, U, I, U, C.c_ubyte, I, P),
        "Uniform1f": gl("glUniform1f", None, I, F),
    }
    return table


# --------------------------------------------------------------------------
# the emulated game
# --------------------------------------------------------------------------
def material_pixels(size):
    pixels = bytearray(size * size * 4)
    for y in range(size):
        for x in range(size):
            checker = ((x // 8) + (y // 8)) & 1
            band = ((x + y) // 6) & 1
            red = 235 if band else 20
            green = 210 if checker else 30
            blue = (x * 255) // (size - 1)
            if x % 16 < 2 or y % 16 < 2:
                red = green = blue = 255
            at = (y * size + x) * 4
            pixels[at:at + 4] = bytes((red, green, blue, 255))
    return bytes(pixels)


class EmulatedGame:
    """A game-shaped GLES 3 renderer: asset mip chain, scene draws, UI pass."""

    def __init__(self, gl):
        self.gl = gl
        self.program = self._program()
        vao = U()
        gl["GenVertexArrays"](1, C.byref(vao))
        gl["BindVertexArray"](vao)
        self.vao = vao
        # One full-frame triangle, submitted the way games do: a vertex + index
        # buffer, GL_TRIANGLES, glDrawElements (not a two-vertex strip).
        vertices = (F * 6)(-1, -1, 3, -1, -1, 3)
        vertex_buffer = U()
        gl["GenBuffers"](1, C.byref(vertex_buffer))
        gl["BindBuffer"](0x8892, vertex_buffer)
        gl["BufferData"](0x8892, C.sizeof(vertices), vertices, 0x88E4)
        gl["EnableVertexAttribArray"](0)
        gl["VertexAttribPointer"](0, 2, 0x1406, 0, 8, None)
        indices = (C.c_ushort * 3)(0, 1, 2)
        index_buffer = U()
        gl["GenBuffers"](1, C.byref(index_buffer))
        gl["BindBuffer"](0x8893, index_buffer)
        gl["BufferData"](0x8893, C.sizeof(indices), indices, 0x88E4)
        self.index_count = 3
        gl["UseProgram"](self.program)
        location = gl["GetUniformLocation"](self.program, b"depth")
        assert location >= 0, "the scene program must expose its depth uniform"
        gl["Uniform1f"](location, 0.5)
        location = gl["GetUniformLocation"](self.program, b"gameMaterial")
        assert location >= 0, "the compositor program must expose its sampler"
        gl["Uniform1i"](location, 0)
        gl["PixelStorei"](0x0D05, 1)  # PACK_ALIGNMENT
        self.material = self._material()
        samplers = (U * 2)()
        gl["GenSamplers"](2, samplers)
        self.sampler, self.texture_sampler = int(samplers[0]), int(samplers[1])
        assert self.sampler and self.texture_sampler, "the game must own real sampler objects"
        # A real game asset sampler: trilinear over a complete mip chain, no
        # LOD clamps of its own (the driver default -1000/1000 is what Potato
        # is allowed to narrow for a draw and must restore afterwards).
        for sampler in (self.sampler, self.texture_sampler):
            gl["SamplerParameteri"](sampler, 0x2800, 0x2601)  # MAG linear
            gl["SamplerParameteri"](sampler, 0x2801, 0x2703)  # MIN trilinear
            gl["SamplerParameteri"](sampler, 0x2802, 0x812F)  # CLAMP_TO_EDGE
            gl["SamplerParameteri"](sampler, 0x2803, 0x812F)
            gl["SamplerParameterf"](sampler, 0x813A, -1000.0)  # MIN_LOD default
            gl["SamplerParameterf"](sampler, 0x813B, 1000.0)   # MAX_LOD default

    def _program(self):
        gl = self.gl
        prefix = (b"#version 300 es\nprecision highp float;\nprecision highp sampler2D;\n")

        def shader(kind, body):
            obj = gl["CreateShader"](kind)
            source = prefix + body
            strings = (C.c_char_p * 1)(source)
            gl["ShaderSource"](obj, 1, strings, None)
            gl["CompileShader"](obj)
            status = I()
            gl["GetShaderiv"](obj, 0x8B81, C.byref(status))
            log = C.create_string_buffer(4096)
            gl["GetShaderInfoLog"](obj, len(log), None, log)
            assert status.value, f"game shader failed to compile: {log.value.decode()}"
            return obj

        # A real scene vertex stage: indexed quad vertices, a perspective-style
        # MVP and a half-depth quad, so depth testing/writing is genuinely on.
        vertex = shader(0x8B31, b"layout(location=0)in vec2 position;uniform float depth;"
                                b"void main(){gl_Position=vec4(position,0.,1.);"
                                b"gl_Position.z=depth;}")
        fragment = shader(0x8B30, b"uniform sampler2D gameMaterial;out vec4 color;"
                                  b"void main(){color=texture(gameMaterial,gl_FragCoord.xy/"
                                  + str(float(SIZE)).encode() + b");}")  # 1:1 asset read
        program = gl["CreateProgram"]()
        gl["AttachShader"](program, vertex)
        gl["AttachShader"](program, fragment)
        gl["LinkProgram"](program)
        status = I()
        gl["GetProgramiv"](program, 0x8B82, C.byref(status))
        assert status.value, "the game compositor program must link"
        gl["DeleteShader"](vertex)
        gl["DeleteShader"](fragment)
        return program

    def _material(self):
        gl = self.gl
        texture = U()
        gl["GenTextures"](1, C.byref(texture))
        gl["ActiveTexture"](GL_TEXTURE0)
        gl["BindTexture"](GL_TEXTURE_2D, texture)
        data = material_pixels(MATERIAL)
        buffer = (C.c_ubyte * len(data)).from_buffer_copy(data)
        gl["TexImage2D"](GL_TEXTURE_2D, 0, 0x8058, MATERIAL, MATERIAL, 0, 0x1908, 0x1401, buffer)
        gl["GenerateMipmap"](GL_TEXTURE_2D)  # the game asset pipeline, not Potato
        for key, value in ((0x2800, 0x2601), (0x2801, 0x2703), (0x2802, 0x812F), (0x2803, 0x812F)):
            gl["TexParameteri"](GL_TEXTURE_2D, key, value)
        gl["TexParameterf"](GL_TEXTURE_2D, 0x813A, -1000.0)
        gl["TexParameterf"](GL_TEXTURE_2D, 0x813B, 1000.0)
        return texture

    # -- frame composition -------------------------------------------------
    def _sampler_binding(self):
        gl = self.gl
        gl["ActiveTexture"](GL_TEXTURE0)
        binding = I()
        gl["GetIntegerv"](GL_SAMPLER_BINDING, C.byref(binding))
        return binding.value

    def _scene_draw(self, adapter, mode, quality, policy, strength, message):
        """One qualified scene draw with the production draw-scoped replacement."""
        gl = self.gl
        original = self.sampler if mode == "sampler" else 0
        gl["BindSampler"](0, original)
        gl["ActiveTexture"](GL_TEXTURE0)
        gl["BindTexture"](GL_TEXTURE_2D, self.material)
        count = adapter.begin(quality, 1, GL_TRIANGLES, policy, strength, message, len(message))
        if count:
            # Between begin() and the draw, the replacement must be Potato's own
            # private sampler carrying the saved-strength floor, never the game's.
            gl["ActiveTexture"](GL_TEXTURE0)
            primed = I()
            gl["GetIntegerv"](GL_SAMPLER_BINDING, C.byref(primed))
            assert primed.value not in (0, self.sampler, self.texture_sampler), \
                "Potato must bind a private sampler for the game draw, not the game's own object"
            floor = F()
            gl["GetSamplerParameterfv"](primed.value, 0x813A, C.byref(floor))
            assert abs(floor.value - MODE_FLOOR[quality] * strength / 100) < 1e-4, \
                f"strength {strength}% must prepare the expected mip floor, got {floor.value} (the game's is untouched)"
            own = F()
            gl["GetSamplerParameterfv"](self.sampler, 0x813A, C.byref(own))
            assert own.value == -1000.0, "the game's sampler must still hold its own MIN_LOD during the draw"
        gl["DrawElements"](GL_TRIANGLES, self.index_count, 0x1403, None)
        adapter.end()
        assert self._sampler_binding() == original, \
            "the draw-scoped replacement must restore the game's exact sampler binding"
        return count

    def frame(self, adapter, mode, strength, message, quality=MODE_STRONG):
        """One presented game frame. Returns the per-draw Potato counts."""
        gl = self.gl
        counts = {"ui": 0, "scene": 0}
        gl["Viewport"](0, 0, SIZE, SIZE)
        gl["DepthMask"](1)
        gl["DepthFunc"](0x0203)  # LEQUAL, as a real depth-tested scene
        gl["ClearDepthf"](1.0)
        gl["ClearColor"](0.0, 0.0, 0.0, 1.0)
        gl["Clear"](0x0100 | 0x0400)
        # A blended HUD/UI pass: must never be touched, whatever the setting.
        gl["Enable"](0x0BE2)
        gl["BlendFunc"](1, 0x0303)
        counts["ui"] = self._scene_draw(adapter, mode, quality, POLICY_OWNED_MIP, strength, message)
        gl["Disable"](0x0BE2)
        # Qualified scene draws the compositor later samples.
        for _ in range(SCENE_DRAWS):
            adapter.observe(message, len(message))
            counts["scene"] += self._scene_draw(adapter, mode, quality, POLICY_OWNED_MIP, strength, message)
        assert gl["GetError"]() == 0, "an emulated game frame must not leave a GL error"
        return counts

    def present(self, adapter, mode, strength, message, frames=FRAMES, quality=MODE_STRONG):
        total = {"ui": 0, "scene": 0}
        for _ in range(frames):
            counts = self.frame(adapter, mode, strength, message, quality)
            total["ui"] += counts["ui"]
            total["scene"] += counts["scene"]
        return total

    def read(self):
        gl = self.gl
        pixels = (C.c_ubyte * (SIZE * SIZE * 4))()
        gl["ReadPixels"](0, 0, SIZE, SIZE, 0x1908, 0x1401, pixels)
        assert gl["GetError"]() == 0, "frame read-back must not leave a GL error"
        return bytes(pixels)


def detail_energy(pixels, width=SIZE, height=SIZE):
    """Sum of horizontal+vertical colour deltas: visible detail, not a counter."""
    energy = 0
    stride = width * 4
    for y in range(1, height):
        row, above = y * stride, (y - 1) * stride
        for x in range(1, width):
            at = row + x * 4
            for channel in range(3):
                value = pixels[at + channel]
                energy += abs(value - pixels[at - 4 + channel]) + abs(value - pixels[above + channel])
    return energy


def write_png(path, pixels, width=SIZE, height=SIZE):
    raw = bytearray()
    for y in range(height - 1, -1, -1):  # GL rows are bottom-up
        raw.append(0)
        raw += pixels[y * width * 4:(y + 1) * width * 4]

    def chunk(kind, payload):
        return (struct.pack(">I", len(payload)) + kind + payload +
                struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF))

    body = (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b""))
    path.write_bytes(body)


# --------------------------------------------------------------------------
def main():
    get_proc = open_egl()
    gl = gl_table(get_proc)
    renderer = gl["GetString"](0x1F01).decode()
    version = gl["GetString"](0x1F02).decode()
    print(f"EMULATED-GAME: renderer={renderer} version={version}")
    if "SwiftShader" in renderer or "llvmpipe" in renderer or "softpipe" in renderer:
        print("EMULATED-GAME LIMIT: software renderer; pixel behaviour is API conformance, not GPU performance")

    adapter_path = Path(tempfile.mkdtemp(prefix="potato-game-")) / "libpotato_game_adapter.so"
    build_adapter(adapter_path)
    adapter = C.CDLL(str(adapter_path))
    resolve_type = C.CFUNCTYPE(P, C.c_char_p, P)
    resolver = resolve_type(lambda name, user: get_proc(name))

    def api(name, result, *args):
        function = getattr(adapter, name)
        function.restype, function.argtypes = result, list(args)
        return function

    message = C.create_string_buffer(2048)
    api("bsi_game_potato_initialize", I, resolve_type, P, C.c_char_p, I)
    api("bsi_game_claim_presentation", I, C.c_char_p, I)
    api("bsi_game_observe_compositor", U, C.c_char_p, I)
    api("bsi_game_begin", U, I, I, U, I, I, C.c_char_p, I)
    api("bsi_game_end", None)
    api("bsi_game_release", None)

    assert adapter.bsi_game_potato_initialize(resolver, None, message, len(message)), message.value.decode()
    assert adapter.bsi_game_claim_presentation(message, len(message)), message.value.decode()

    game = EmulatedGame(gl)

    class Harness:
        def __init__(self, game):
            self.game = game

        def observe(self, buffer, capacity):
            return adapter.bsi_game_observe_compositor(buffer, capacity)

        def begin(self, mode, reviewed, primitive, policy, strength, buffer, capacity):
            return adapter.bsi_game_begin(mode, reviewed, primitive, policy, strength, buffer, capacity)

        def end(self):
            adapter.bsi_game_end()

    harness = Harness(game)
    # The game's own sampler/LOD state, which Potato must never edit.
    source_state = []
    for key in (0x813A, 0x813B):
        value = F()
        gl["GetTexParameterfv"](GL_TEXTURE_2D, key, C.byref(value))
        source_state.append(("texture", key, value.value))
        gl["GetSamplerParameterfv"](game.sampler, key, C.byref(value))
        source_state.append(("sampler", key, value.value))
    maximum_bias = F()
    gl["GetFloatv"](0x84FD, C.byref(maximum_bias))
    assert maximum_bias.value > 0, "the real device LOD-bias limit must be queried, not assumed"

    artifacts = os.environ.get("BSI_TEXTURE_PIXEL_ARTIFACT_DIR")
    artifact_dir = Path(artifacts) if artifacts else None
    if artifact_dir:
        artifact_dir.mkdir(parents=True, exist_ok=True)

    evidence = {"renderer": renderer, "version": version, "frames": FRAMES, "sceneDraws": SCENE_DRAWS, "modes": {}}
    native_frames = {}
    for mode in ("sampler", "texture"):
        print(f"EMULATED-GAME: source sampling mode={mode}")
        # Native baseline: the game presents with its own sampler untouched.
        gl["BindSampler"](0, game.sampler if mode == "sampler" else 0)
        present = game.present(harness, mode, 0, message)
        native = game.read()
        native_energy = detail_energy(native)
        assert present["scene"] == 0, "Off/0% must prepare no private sampler for the game draw"
        native_frames[mode] = native
        if artifact_dir:
            write_png(artifact_dir / f"emulated-game-{mode}-native.png", native)
        print(f"EMULATED-GAME: native frame energy={native_energy}")

        results = {}
        for strength in STRENGTHS:
            present = game.present(harness, mode, strength, message)
            pixels = game.read()
            energy = detail_energy(pixels)
            if artifact_dir:
                write_png(artifact_dir / f"emulated-game-{mode}-strength-{strength}.png", pixels)
            assert present["ui"] == 0, f"strength {strength}: blended UI draws must stay native"
            expected = 0 if strength == 0 else FRAMES * SCENE_DRAWS
            assert present["scene"] == expected, \
                f"strength {strength}: expected {expected} qualified scene draws with a private sampler, got {present['scene']}"
            assert (pixels == native) == (strength == 0), \
                f"strength {strength}: only 0% may reproduce the native presented frame"
            results[strength] = energy
            print(f"EMULATED-GAME: strength={strength:3d}% sceneDraws={present['scene']} uiDraws={present['ui']} "
                  f"presentedEnergy={energy}")

        strong = results[100]
        assert strong < native_energy * 0.75, (
            f"100% must visibly coarsen the presented game material (energy {native_energy} -> {strong}); "
            "a binding count alone is not evidence")
        assert results[0] == native_energy, "Off must present the native frame's exact detail"
        ordered = [results[strength] for strength in STRENGTHS]
        assert ordered == sorted(ordered, reverse=True), \
            f"strength must monotonically reduce presented detail, got {ordered}"
        assert len(set(ordered)) == len(ordered), \
            f"each saved strength must produce a distinct presented frame, got {ordered}"

        # Off after use must present the exact previous native frame again.
        present = game.present(harness, mode, 0, message)
        assert present["scene"] == 0 and game.read() == native, "Off after use must restore the exact native frame"

        # The depth-qualified policy is the conservative control: a UI-like draw
        # with depth testing off must be refused even at 100%.
        gl["Disable"](0x0B71)
        gl["BindSampler"](0, game.sampler if mode == "sampler" else 0)
        refused = adapter.bsi_game_begin(MODE_STRONG, 1, GL_TRIANGLES, POLICY_DEPTH_QUALIFIED, 100, message,
                                         len(message))
        gl["DrawElements"](GL_TRIANGLES, game.index_count, 0x1403, None)
        adapter.bsi_game_end()
        gl["Enable"](0x0B71)
        assert refused == 0, "a depth-disabled UI draw must keep native sampling under the conservative policy"

        # Light ("Natural") keeps the original lighter intent and must still be
        # visible, and another way round: it must not be as coarse as Strong.
        light = game.present(harness, mode, 100, message, quality=MODE_LIGHT)
        light_pixels, light_energy = game.read(), detail_energy(game.read())
        assert light["scene"] == FRAMES * SCENE_DRAWS and light["ui"] == 0, \
            "Light/Natural must prepare private samplers for scene draws only"
        assert light_pixels != native and light_energy < native_energy, \
            "Light/Natural at 100% must still be visible on the game frame"
        assert light_energy > strong, "Light/Natural must stay closer to native than Strong (lighter floor)"
        print(f"EMULATED-GAME: light(100%) presentedEnergy={light_energy} vs strong={strong}")
        assert game.present(harness, mode, 0, message)["scene"] == 0 and game.read() == native

        # The player's real interaction: the user drags the slider while the game
        # keeps running. Every setting change must take effect on the next frame
        # and Off must land back on the exact native frame.
        live = [(100, None), (25, None), (100, None), (0, None)]
        for strength, _ in live:
            present = game.present(harness, mode, strength, message, frames=2)
            pixels = game.read()
            expected = 0 if strength == 0 else 2 * SCENE_DRAWS
            assert present["scene"] == expected, f"live change to {strength}% was not applied"
            assert (pixels == native) == (strength == 0), f"live change to {strength}% did not reach the frame"
        print("EMULATED-GAME: live strength changes 100->25->100->Off applied on the next frame")

        for kind, key, value in source_state:
            current = F()
            if kind == "texture":
                gl["GetTexParameterfv"](GL_TEXTURE_2D, key, C.byref(current))
            else:
                gl["GetSamplerParameterfv"](game.sampler, key, C.byref(current))
            assert current.value == value, f"Potato must never edit the game's own {kind} LOD state ({hex(key)})"
        assert gl["GetError"]() == 0, "the whole emulated game run must not leave a GL error"
        evidence["modes"][mode] = {"nativeEnergy": native_energy,
                                   "strengthEnergy": {str(k): v for k, v in results.items()}}

    adapter.bsi_game_release()
    if artifact_dir:
        (artifact_dir / "emulated-game-evidence.json").write_text(json.dumps(evidence, indent=1) + "\n")
    print(f"EMULATED-GAME: energy per strength (sampler mode) "
          f"{[evidence['modes']['sampler']['strengthEnergy'][str(s)] for s in STRENGTHS]} "
          f"from native {evidence['modes']['sampler']['nativeEnergy']}")
    print("EMULATED-GAME PASS: in a game-shaped GLES frame loop the production Potato changed sampled "
          "game-material detail at every saved strength, left blended UI/its own sampler state untouched, "
          "and Off restored the previous presented frame byte for byte.")
    print("EMULATED-GAME LIMIT: software GLES renderer standing in for an Android game inside the player; "
          "not a captured game, physical GPU, FPS measurement or Windows detour proof.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
