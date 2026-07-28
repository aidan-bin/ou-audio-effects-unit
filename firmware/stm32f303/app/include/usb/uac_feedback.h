/*
 * usbd_uac_feedback — UAC1 asynchronous feedback value computation.
 */

#ifndef USBD_UAC_FEEDBACK_H
#define USBD_UAC_FEEDBACK_H

#include <stdint.h>

/*
 * 10.14 fixed-point samples/frame implied by draining `samples_consumed`
 * samples over `period_ms` of host frames. Returns the 24-bit value for the
 * 3-byte feedback packet (little-endian). Returns 0 if `period_ms` is 0.
 */
uint32_t usbd_uac_compute_feedback_value(uint32_t samples_consumed, uint32_t period_ms);

#endif /* USBD_UAC_FEEDBACK_H */
