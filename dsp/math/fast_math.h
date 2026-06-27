#ifndef FAST_MATH_H
#define FAST_MATH_H

#include <stddef.h>
#include <stdint.h>
// #include "bam.h"

#ifndef FIXED_POINT_Q
#define FIXED_POINT_Q 8 // N in QN for fixed-point numbers
#endif

// int16_t bam_sin(bam_t x);   // Returns sin(x in BAM) as int16_t QA
int16_t q_tanh(int16_t x); // Returns tanh(x in QA) as int16_t QA
// int16_t q_exp(int16_t x);   // Returns exp(x in QA) as int16_t QA

// Clamp helpers shared by the DSP effect implementations.
size_t clamp_range(size_t value, size_t min, size_t max); // Clamp to [min, max]
size_t clamp_qn(size_t value);                            // Clamp to [0, 1.0] in QN
size_t clamp_min(size_t value, size_t min);               // Clamp to [min, inf)

#endif
