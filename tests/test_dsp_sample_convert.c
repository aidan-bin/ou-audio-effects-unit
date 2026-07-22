/*
 * Host tests for sample_convert.h: bias, saturation and the X_AXIS=32767 asymmetry.
 */

#include <stdint.h>
#include <stdio.h>

#include "harness/expect.h"
#include "sample_convert.h"

int failures = 0;

static void test_x_axis_is_odd_midpoint(void)
{
    expect_eq_u16(32767U, (uint16_t)X_AXIS, "X_AXIS midpoint value");
}

static void test_usb_in_midscale_bias(void)
{
    expect_eq_u16((uint16_t)X_AXIS, usb_i16_to_sample(0), "usb_i16_to_sample(0) == X_AXIS");
}

static void test_usb_out_midscale_bias(void)
{
    expect_eq_i16(0, sample_to_usb_i16((uint16_t)X_AXIS), "sample_to_usb_i16(X_AXIS) == 0");
}

static void test_usb_in_full_scale_mapping(void)
{
    expect_eq_u16(65534U, usb_i16_to_sample(INT16_MAX), "usb_i16_to_sample(INT16_MAX)");
    expect_eq_u16(0U, usb_i16_to_sample(INT16_MIN), "usb_i16_to_sample(INT16_MIN) saturates");
}

static void test_usb_out_saturation(void)
{
    expect_eq_i16(INT16_MAX, sample_to_usb_i16(UINT16_MAX), "sample_to_usb_i16(UINT16_MAX) sat");
    expect_eq_i16((int16_t)-32767, sample_to_usb_i16(0U), "sample_to_usb_i16(0)");
}

static void test_usb_round_trip_identity_in_symmetric_range(void)
{
    for (int32_t s = -32767; s <= 32767; s += 7)
    {
        int16_t back = sample_to_usb_i16(usb_i16_to_sample((int16_t)s));
        expect_eq_i16((int16_t)s, back, "usb round-trip identity");
    }
}

static void test_usb_round_trip_negative_extreme(void)
{
    /* INT16_MIN saturates in (to 0) and returns as -32767; cannot round-trip exactly. */
    int16_t back = sample_to_usb_i16(usb_i16_to_sample(INT16_MIN));
    expect_eq_i16((int16_t)-32767, back, "INT16_MIN round-trips to -32767");
}

static void test_usb_in_never_wraps_monotonic(void)
{
    uint16_t prev = usb_i16_to_sample(INT16_MIN);
    for (int32_t s = INT16_MIN + 1; s <= INT16_MAX; s += 31)
    {
        uint16_t cur = usb_i16_to_sample((int16_t)s);
        expect_true(cur >= prev, "usb_i16_to_sample monotonic non-decreasing");
        prev = cur;
    }
}

int main(void)
{
    test_x_axis_is_odd_midpoint();
    test_usb_in_midscale_bias();
    test_usb_out_midscale_bias();
    test_usb_in_full_scale_mapping();
    test_usb_out_saturation();
    test_usb_round_trip_identity_in_symmetric_range();
    test_usb_round_trip_negative_extreme();
    test_usb_in_never_wraps_monotonic();

    if (failures != 0)
    {
        fprintf(stderr, "sample_convert tests failed: %d\n", failures);
        return 1;
    }

    puts("sample_convert tests passed");
    return 0;
}
