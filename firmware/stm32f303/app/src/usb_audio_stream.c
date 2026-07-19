#include "usb_audio_stream.h"

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

    return read_from_ring(stream->in_ring, &stream->in_head,
                          &stream->in_tail, sample_out, USB_AUDIO_RING_SIZE);
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
}

uint32_t usb_audio_stream_out_dropped(const UsbAudioStream *stream)
{
    return (stream == NULL) ? 0U : stream->out_dropped;
}

uint32_t usb_audio_stream_in_dropped(const UsbAudioStream *stream)
{
    return (stream == NULL) ? 0U : stream->in_dropped;
}
