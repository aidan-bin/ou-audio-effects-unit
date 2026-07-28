#include <stdint.h>
#include <stdio.h>

#include "usb/uac_feedback.h"
#include "harness/expect.h"

int failures = 0;

static void test_zero_period_is_defined_zero(void)
{
    expect_eq_u32(0U, usbd_uac_compute_feedback_value(1000U, 0U), "zero period returns 0");
}

static void test_nominal_rate_round_trips(void)
{
    /* 40506 samples/sec == 40.506 samples/ms; over 1000 ms that's exactly
     * 40506 samples, and the 10.14 value should be 40506 * 16384, rounded. */
    uint32_t value = usbd_uac_compute_feedback_value(40506U, 1000U);
    expect_eq_u32(40506U * 16384U / 1000U, value, "nominal rate over 1000ms");
}

static void test_rounds_to_nearest_not_truncated(void)
{
    /* 3 samples over 2 ms = 1.5 samples/ms -> 1.5 * 16384 = 24576 exactly. */
    expect_eq_u32(24576U, usbd_uac_compute_feedback_value(3U, 2U), "exact half-sample rate");

    /* 5 samples over 2 ms = 2.5 samples/ms -> 40960 exactly; nudge to check
     * rounding direction on a non-exact case: 5 samples over 3 ms = 1.667. */
    uint32_t exact_thirds = (5U * 16384U * 3U + 3U) / (3U * 3U); /* reference via wider math */
    (void)exact_thirds;
    uint32_t got = usbd_uac_compute_feedback_value(5U, 3U);
    /* 5*16384 = 81920; 81920/3 = 27306.67 -> rounds to 27307 */
    expect_eq_u32(27307U, got, "non-exact rate rounds to nearest");
}

static void test_zero_samples_is_zero_rate(void)
{
    expect_eq_u32(0U, usbd_uac_compute_feedback_value(0U, 32U), "no samples consumed -> zero rate");
}

int main(void)
{
    test_zero_period_is_defined_zero();
    test_nominal_rate_round_trips();
    test_rounds_to_nearest_not_truncated();
    test_zero_samples_is_zero_rate();

    if (failures == 0)
    {
        printf("usbd_uac_feedback tests passed\n");
    }
    return failures == 0 ? 0 : 1;
}
