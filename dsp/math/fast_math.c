#include "fast_math.h"
#include <math.h>

#define Q_ONE (1 << FIXED_POINT_Q)

#define TANH_CLAMP_POINT ((int16_t)(3 * Q_ONE)) // Point where tanh gets clamped to 1.0 in QA

int16_t q_tanh(int16_t x) {
    // Note: x and return value are in QA
    // Uses finite Lamber's series to 4 divisions (before simplification)

    if (x < -TANH_CLAMP_POINT)
        return -Q_ONE;

    if (x > TANH_CLAMP_POINT)
        return Q_ONE;

    int32_t x_squared = (x * x) >> FIXED_POINT_Q;

    int32_t a = ((10 * x * x_squared) >> FIXED_POINT_Q) + 105 * x;
    int32_t b =
        ((x_squared * x_squared) >> FIXED_POINT_Q) + (45 * x_squared) + (105 << FIXED_POINT_Q);

    return (int16_t)((a << FIXED_POINT_Q) / b);
}
