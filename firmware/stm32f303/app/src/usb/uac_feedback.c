#include "usb/uac_feedback.h"

uint32_t usbd_uac_compute_feedback_value(uint32_t samples_consumed, uint32_t period_ms)
{
    if (period_ms == 0U)
    {
        return 0U;
    }

    /* UAC1 Full-Speed feedback: 10.10 fixed-point samples/frame in a 3-byte
     * payload (USB Audio 1.0 §3.7.2.2 / USB 2.0 §5.12.4.2). Rounded to nearest
     * so steady-state drift doesn't bias one direction. */
    uint64_t numerator = (uint64_t)samples_consumed * 1024ULL;
    uint64_t rounded = (numerator + (period_ms / 2U)) / period_ms;

    return (uint32_t)(rounded & 0xFFFFFFULL); /* field is 3 bytes (24 bits) */
}
