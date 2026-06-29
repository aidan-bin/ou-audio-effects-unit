#ifndef OU_FIXED_POINT_H
#define OU_FIXED_POINT_H

#include <stddef.h>

/*
 * FIXED_POINT_Q - number of fractional bits (N) in the QN fixed-point
 * format used for effect math.
 *
 * Portable DSP code is written to work for any N (default: 8).
 *
 * Larger N gives finer resolution, but smaller dynamic range. The practical limit is around half
 * the width of the integer type used (e.g., N=16 for int32_t) to avoid overflow in intermediate calculations.
 */
#ifndef FIXED_POINT_Q
#define FIXED_POINT_Q 8 // N in QN for fixed-point numbers
#endif

/* 1.0 in QN as an unsigned size_t coefficient (parameter domain). */
#define QN_ONE ((size_t)1u << FIXED_POINT_Q)

#endif
