/*
 * sample_convert — the canonical audio sample model and its conversions.
 *
 * Samples are left-aligned 12-bit uint16_t biased about X_AXIS = 32767,
 * with asymmetric swing (+32768 / -32767); conversions saturate, never wrap.
 */

#ifndef OU_SAMPLE_CONVERT_H
#define OU_SAMPLE_CONVERT_H

#include <stdint.h>

#define X_AXIS (UINT16_MAX / 2) /* 32767 */

static inline uint16_t usb_i16_to_sample(int16_t value)
{
    int32_t centered = (int32_t)X_AXIS + (int32_t)value;
    if (centered < 0)
    {
        return 0U;
    }
    if (centered > (int32_t)UINT16_MAX)
    {
        return (uint16_t)UINT16_MAX;
    }
    return (uint16_t)centered;
}

static inline int16_t sample_to_usb_i16(uint16_t sample)
{
    int32_t centered = (int32_t)sample - (int32_t)X_AXIS;
    if (centered > INT16_MAX)
    {
        return INT16_MAX;
    }
    if (centered < INT16_MIN)
    {
        return INT16_MIN;
    }
    return (int16_t)centered;
}

static inline int32_t sample_center(uint16_t sample)
{
    return (int32_t)sample - (int32_t)X_AXIS;
}

static inline uint16_t sample_uncenter_saturate(int32_t centered)
{
    int32_t biased = centered + (int32_t)X_AXIS;
    if (biased < 0)
    {
        return 0U;
    }
    if (biased > (int32_t)UINT16_MAX)
    {
        return (uint16_t)UINT16_MAX;
    }
    return (uint16_t)biased;
}

#endif /* OU_SAMPLE_CONVERT_H */
