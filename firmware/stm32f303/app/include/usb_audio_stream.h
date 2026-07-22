/*
 * usb_audio_stream — CDC1 audio stream ring buffers.
 */

#ifndef USB_AUDIO_STREAM_H
#define USB_AUDIO_STREAM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define USB_AUDIO_RING_SIZE 2048U

typedef struct
{
    int16_t in_ring[USB_AUDIO_RING_SIZE];
    volatile size_t in_head;
    volatile size_t in_tail;

    int16_t out_ring[USB_AUDIO_RING_SIZE];
    volatile size_t out_head;
    volatile size_t out_tail;
    volatile bool output_active;

    volatile uint32_t in_dropped;
    volatile uint32_t out_dropped;
} UsbAudioStream;

void usb_audio_stream_init(UsbAudioStream *stream);

void usb_audio_stream_reset(UsbAudioStream *stream);

void usb_audio_stream_reset_output(UsbAudioStream *stream);

bool usb_audio_stream_pop_sample(UsbAudioStream *stream, int16_t *sample_out);

bool usb_audio_stream_push_sample(UsbAudioStream *stream, int16_t sample);

/*
 * Bulk helpers for the dual-CDC transport.
 * Samples are packed as little-endian int16_t.
 */

/* Pop up to `max_samples` samples from out_ring into `samples_out`.
 * Returns the number of samples actually popped. */
size_t usb_audio_stream_pop_samples(UsbAudioStream *stream,
                                    int16_t *samples_out,
                                    size_t max_samples);

/* Push raw little-endian int16_t byte pairs into the in_ring.
 * `byte_count` must be even; odd trailing bytes are ignored. */
void usb_audio_stream_push_bytes(UsbAudioStream *stream,
                                 const uint8_t *bytes,
                                 size_t byte_count);

/* Number of samples available in out_ring. */
size_t usb_audio_stream_out_available(const UsbAudioStream *stream);

/* Diagnostic counters: total samples dropped on a full ring. */
uint32_t usb_audio_stream_out_dropped(const UsbAudioStream *stream);
uint32_t usb_audio_stream_in_dropped(const UsbAudioStream *stream);

#endif
