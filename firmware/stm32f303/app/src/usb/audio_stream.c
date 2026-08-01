#include "usb/audio_stream.h"

#include <string.h>

void usb_audio_stream_init(UsbAudioStream *stream)
{
    if (stream == NULL)
    {
        return;
    }

    memset(stream, 0, sizeof(*stream));
}

void usb_audio_stream_reset(UsbAudioStream *stream)
{
    if (stream == NULL)
    {
        return;
    }

    /* Callers should hold taskENTER_CRITICAL (or equivalent) to prevent races with ISR writes */
    stream->in_head = 0U;
    stream->in_tail = 0U;
    stream->out_head = 0U;
    stream->out_tail = 0U;
    stream->in_dropped = 0U;
    stream->out_dropped = 0U;
}

void usb_audio_stream_reset_output(UsbAudioStream *stream)
{
    if (stream == NULL)
    {
        return;
    }

    /* Callers should hold taskENTER_CRITICAL (or equivalent) to prevent races with ISR writes */
    stream->out_head = 0U;
    stream->out_tail = 0U;
    stream->out_dropped = 0U;
}

static bool write_to_ring(int16_t *ring, volatile size_t *head,
                          volatile size_t *tail, int16_t sample,
                          size_t ring_size)
{
    size_t next_tail = (*tail + 1U) % ring_size;
    if (next_tail == *head)
    {
        return false;
    }

    ring[*tail] = sample;
    *tail = next_tail;
    return true;
}

static bool read_from_ring(const int16_t *ring, volatile size_t *head,
                           volatile size_t *tail, int16_t *sample,
                           size_t ring_size)
{
    if (*head == *tail)
    {
        return false;
    }

    *sample = ring[*head];
    *head = (*head + 1U) % ring_size;
    return true;
}

static size_t ring_available(const volatile size_t *head,
                             const volatile size_t *tail,
                             size_t ring_size)
{
    size_t h = *head;
    size_t t = *tail;
    if (t >= h)
    {
        return t - h;
    }
    return (ring_size - h) + t;
}

bool usb_audio_stream_pop_sample(UsbAudioStream *stream, int16_t *sample_out)
{
    if (stream == NULL || sample_out == NULL)
    {
        return false;
    }

    bool popped = read_from_ring(stream->in_ring, &stream->in_head,
                                 &stream->in_tail, sample_out, USB_AUDIO_RING_SIZE);
    return popped;
}

bool usb_audio_stream_pop_sample_or_hold(UsbAudioStream *stream, int16_t *sample_out)
{
    if (stream == NULL || sample_out == NULL)
    {
        return false;
    }

    if (usb_audio_stream_pop_sample(stream, sample_out))
    {
        return true;
    }

    /* Return silence on starvation. Repeating the last sample causes an audible
     * click at the effects-frame rate when the ring drains. */
    *sample_out = 0;
    stream->effects_zoh_holds++;
    return false;
}

bool usb_audio_stream_push_sample(UsbAudioStream *stream, int16_t sample)
{
    if (stream == NULL)
    {
        return false;
    }

    bool written = write_to_ring(stream->out_ring, &stream->out_head,
                                 &stream->out_tail, sample, USB_AUDIO_RING_SIZE);
    if (!written)
    {
        stream->out_dropped++;
    }
    return written;
}

size_t usb_audio_stream_out_available(const UsbAudioStream *stream)
{
    if (stream == NULL)
    {
        return 0U;
    }

    return ring_available(&stream->out_head, &stream->out_tail,
                          USB_AUDIO_RING_SIZE);
}

size_t usb_audio_stream_in_available(const UsbAudioStream *stream)
{
    if (stream == NULL)
    {
        return 0U;
    }

    return ring_available(&stream->in_head, &stream->in_tail,
                          USB_AUDIO_RING_SIZE);
}

size_t usb_audio_stream_pop_samples(UsbAudioStream *stream,
                                    int16_t *samples_out,
                                    size_t max_samples)
{
    if (stream == NULL || samples_out == NULL || max_samples == 0U)
    {
        return 0U;
    }

    size_t popped = 0U;
    while (popped < max_samples)
    {
        if (!read_from_ring(stream->out_ring, &stream->out_head,
                            &stream->out_tail, &samples_out[popped],
                            USB_AUDIO_RING_SIZE))
        {
            break;
        }
        popped++;
    }
    return popped;
}

void usb_audio_stream_push_bytes(UsbAudioStream *stream,
                                 const uint8_t *bytes,
                                 size_t byte_count)
{
    if (stream == NULL || bytes == NULL || byte_count < 2U)
    {
        return;
    }

    size_t pair_count = byte_count / 2U;
    for (size_t i = 0U; i < pair_count; i++)
    {
        int16_t sample =
            (int16_t)((uint16_t)bytes[i * 2U] |
                      ((uint16_t)bytes[i * 2U + 1U] << 8));
        if (!write_to_ring(stream->in_ring, &stream->in_head, &stream->in_tail,
                           sample, USB_AUDIO_RING_SIZE))
        {
            stream->in_dropped++;
        }
    }

    uint32_t fill = (uint32_t)usb_audio_stream_in_available(stream);
    if (fill > stream->in_ring_fill_peak)
    {
        stream->in_ring_fill_peak = fill;
    }
}

uint32_t usb_audio_stream_out_dropped(const UsbAudioStream *stream)
{
    return (stream == NULL) ? 0U : stream->out_dropped;
}

uint32_t usb_audio_stream_in_dropped(const UsbAudioStream *stream)
{
    return (stream == NULL) ? 0U : stream->in_dropped;
}

void usb_audio_stream_note_as_out_packet(UsbAudioStream *stream)
{
    if (stream != NULL)
    {
        stream->as_out_packets++;
    }
}

void usb_audio_stream_note_effects_frame(UsbAudioStream *stream)
{
    if (stream != NULL)
    {
        stream->effects_frames++;
    }
}

void usb_audio_stream_reset_diag(UsbAudioStream *stream)
{
    if (stream == NULL)
    {
        return;
    }
    stream->as_out_packets = 0U;
    stream->in_ring_fill_peak = 0U;
    stream->effects_zoh_holds = 0U;
    stream->effects_frames = 0U;
}

uint32_t usb_audio_stream_as_out_packets(const UsbAudioStream *stream)
{
    return (stream == NULL) ? 0U : stream->as_out_packets;
}

uint32_t usb_audio_stream_in_ring_fill_peak(const UsbAudioStream *stream)
{
    return (stream == NULL) ? 0U : stream->in_ring_fill_peak;
}

uint32_t usb_audio_stream_effects_zoh_holds(const UsbAudioStream *stream)
{
    return (stream == NULL) ? 0U : stream->effects_zoh_holds;
}

uint32_t usb_audio_stream_effects_frames(const UsbAudioStream *stream)
{
    return (stream == NULL) ? 0U : stream->effects_frames;
}
