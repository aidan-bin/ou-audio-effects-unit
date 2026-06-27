#include "usb_audio_stream.h"

#include <string.h>

#define USB_AUDIO_FRAME_HEADER_SIZE 6U
#define USB_AUDIO_FRAME_OVERHEAD (USB_AUDIO_FRAME_HEADER_SIZE + 1U)

void usb_audio_stream_init(UsbAudioStream *stream)
{
    if (stream == NULL)
    {
        return;
    }

    memset(stream, 0, sizeof(*stream));
    stream->parse_state = USB_AUDIO_PARSE_IDLE;
}

bool usb_audio_stream_is_parsing_frame(const UsbAudioStream *stream)
{
    if (stream == NULL)
    {
        return false;
    }

    return stream->parse_state != USB_AUDIO_PARSE_IDLE;
}

static void reset_parser(UsbAudioStream *stream)
{
    stream->parse_state = USB_AUDIO_PARSE_IDLE;
    stream->cmd = 0;
    stream->payload_len = 0;
    stream->payload_idx = 0;
    stream->cksum = 0;
}

static uint8_t compute_xor_checksum(uint8_t cmd, uint16_t len, const uint8_t *payload)
{
    uint8_t ck = cmd ^ (uint8_t)(len & 0xFFU) ^ (uint8_t)((len >> 8) & 0xFFU);
    if (payload != NULL)
    {
        for (uint16_t i = 0; i < len; i++)
        {
            ck ^= payload[i];
        }
    }
    return ck;
}

static bool write_to_ring(int16_t *ring, volatile size_t *head, volatile size_t *tail,
                          int16_t sample, size_t ring_size)
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

static bool read_from_ring(const int16_t *ring, volatile size_t *head, volatile size_t *tail,
                           int16_t *sample, size_t ring_size)
{
    if (*head == *tail)
    {
        return false;
    }

    *sample = ring[*head];
    *head = (*head + 1U) % ring_size;
    return true;
}

static size_t ring_available(const volatile size_t *head, const volatile size_t *tail, size_t ring_size)
{
    size_t h = *head;
    size_t t = *tail;
    if (t >= h)
    {
        return t - h;
    }
    return (ring_size - h) + t;
}

static void dispatch_command(UsbAudioStream *stream)
{
    switch (stream->cmd)
    {
    case USB_AUDIO_CMD_IN_DATA:
        for (uint16_t i = 0; i + 1 < stream->payload_len; i += 2)
        {
            int16_t sample = (int16_t)((uint16_t)stream->payload_buf[i] |
                                       ((uint16_t)stream->payload_buf[i + 1] << 8));
            write_to_ring(stream->in_ring, &stream->in_head, &stream->in_tail,
                          sample, USB_AUDIO_RING_SIZE);
        }
        break;

    case USB_AUDIO_CMD_IN_FLUSH:
        stream->in_head = 0;
        stream->in_tail = 0;
        break;

    case USB_AUDIO_CMD_OUT_ENABLE:
        if (stream->payload_len >= 4)
        {
            uint32_t sample_rate_hint = (uint32_t)stream->payload_buf[0] |
                                        ((uint32_t)stream->payload_buf[1] << 8) |
                                        ((uint32_t)stream->payload_buf[2] << 16) |
                                        ((uint32_t)stream->payload_buf[3] << 24);
            (void)sample_rate_hint;
        }
        stream->output_active = true;
        break;

    case USB_AUDIO_CMD_OUT_DISABLE:
        stream->output_active = false;
        break;

    default:
        break;
    }
}

