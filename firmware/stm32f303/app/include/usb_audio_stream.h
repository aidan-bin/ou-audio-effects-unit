#ifndef USB_AUDIO_STREAM_H
#define USB_AUDIO_STREAM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define USB_AUDIO_SYNC 0xAAU
#define USB_AUDIO_MAX_PAYLOAD 512U
#define USB_AUDIO_RING_SIZE 2048U

#define USB_AUDIO_CMD_IN_DATA 0x01U
#define USB_AUDIO_CMD_IN_FLUSH 0x02U
#define USB_AUDIO_CMD_OUT_ENABLE 0x03U
#define USB_AUDIO_CMD_OUT_DISABLE 0x04U
#define USB_AUDIO_CMD_OUT_DATA 0x05U
#define USB_AUDIO_CMD_ACK 0x80U
#define USB_AUDIO_CMD_NACK 0x81U

typedef enum
{
    USB_AUDIO_PARSE_IDLE = 0,
    USB_AUDIO_PARSE_GOT_CMD,
    USB_AUDIO_PARSE_GOT_LEN_L,
    USB_AUDIO_PARSE_GOT_LEN_H,
    USB_AUDIO_PARSE_IN_PAYLOAD,
    USB_AUDIO_PARSE_GOT_CKSUM,
} UsbAudioParseState;

typedef struct
{
    int16_t in_ring[USB_AUDIO_RING_SIZE];
    volatile size_t in_head;
    volatile size_t in_tail;
    bool input_active;

    int16_t out_ring[USB_AUDIO_RING_SIZE];
    volatile size_t out_head;
    volatile size_t out_tail;
    bool output_active;

    UsbAudioParseState parse_state;
    uint8_t cmd;
    uint16_t payload_len;
    uint16_t payload_idx;
    uint8_t cksum;
    uint8_t payload_buf[USB_AUDIO_MAX_PAYLOAD];
} UsbAudioStream;

void usb_audio_stream_init(UsbAudioStream *stream);

bool usb_audio_stream_is_parsing_frame(const UsbAudioStream *stream);

void usb_audio_stream_push_byte(UsbAudioStream *stream, uint8_t byte);

bool usb_audio_stream_pop_sample(UsbAudioStream *stream, int16_t *sample_out);

bool usb_audio_stream_push_sample(UsbAudioStream *stream, int16_t sample);

bool usb_audio_stream_has_tx_frame(UsbAudioStream *stream);

bool usb_audio_stream_get_tx_frame(UsbAudioStream *stream, uint8_t *buf_out, size_t *len_out);

#endif
