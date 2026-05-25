# Bindings for effects.c
#   Usage: Instantiate effect param objects, call <effect>(in_samples, <effect object>)

import ctypes
import os

libpath = os.path.abspath("libeffects.dll")
libeffects = ctypes.CDLL(libpath)

x_axis = int(65535/2)
fixed_point_q = 8
param_max = 2 ** 16 - 1  # 16-bit unsigned integer max


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


def overdrive(in_samples_list, param):
    in_samples_list = [(round(sample) + x_axis) for sample in in_samples_list]
    num_samples = len(in_samples_list)
    in_samples_array = (ctypes.c_uint16 * num_samples)(*in_samples_list)
    out_samples_array = (ctypes.c_uint16 * num_samples)()
    libeffects.buf_overdrive(ctypes.cast(in_samples_array, ctypes.POINTER(ctypes.c_uint16)),
                             ctypes.cast(out_samples_array, ctypes.POINTER(ctypes.c_uint16)),
                             num_samples, ctypes.byref(param))
    return [(out_samples_array[i] - x_axis) for i in range(num_samples)]


def echo(in_samples_list, param):
    in_samples_list = [(round(sample) + x_axis) for sample in in_samples_list]
    num_prev_samples = param.delay_samples
    num_curr_samples = len(in_samples_list) - num_prev_samples
    in_samples_array = (ctypes.c_uint16 * len(in_samples_list))(*in_samples_list)
    out_samples_array = (ctypes.c_uint16 * num_curr_samples)()
    libeffects.buf_echo(ctypes.cast(in_samples_array, ctypes.POINTER(ctypes.c_uint16)),
                        ctypes.cast(out_samples_array, ctypes.POINTER(ctypes.c_uint16)),
                        num_curr_samples, ctypes.byref(param))
    return [(out_samples_array[i] - x_axis) for i in range(num_curr_samples)]


def compression(in_samples_list, param):
    in_samples_list = [(round(sample) + x_axis) for sample in in_samples_list]
    num_samples = len(in_samples_list)
    in_samples_array = (ctypes.c_uint16 * num_samples)(*in_samples_list)
    out_samples_array = (ctypes.c_uint16 * num_samples)()
    libeffects.buf_compression(ctypes.cast(in_samples_array, ctypes.POINTER(ctypes.c_uint16)),
                               ctypes.cast(out_samples_array, ctypes.POINTER(ctypes.c_uint16)),
                               num_samples, ctypes.byref(param))
    return [(out_samples_array[i] - x_axis) for i in range(num_samples)]


# Saturate num to +/- amp
def saturate(num, amp):
    if num > amp:
        return amp
    elif num < -amp:
        return -amp
    else:
        return num
