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
        "Run ./scripts/run.sh build-demo from the repository root first."
    )


libeffects = _load_effects_library()

X_AXIS = int(65535 / 2)
FIXED_POINT_Q = 8

EFFECT_TYPE_OVERDRIVE = 0
EFFECT_TYPE_ECHO = 1
EFFECT_TYPE_COMPRESSION = 2


class OverdriveParam(ctypes.Structure):
    _fields_ = [
        ("level", ctypes.c_size_t),
        ("gain", ctypes.c_size_t),
        ("tone", ctypes.c_size_t),
        ("mix", ctypes.c_size_t),
    ]

    def __str__(self):
        return f"level = {self.level}\ngain = {self.gain}\ntone = {self.tone}\nmix = {self.mix}"


class EchoParam(ctypes.Structure):
    _fields_ = [
        ("delay_samples", ctypes.c_size_t),
        ("pre_delay", ctypes.c_size_t),
        ("density", ctypes.c_size_t),
        ("attack", ctypes.c_size_t),
        ("decay", ctypes.c_size_t),
        ("feedback", ctypes.c_size_t),
        ("feedback_delay", ctypes.c_size_t),
        ("damping", ctypes.c_size_t),
    ]

    def __str__(self):
        return (
            f"delay_samples = {self.delay_samples}\n"
            f"pre_delay = {self.pre_delay}\n"
            f"density = {self.density}\n"
            f"attack = {self.attack}\n"
            f"decay = {self.decay}\n"
            f"feedback = {self.feedback}\n"
            f"feedback_delay = {self.feedback_delay}\n"
            f"damping = {self.damping}"
        )


class CompressionParam(ctypes.Structure):
    _fields_ = [
        ("threshold", ctypes.c_size_t),
        ("ratio", ctypes.c_size_t),
    ]

    def __str__(self):
        return f"threshold = {self.threshold}\nratio = {self.ratio}"


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
        ("echo_state", ctypes.c_void_p),
    ]


UINT16_PTR = ctypes.POINTER(ctypes.c_uint16)

libeffects.effect_instance_init.argtypes = [ctypes.POINTER(EffectInstance), ctypes.c_int]
libeffects.effect_instance_init.restype = None

libeffects.effect_instance_reset.argtypes = [ctypes.POINTER(EffectInstance)]
libeffects.effect_instance_reset.restype = None

libeffects.effect_instance_set_overdrive_params.argtypes = [
    ctypes.POINTER(EffectInstance),
    ctypes.POINTER(OverdriveParam),
]
libeffects.effect_instance_set_overdrive_params.restype = ctypes.c_int

libeffects.effect_instance_set_echo_params.argtypes = [
    ctypes.POINTER(EffectInstance),
    ctypes.POINTER(EchoParam),
]
libeffects.effect_instance_set_echo_params.restype = ctypes.c_int

libeffects.effect_instance_set_compression_params.argtypes = [
    ctypes.POINTER(EffectInstance),
    ctypes.POINTER(CompressionParam),
]
libeffects.effect_instance_set_compression_params.restype = ctypes.c_int

libeffects.effect_instance_process.argtypes = [
    ctypes.POINTER(EffectInstance),
    UINT16_PTR,
    UINT16_PTR,
    ctypes.c_size_t,
]
libeffects.effect_instance_process.restype = ctypes.c_int


class EffectRuntime:
    def __init__(self, effect_type):
        self.instance = EffectInstance()
        libeffects.effect_instance_init(ctypes.byref(self.instance), effect_type)

    def reset(self):
        libeffects.effect_instance_reset(ctypes.byref(self.instance))

    def set_overdrive(self, param):
        result = libeffects.effect_instance_set_overdrive_params(
            ctypes.byref(self.instance), ctypes.byref(param)
        )
        if result != 0:
            raise RuntimeError("Failed to set overdrive parameters")

    def set_echo(self, param):
        result = libeffects.effect_instance_set_echo_params(
            ctypes.byref(self.instance), ctypes.byref(param)
        )
        if result != 0:
            raise RuntimeError("Failed to set echo parameters")

    def set_compression(self, param):
        result = libeffects.effect_instance_set_compression_params(
            ctypes.byref(self.instance), ctypes.byref(param)
        )
        if result != 0:
            raise RuntimeError("Failed to set compression parameters")

    def _process(self, in_samples_list, out_samples_count):
        in_samples_array = (ctypes.c_uint16 * len(in_samples_list))(*in_samples_list)
        out_samples_array = (ctypes.c_uint16 * out_samples_count)()

        result = libeffects.effect_instance_process(
            ctypes.byref(self.instance),
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
