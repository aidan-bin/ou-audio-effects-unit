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

static void test_reset_clears_rings(void)
{
    UsbAudioStream stream;
    usb_audio_stream_init(&stream);

    for (size_t i = 0; i < 5; i++)
    {
        usb_audio_stream_push_sample(&stream, (int16_t)(100 + (int)i));
    }
    uint8_t bytes[4] = {0x01, 0x00, 0x02, 0x00};
    usb_audio_stream_push_bytes(&stream, bytes, sizeof(bytes));

    stream.in_dropped = 42;
    stream.out_dropped = 99;

    usb_audio_stream_reset(&stream);

    int16_t s = 0;
    expect_false(usb_audio_stream_pop_sample(&stream, &s), "in_ring empty after reset");
    expect_eq_size(0, usb_audio_stream_pop_samples(&stream, &s, 1), "out_ring empty after reset");
    expect_eq_u32(0, usb_audio_stream_out_dropped(&stream), "out_dropped zeroed");
    expect_eq_u32(0, usb_audio_stream_in_dropped(&stream), "in_dropped zeroed");
    expect_eq_size(0, usb_audio_stream_out_available(&stream), "out_available zero after reset");
}

static void test_reset_null_safe(void)
{
    usb_audio_stream_reset(NULL);
}

static void test_reset_output_leaves_input_intact(void)
{
    UsbAudioStream stream;
    usb_audio_stream_init(&stream);

    for (size_t i = 0; i < 5; i++)
    {
        usb_audio_stream_push_sample(&stream, (int16_t)(100 + (int)i));
    }

    uint8_t bytes[6] = {0x01, 0x00, 0x02, 0x00, 0x03, 0x00};
    usb_audio_stream_push_bytes(&stream, bytes, sizeof(bytes));

    stream.in_dropped = 7;
    stream.out_dropped = 42;

    usb_audio_stream_reset_output(&stream);

    int16_t s = 0;
    expect_eq_u32(0, usb_audio_stream_out_dropped(&stream), "out_dropped zeroed");
    expect_eq_size(0, usb_audio_stream_out_available(&stream), "out_available zero after reset_output");

    expect_true(usb_audio_stream_pop_sample(&stream, &s), "in_ring intact after reset_output");
    expect_eq_u32(7, usb_audio_stream_in_dropped(&stream), "in_dropped untouched");
}

static void test_reset_output_null_safe(void)
{
    usb_audio_stream_reset_output(NULL);
}

static void test_in_ring_overflow_drops(void)
{
    UsbAudioStream stream;
    usb_audio_stream_init(&stream);

    size_t capacity = USB_AUDIO_RING_SIZE - 1U;
    for (size_t i = 0; i < capacity; i++)
    {
        uint8_t b[2] = {(uint8_t)(i & 0xFF), (uint8_t)((i >> 8) & 0xFF)};
        usb_audio_stream_push_bytes(&stream, b, sizeof(b));
    }
    expect_eq_u32(0, usb_audio_stream_in_dropped(&stream), "no drops while filling in_ring to capacity");

    uint8_t overflow[2] = {0xFF, 0x7F};
    usb_audio_stream_push_bytes(&stream, overflow, sizeof(overflow));
    expect_eq_u32(1, usb_audio_stream_in_dropped(&stream), "in_ring drop counted on overflow");
}

static void test_push_bytes_cdc_packet_size(void)
{
    UsbAudioStream stream;
    usb_audio_stream_init(&stream);

    uint8_t packet[64];
    for (int pkt = 0; pkt < 16; pkt++)
    {
        for (int i = 0; i < 32; i++)
        {
            int16_t val = (int16_t)((pkt * 32 + i) & 0x7FFF);
            packet[(size_t)i * 2] = (uint8_t)(val & 0xFF);
            packet[(size_t)i * 2 + 1] = (uint8_t)((val >> 8) & 0xFF);
        }
        usb_audio_stream_push_bytes(&stream, packet, sizeof(packet));
    }

    expect_eq_u32(0, usb_audio_stream_in_dropped(&stream), "no drops for 16 full CDC packets");

    int16_t expected = 0;
    int16_t actual = 0;
    while (usb_audio_stream_pop_sample(&stream, &actual))
    {
        expect_eq_i16(expected, actual, "cdc packet sample in order");
        expected = (int16_t)(((int)expected + 1) & 0x7FFF);
    }
    expect_eq_i16(512, expected, "exactly 512 samples (16*32) popped");
}

static void test_interleaved_isr_push_task_pop(void)
{
    UsbAudioStream stream;
    usb_audio_stream_init(&stream);

    int16_t expected[405];
    int16_t received[405];

    for (int frame = 0; frame < 8; frame++)
    {
        for (size_t i = 0; i < 405; i++)
        {
            expected[i] = (int16_t)((frame * 405 + (int)i) & 0xFFFF);
        }

        for (size_t i = 0; i < 405; i++)
        {
            usb_audio_stream_push_sample(&stream, expected[i]);
        }

        size_t popped = usb_audio_stream_pop_samples(&stream, received, 405);
        expect_eq_size(405, popped, "full frame popped");

        for (size_t i = 0; i < 405; i++)
        {
            if (expected[i] != received[i])
            {
                expect_eq_i16(expected[i], received[i], "interleaved frame sample match");
                break;
            }
        }
    }

    expect_eq_u32(0, usb_audio_stream_out_dropped(&stream), "no drops during interleaved");
    expect_eq_size(0, usb_audio_stream_out_available(&stream), "ring drained after interleaved");
}

