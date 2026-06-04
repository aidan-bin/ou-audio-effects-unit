#include "fast_math.h"
#include <math.h>

#define Q_ONE (1 << FIXED_POINT_Q)

// Clamp point where tanh saturates to +/-1.0 in QA.
#define TANH_CLAMP_POINT ((int16_t)(3 * Q_ONE))

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

    int64_t a = ((10 * x64 * x_squared) >> FIXED_POINT_Q) + 105 * x64;
    int64_t b = ((x_squared * x_squared) >> FIXED_POINT_Q) + (45 * x_squared) +
                ((int64_t)105 << FIXED_POINT_Q);

    return (int16_t)((a << FIXED_POINT_Q) / b);
}
