/*
 * usb_audio_stream — sample ring buffers shared between USB and DSP.
 */

#ifndef USB_AUDIO_STREAM_H
#define USB_AUDIO_STREAM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define USB_AUDIO_RING_SIZE 2048U

typedef struct UsbAudioStream
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

    /* Diagnostic counters (never reset by normal operation). */
    volatile uint32_t as_out_packets;    /* AS-OUT ISR callback count */
    volatile uint32_t in_ring_fill_peak; /* Peak in_ring occupancy since reset */
    volatile uint32_t effects_zoh_holds; /* pop_sample_or_hold falling back to hold */
    volatile uint32_t effects_frames;    /* Frames processed by effects task */
} UsbAudioStream;

void usb_audio_stream_init(UsbAudioStream *stream);

void usb_audio_stream_reset(UsbAudioStream *stream);

void usb_audio_stream_reset_output(UsbAudioStream *stream);

bool usb_audio_stream_pop_sample(UsbAudioStream *stream, int16_t *sample_out);

bool usb_audio_stream_pop_sample_or_hold(UsbAudioStream *stream, int16_t *sample_out);

bool usb_audio_stream_push_sample(UsbAudioStream *stream, int16_t sample);

/* Samples are packed as little-endian int16_t. */
size_t usb_audio_stream_pop_samples(UsbAudioStream *stream,
                                    int16_t *samples_out,
                                    size_t max_samples);

/* `byte_count` must be even; odd trailing bytes are ignored. */
void usb_audio_stream_push_bytes(UsbAudioStream *stream,
                                 const uint8_t *bytes,
                                 size_t byte_count);

size_t usb_audio_stream_out_available(const UsbAudioStream *stream);

size_t usb_audio_stream_in_available(const UsbAudioStream *stream);

uint32_t usb_audio_stream_out_dropped(const UsbAudioStream *stream);
uint32_t usb_audio_stream_in_dropped(const UsbAudioStream *stream);

/* Diagnostics. Counters wrap on overflow. */
void usb_audio_stream_note_as_out_packet(UsbAudioStream *stream);
void usb_audio_stream_note_effects_frame(UsbAudioStream *stream);
void usb_audio_stream_reset_diag(UsbAudioStream *stream);

uint32_t usb_audio_stream_as_out_packets(const UsbAudioStream *stream);
uint32_t usb_audio_stream_in_ring_fill_peak(const UsbAudioStream *stream);
uint32_t usb_audio_stream_effects_zoh_holds(const UsbAudioStream *stream);
uint32_t usb_audio_stream_effects_frames(const UsbAudioStream *stream);

#endif
