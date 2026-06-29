#ifndef FAST_MATH_H
#define FAST_MATH_H

#include <stddef.h>
#include <stdint.h>

#include "fixed_point.h"

int32_t q_tanh(int32_t x); // Returns tanh(x in QN) as int32_t QN

size_t clamp_range(size_t value, size_t min, size_t max); // Clamp to [min, max]
size_t clamp_qn(size_t value);                            // Clamp to [0, 1.0] in QN
size_t clamp_min(size_t value, size_t min);               // Clamp to [min, inf)

#endif
