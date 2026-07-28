#include "fast_math.h"
#include <math.h>

#define Q_ONE ((int32_t)1 << FIXED_POINT_Q)

// Clamp point where tanh saturates to +/-1.0 in QN.
#define TANH_CLAMP_POINT ((int32_t)(3 * Q_ONE))

#define TANH_COEFF_A1 10
#define TANH_COEFF_A2 105
#define TANH_COEFF_B1 45
#define TANH_COEFF_B2 105

int32_t q_tanh(int32_t x)
{
    // x and return value are QN fixed-point values.
    // Approximation from a truncated Lambert continued-fraction form.
    if (x < -TANH_CLAMP_POINT)
        return -Q_ONE;

    if (x > TANH_CLAMP_POINT)
        return Q_ONE;

    int64_t x64 = x;
    int64_t x_squared = (x64 * x64) >> FIXED_POINT_Q;

    int64_t a = ((TANH_COEFF_A1 * x64 * x_squared) >> FIXED_POINT_Q) + TANH_COEFF_A2 * x64;
    int64_t b = ((x_squared * x_squared) >> FIXED_POINT_Q) + (TANH_COEFF_B1 * x_squared) +
                ((int64_t)TANH_COEFF_B2 << FIXED_POINT_Q);

    return (int32_t)((a << FIXED_POINT_Q) / b);
}

size_t clamp_range(size_t value, size_t min, size_t max)
{
    if (value < min)
    {
        return min;
    }

    if (value > max)
    {
        return max;
    }

    return value;
}

size_t clamp_qn(size_t value)
{
    return clamp_range(value, 0, QN_ONE);
}

size_t clamp_min(size_t value, size_t min)
{
    return clamp_range(value, min, SIZE_MAX);
}

int32_t saturate_i32(int64_t v)
{
    if (v > INT32_MAX)
        return INT32_MAX;
    else if (v < INT32_MIN)
        return INT32_MIN;
    else
        return (int32_t)v;
}

int32_t saturate_min(int32_t num, int32_t min)
{
    if (num < min)
        return min;
    else
        return num;
}

uint16_t saturate_u16(int32_t num)
{
    if (num < 0)
    {
        return 0;
    }

    if (num > (int32_t)UINT16_MAX)
    {
        return UINT16_MAX;
    }

    return (uint16_t)num;
}
