#include "fast_math.h"
#include <math.h>

#define Q_ONE (1 << FIXED_POINT_Q)

// Clamp point where tanh saturates to +/-1.0 in QA.
#define TANH_CLAMP_POINT ((int16_t)(3 * Q_ONE))

#define TANH_COEFF_A1 10
#define TANH_COEFF_A2 105
#define TANH_COEFF_B1 45
#define TANH_COEFF_B2 105

int16_t q_tanh(int16_t x)
{
    // x and return value are QA fixed-point values.
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

    return (int16_t)((a << FIXED_POINT_Q) / b);
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
    return clamp_range(value, 0, (1U << FIXED_POINT_Q));
}

size_t clamp_min(size_t value, size_t min)
{
    if (value < min)
    {
        return min;
    }

    return value;
}
