#include "usb/uac_feedback.h"

uint32_t usbd_uac_compute_feedback_value(uint32_t samples_consumed, uint32_t period_ms)
{
    if (period_ms == 0U)
    {
        return 0U;
    }

    /* 10.14 fixed-point samples/frame: value = (samples_consumed / period_ms) * 2^14,
     * rounded to nearest rather than truncated so small steady-state drift doesn't
     * get rounded the same direction every time. */
    uint64_t numerator = (uint64_t)samples_consumed * 16384ULL;
    uint64_t rounded = (numerator + (period_ms / 2U)) / period_ms;

    return (uint32_t)(rounded & 0xFFFFFFULL); /* field is 3 bytes (24 bits) */
}