static void test_interleaved_in_ring_bytes_task_pop(void)
{
    UsbAudioStream stream;
    usb_audio_stream_init(&stream);

    uint8_t packet[64];
    int16_t popped[32];
    int16_t next_val = 0;
    size_t total_popped = 0;

    for (int pkt = 0; pkt < 256; pkt++)
    {
        for (int i = 0; i < 32; i++)
        {
            packet[(size_t)i * 2] = (uint8_t)(next_val & 0xFF);
            packet[(size_t)i * 2 + 1] = (uint8_t)((next_val >> 8) & 0xFF);
            next_val++;
        }
        usb_audio_stream_push_bytes(&stream, packet, sizeof(packet));

        if (pkt % 12 == 11)
        {
            size_t n = 0;
            int16_t s;
            while (usb_audio_stream_pop_sample(&stream, &s))
            {
                popped[n++] = s;
                total_popped++;
                if (n >= 32)
                    break;
            }
        }
    }

    int16_t s;
    while (usb_audio_stream_pop_sample(&stream, &s))
    {
        total_popped++;
    }

    size_t total_in = (size_t)256 * 32;
    size_t drops = (size_t)usb_audio_stream_in_dropped(&stream);
    expect_eq_size(total_in, total_popped + drops, "in_ring: all samples accounted (popped + dropped = pushed)");
}

static void test_push_bytes_at_wrap_boundary(void)
{
    UsbAudioStream stream;
    usb_audio_stream_init(&stream);

    size_t capacity = USB_AUDIO_RING_SIZE - 1U;
    for (size_t i = 0; i < capacity - 1; i++)
    {
        uint8_t b[2] = {(uint8_t)((i + 1) & 0xFF), (uint8_t)(((i + 1) >> 8) & 0xFF)};
        usb_audio_stream_push_bytes(&stream, b, sizeof(b));
    }

    uint8_t at_wrap[6] = {0x01, 0x01, 0x02, 0x01, 0x03, 0x01};
    usb_audio_stream_push_bytes(&stream, at_wrap, sizeof(at_wrap));

    expect_eq_u32(2, usb_audio_stream_in_dropped(&stream), "two pairs dropped at wrap (1 slot free, 3 pushed)");

    int16_t first = 0;
    expect_true(usb_audio_stream_pop_sample(&stream, &first), "first sample after wrap");
    expect_eq_i16(1, first, "first pushed sample preserved");
}

static void test_throughput_stress(void)
{
    UsbAudioStream stream;
    usb_audio_stream_init(&stream);

    const size_t total_samples = 40500;
    uint8_t pair[2];

    for (size_t i = 0; i < total_samples; i++)
    {
        uint16_t val = (uint16_t)(i & 0xFFFF);
        pair[0] = (uint8_t)(val & 0xFF);
        pair[1] = (uint8_t)((val >> 8) & 0xFF);
        usb_audio_stream_push_bytes(&stream, pair, sizeof(pair));
    }

    size_t burst_size = 405;
    int16_t buf[405];
    size_t total_popped = 0;
    for (;;)
    {
        size_t n = 0;
        int16_t s;
        while (n < burst_size && usb_audio_stream_pop_sample(&stream, &s))
        {
            buf[n++] = s;
        }
        if (n == 0)
            break;
        total_popped += n;

        for (size_t j = 0; j < n; j++)
        {
            int16_t expected_val = (int16_t)((total_popped - n + j) & 0xFFFF);
            if (buf[j] != expected_val)
            {
                expect_eq_i16(expected_val, buf[j], "throughput stress sample correct");
                break;
            }
        }
    }

    size_t drops = (size_t)usb_audio_stream_in_dropped(&stream);
    expect_eq_size(total_samples, total_popped + drops, "throughput: total pushed = popped + dropped");
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
    usb_audio_stream_push_bytes(NULL, (const uint8_t *)&sample, 1);
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
    test_in_ring_overflow_drops();
    test_push_bytes_cdc_packet_size();
    test_interleaved_isr_push_task_pop();
    test_interleaved_in_ring_bytes_task_pop();
    test_push_bytes_at_wrap_boundary();
    test_throughput_stress();
    test_null_safe();
    test_reset_clears_rings();
    test_reset_null_safe();
    test_reset_output_leaves_input_intact();
    test_reset_output_null_safe();

    if (failures != 0)
    {
        fprintf(stderr, "usb_audio_stream tests failed: %d\n", failures);
        return 1;
    }

    puts("usb_audio_stream tests passed");
    return 0;
}
