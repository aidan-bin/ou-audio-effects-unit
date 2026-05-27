# Bindings for effects.c
#   Usage: Instantiate effect param objects, call <effect>(in_samples, <effect object>)

import ctypes
from pathlib import Path


def _load_effects_library():
    module_dir = Path(__file__).resolve().parent
    candidates = (
        module_dir / "libeffects.dll",
        module_dir / "libeffects.so",
        module_dir / "libeffects.dylib",
    )

    for candidate in candidates:
        if candidate.exists():
            return ctypes.CDLL(str(candidate))

    raise RuntimeError(
        "Could not find the effects shared library. "
        "Run ./scripts/build.sh from the repository root first."
    )


libeffects = _load_effects_library()

X_AXIS = int(65535 / 2)
fixed_point_q = 8

EFFECT_TYPE_OVERDRIVE = 0
EFFECT_TYPE_ECHO = 1
EFFECT_TYPE_COMPRESSION = 2


class OverdriveParam(ctypes.Structure):
    _fields_ = [("level", ctypes.c_size_t), # Saturation amplitude of wet signal
                ("gain", ctypes.c_size_t),  # Output gain in QN, fraction of level (note: level is for consistency,
                                            # keep gain at 1.0 and use level to control output)
                ("tone", ctypes.c_size_t),  # Tone in QN (lower tone softens saturation effect)
                ("mix", ctypes.c_size_t)]   # Wet/dry ratio in QN (0.0 = fully dry, 0.5 = equally wet/dry, 1.0 = fully wet)

    def __str__(self):
        return f"level = {self.level}\n" + \
               f"gain = {self.gain}\n" + \
               f"tone = {self.tone}\n" + \
               f"mix = {self.mix}"


class EchoParam(ctypes.Structure):
    _fields_ = [("delay_samples", ctypes.c_size_t), # Number of samples of delay for last echo of an input sample
                                                    # (determines max pre-delay and duration of echo)
                ("pre_delay", ctypes.c_size_t),     # Number of samples before first echo
                ("density", ctypes.c_size_t),       # Discrete, evenly-spaced echoes per sample in QN (after pre-delay,
                                                    # before end of delay samples)
                ("attack", ctypes.c_size_t),        # Gain on first echo (subsequent echoes have lower gain)
                ("decay", ctypes.c_size_t)]         # Amount of gain reduction per subsequent echo in QN
                                                    # (will saturate to 0 gain)

    def __str__(self):
        return f"delay_samples = {self.delay_samples}\n" + \
               f"pre_delay = {self.pre_delay}\n" + \
               f"density = {self.density}\n" + \
               f"attack = {self.attack}\n" + \
               f"decay = {self.decay}"


class CompressionParam(ctypes.Structure):
    _fields_ = [("threshold", ctypes.c_size_t), # Amplitude above which to apply gain reduction
                ("ratio", ctypes.c_size_t)]     # Amount of gain reduction to apply in QN (0 for no compression,
                                                # 1 for hard clipping)

    def __str__(self):
        return f"threshold = {self.threshold}\n" + \
               f"ratio = {self.ratio}"


class EffectParams(ctypes.Union):
    _fields_ = [
        ("overdrive", OverdriveParam),
        ("echo", EchoParam),
        ("compression", CompressionParam),
    ]


class EffectInstance(ctypes.Structure):
    _fields_ = [
        ("type", ctypes.c_int),
        ("params", EffectParams),
    ]


class EffectHandle(ctypes.Structure):
    _fields_ = [
        ("instance", EffectInstance),
        ("initialized", ctypes.c_int),
    ]


UINT16_PTR = ctypes.POINTER(ctypes.c_uint16)

libeffects.effect_handle_init.argtypes = [ctypes.POINTER(EffectHandle), ctypes.c_int]
libeffects.effect_handle_init.restype = ctypes.c_int

libeffects.effect_handle_reset.argtypes = [ctypes.POINTER(EffectHandle)]
libeffects.effect_handle_reset.restype = ctypes.c_int

libeffects.effect_handle_set_overdrive_params.argtypes = [
    ctypes.POINTER(EffectHandle),
    ctypes.POINTER(OverdriveParam),
]
libeffects.effect_handle_set_overdrive_params.restype = ctypes.c_int

libeffects.effect_handle_set_echo_params.argtypes = [
    ctypes.POINTER(EffectHandle),
    ctypes.POINTER(EchoParam),
]
libeffects.effect_handle_set_echo_params.restype = ctypes.c_int

libeffects.effect_handle_set_compression_params.argtypes = [
    ctypes.POINTER(EffectHandle),
    ctypes.POINTER(CompressionParam),
]
libeffects.effect_handle_set_compression_params.restype = ctypes.c_int

libeffects.effect_handle_process.argtypes = [
    ctypes.POINTER(EffectHandle),
    UINT16_PTR,
    UINT16_PTR,
    ctypes.c_size_t,
]
libeffects.effect_handle_process.restype = ctypes.c_int


class EffectRuntime:
    def __init__(self, effect_type):
        self.handle = EffectHandle()
        result = libeffects.effect_handle_init(ctypes.byref(self.handle), effect_type)
        if result != 0:
            raise RuntimeError(f"Failed to initialize effect runtime (type={effect_type})")

    def reset(self):
        result = libeffects.effect_handle_reset(ctypes.byref(self.handle))
        if result != 0:
            raise RuntimeError("Failed to reset effect runtime")

    def set_overdrive(self, param):
        result = libeffects.effect_handle_set_overdrive_params(ctypes.byref(self.handle), ctypes.byref(param))
        if result != 0:
            raise RuntimeError("Failed to set overdrive parameters")

    def set_echo(self, param):
        result = libeffects.effect_handle_set_echo_params(ctypes.byref(self.handle), ctypes.byref(param))
        if result != 0:
            raise RuntimeError("Failed to set echo parameters")

    def set_compression(self, param):
        result = libeffects.effect_handle_set_compression_params(ctypes.byref(self.handle), ctypes.byref(param))
        if result != 0:
            raise RuntimeError("Failed to set compression parameters")

    def _process(self, in_samples_list, out_samples_count):
        in_samples_array = (ctypes.c_uint16 * len(in_samples_list))(*in_samples_list)
        out_samples_array = (ctypes.c_uint16 * out_samples_count)()

        result = libeffects.effect_handle_process(
            ctypes.byref(self.handle),
            in_samples_array,
            out_samples_array,
            out_samples_count,
        )
        if result != 0:
            raise RuntimeError("Failed to process effect runtime")

        return self._decode_samples(out_samples_array, out_samples_count)

    @staticmethod
    def _encode_samples(in_samples_list):
        return [(round(sample) + X_AXIS) for sample in in_samples_list]

    @staticmethod
    def _decode_samples(out_samples_array, out_samples_count):
        return [(out_samples_array[i] - X_AXIS) for i in range(out_samples_count)]

    def process(self, in_samples_list):
        samples = self._encode_samples(in_samples_list)
        return self._process(samples, len(samples))

    def process_echo(self, in_samples_list, delay_samples):
        samples = self._encode_samples(in_samples_list)
        num_samples = len(samples) - int(delay_samples)

        if num_samples < 0:
            raise ValueError("Echo input must include delay_samples of padding")

        return self._process(samples, num_samples)