void usb_audio_stream_push_byte(UsbAudioStream *stream, uint8_t byte)
{
    if (stream == NULL)
    {
        return;
    }

    switch (stream->parse_state)
    {
    case USB_AUDIO_PARSE_IDLE:
        if (byte == USB_AUDIO_SYNC)
        {
            stream->parse_state = USB_AUDIO_PARSE_GOT_CMD;
            stream->cksum = 0;
        }
        break;

    case USB_AUDIO_PARSE_GOT_CMD:
        stream->cmd = byte;
        stream->cksum ^= byte;
        stream->parse_state = USB_AUDIO_PARSE_GOT_LEN_L;
        break;

    case USB_AUDIO_PARSE_GOT_LEN_L:
        stream->payload_len = (uint16_t)byte;
        stream->cksum ^= byte;
        stream->parse_state = USB_AUDIO_PARSE_GOT_LEN_H;
        break;

    case USB_AUDIO_PARSE_GOT_LEN_H:
        stream->payload_len |= (uint16_t)byte << 8;
        stream->cksum ^= byte;
        if (stream->payload_len > USB_AUDIO_MAX_PAYLOAD)
        {
            reset_parser(stream);
        }
        else if (stream->payload_len == 0)
        {
            uint8_t expected = compute_xor_checksum(stream->cmd, stream->payload_len, NULL);
            stream->parse_state = USB_AUDIO_PARSE_GOT_CKSUM;
            stream->cksum = expected;
        }
        else
        {
            stream->payload_idx = 0;
            stream->parse_state = USB_AUDIO_PARSE_IN_PAYLOAD;
        }
        break;

    case USB_AUDIO_PARSE_IN_PAYLOAD:
        stream->payload_buf[stream->payload_idx] = byte;
        stream->cksum ^= byte;
        stream->payload_idx++;
        if (stream->payload_idx >= stream->payload_len)
        {
            stream->parse_state = USB_AUDIO_PARSE_GOT_CKSUM;
        }
        break;

    case USB_AUDIO_PARSE_GOT_CKSUM:
        if (byte == stream->cksum)
        {
            dispatch_command(stream);
        }
        reset_parser(stream);
        break;

    default:
        reset_parser(stream);
        break;
    }
}

bool usb_audio_stream_pop_sample(UsbAudioStream *stream, int16_t *sample_out)
{
    if (stream == NULL || sample_out == NULL)
    {
        return false;
    }

    return read_from_ring(stream->in_ring, &stream->in_head, &stream->in_tail,
                          sample_out, USB_AUDIO_RING_SIZE);
}

bool usb_audio_stream_push_sample(UsbAudioStream *stream, int16_t sample)
{
    if (stream == NULL)
    {
        return false;
    }

    return write_to_ring(stream->out_ring, &stream->out_head, &stream->out_tail,
                         sample, USB_AUDIO_RING_SIZE);
}

bool usb_audio_stream_has_tx_frame(UsbAudioStream *stream)
{
    if (stream == NULL || !stream->output_active)
    {
        return false;
    }

    return ring_available(&stream->out_head, &stream->out_tail, USB_AUDIO_RING_SIZE) > 0;
}

bool usb_audio_stream_get_tx_frame(UsbAudioStream *stream, uint8_t *buf_out, size_t *len_out)
{
    if (stream == NULL || buf_out == NULL || len_out == NULL || !stream->output_active)
    {
        return false;
    }

    size_t available = ring_available(&stream->out_head, &stream->out_tail, USB_AUDIO_RING_SIZE);
    if (available == 0)
    {
        return false;
    }

    uint16_t sample_count = (uint16_t)(available < 200U ? available : 200U);
    uint16_t payload_len = sample_count * 2U;

    buf_out[0] = USB_AUDIO_SYNC;
    buf_out[1] = USB_AUDIO_CMD_OUT_DATA;
    buf_out[2] = (uint8_t)(payload_len & 0xFFU);
    buf_out[3] = (uint8_t)((payload_len >> 8) & 0xFFU);

    for (uint16_t i = 0; i < sample_count; i++)
    {
        int16_t sample = 0;
        if (read_from_ring(stream->out_ring, &stream->out_head, &stream->out_tail,
                           &sample, USB_AUDIO_RING_SIZE))
        {
            buf_out[4 + i * 2] = (uint8_t)(sample & 0xFFU);
            buf_out[4 + i * 2 + 1] = (uint8_t)((sample >> 8) & 0xFFU);
        }
        else
        {
            buf_out[4 + i * 2] = 0;
            buf_out[4 + i * 2 + 1] = 0;
        }
    }

    uint8_t ck = compute_xor_checksum(USB_AUDIO_CMD_OUT_DATA, payload_len, &buf_out[4]);
    buf_out[4 + payload_len] = ck;

    *len_out = USB_AUDIO_FRAME_HEADER_SIZE + (size_t)payload_len;
    return true;
}
