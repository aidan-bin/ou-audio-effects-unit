#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "usb_audio_stream.h"

#include "harness/expect.h"

int failures = 0;

static void test_init_clears_state(void)
{
    UsbAudioStream stream;
    stream.in_head = 7;
    stream.out_tail = 9;
    stream.in_dropped = 5;
    stream.out_dropped = 5;
    stream.output_active = true;

    usb_audio_stream_init(&stream);

    expect_eq_size(0, usb_audio_stream_out_available(&stream), "out empty after init");
    expect_eq_u32(0, usb_audio_stream_out_dropped(&stream), "out_dropped zero after init");
    expect_eq_u32(0, usb_audio_stream_in_dropped(&stream), "in_dropped zero after init");
    expect_false(stream.output_active, "output_active cleared after init");
}

static void test_push_pop_roundtrip(void)
{
    UsbAudioStream stream;
    usb_audio_stream_init(&stream);

    expect_true(usb_audio_stream_push_sample(&stream, 1234), "push sample succeeds");
    expect_true(usb_audio_stream_push_sample(&stream, -4321), "push second sample succeeds");
    expect_eq_size(2, usb_audio_stream_out_available(&stream), "two samples available");

    int16_t out[4] = {0};
    size_t popped = usb_audio_stream_pop_samples(&stream, out, 4);
    expect_eq_size(2, popped, "pop returns both staged samples");
    expect_eq_i16(1234, out[0], "first sample preserved (FIFO order)");
    expect_eq_i16(-4321, out[1], "second sample preserved");
    expect_eq_size(0, usb_audio_stream_out_available(&stream), "ring empty after drain");
}

static void test_pop_empty_returns_zero(void)
{
    UsbAudioStream stream;
    usb_audio_stream_init(&stream);

    int16_t out[2] = {0};
    expect_eq_size(0, usb_audio_stream_pop_samples(&stream, out, 2), "pop on empty returns 0");

    int16_t in_sample = 0;
    expect_false(usb_audio_stream_pop_sample(&stream, &in_sample), "pop_sample on empty in_ring is false");
}

static void test_partial_drain(void)
{
    UsbAudioStream stream;
    usb_audio_stream_init(&stream);

    for (int16_t i = 0; i < 5; i++)
    {
        expect_true(usb_audio_stream_push_sample(&stream, (int16_t)(100 + i)), "push for partial drain");
    }

    int16_t out[3] = {0};
    expect_eq_size(3, usb_audio_stream_pop_samples(&stream, out, 3), "pop only requested count");
    expect_eq_i16(100, out[0], "partial drain keeps FIFO order");
    expect_eq_size(2, usb_audio_stream_out_available(&stream), "two samples remain after partial drain");
}

static void test_wraparound(void)
{
    UsbAudioStream stream;
    usb_audio_stream_init(&stream);

    for (int i = 0; i < (int)(USB_AUDIO_RING_SIZE * 4); i++)
    {
        int16_t value = (int16_t)(i & 0x7FFF);
        expect_true(usb_audio_stream_push_sample(&stream, value), "push during wrap");

        int16_t out = 0;
        size_t popped = usb_audio_stream_pop_samples(&stream, &out, 1);
        expect_eq_size(1, popped, "pop during wrap");
        expect_eq_i16(value, out, "value preserved across wrap");
    }
    expect_eq_u32(0, usb_audio_stream_out_dropped(&stream), "no drops during balanced wrap");
}

static void test_overflow_drops_and_counts(void)
{
    UsbAudioStream stream;
    usb_audio_stream_init(&stream);

    size_t capacity = USB_AUDIO_RING_SIZE - 1U;
    for (size_t i = 0; i < capacity; i++)
    {
        expect_true(usb_audio_stream_push_sample(&stream, (int16_t)i), "fill to capacity");
    }
    expect_eq_size(capacity, usb_audio_stream_out_available(&stream), "ring at capacity");
    expect_eq_u32(0, usb_audio_stream_out_dropped(&stream), "no drops while filling to capacity");

    expect_false(usb_audio_stream_push_sample(&stream, 999), "push on full ring fails");
    expect_eq_u32(1, usb_audio_stream_out_dropped(&stream), "drop counted on full ring");
    expect_false(usb_audio_stream_push_sample(&stream, 999), "second overflow fails");
    expect_eq_u32(2, usb_audio_stream_out_dropped(&stream), "drop counter accumulates");
}

static void test_push_bytes_little_endian(void)
{
    UsbAudioStream stream;
    usb_audio_stream_init(&stream);

    // 0x0102 = 258, 0xFFFE = -2
    const uint8_t bytes[] = {0x02, 0x01, 0xFE, 0xFF};
    usb_audio_stream_push_bytes(&stream, bytes, sizeof(bytes));

    int16_t sample = 0;
    expect_true(usb_audio_stream_pop_sample(&stream, &sample), "in_ring sample available");
    expect_eq_i16(258, sample, "little-endian decode of 0x0102");
    expect_true(usb_audio_stream_pop_sample(&stream, &sample), "second in_ring sample available");
    expect_eq_i16(-2, sample, "little-endian decode of 0xFFFE");
    expect_false(usb_audio_stream_pop_sample(&stream, &sample), "in_ring drained");
}

static void test_push_bytes_ignores_odd_trailing(void)
{
    UsbAudioStream stream;
    usb_audio_stream_init(&stream);

    // Two full samples + one dangling byte that must be ignored
    const uint8_t bytes[] = {0x01, 0x00, 0x02, 0x00, 0xAA};
    usb_audio_stream_push_bytes(&stream, bytes, sizeof(bytes));

    int16_t sample = 0;
    expect_true(usb_audio_stream_pop_sample(&stream, &sample), "first sample present");
    expect_eq_i16(1, sample, "first decoded sample");
    expect_true(usb_audio_stream_pop_sample(&stream, &sample), "second sample present");
    expect_eq_i16(2, sample, "second decoded sample");
    expect_false(usb_audio_stream_pop_sample(&stream, &sample), "dangling byte produced no sample");

    const uint8_t lone[] = {0x55};
    usb_audio_stream_push_bytes(&stream, lone, sizeof(lone));
    expect_false(usb_audio_stream_pop_sample(&stream, &sample), "single byte ignored");
}

static void test_null_safe(void)
{
    int16_t sample = 0;
    expect_false(usb_audio_stream_push_sample(NULL, 0), "push_sample NULL safe");
    expect_false(usb_audio_stream_pop_sample(NULL, &sample), "pop_sample NULL safe");
    expect_eq_size(0, usb_audio_stream_pop_samples(NULL, &sample, 1), "pop_samples NULL safe");
    expect_eq_size(0, usb_audio_stream_out_available(NULL), "out_available NULL safe");
    expect_eq_u32(0, usb_audio_stream_out_dropped(NULL), "out_dropped NULL safe");
    expect_eq_u32(0, usb_audio_stream_in_dropped(NULL), "in_dropped NULL safe");
    // Does not crash
    usb_audio_stream_push_bytes(NULL, (const uint8_t *)&sample, 2);
}

int main(void)
{
    test_init_clears_state();
    test_push_pop_roundtrip();
    test_pop_empty_returns_zero();
    test_partial_drain();
    test_wraparound();
    test_overflow_drops_and_counts();
    test_push_bytes_little_endian();
    test_push_bytes_ignores_odd_trailing();
    test_null_safe();

    if (failures != 0)
    {
        fprintf(stderr, "usb_audio_stream tests failed: %d\n", failures);
        return 1;
    }

    puts("usb_audio_stream tests passed");
    return 0;
}
