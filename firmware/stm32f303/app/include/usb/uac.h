/*
 * usbd_uac — USB Audio Class 1.0 (UAC1) sub-driver: AC + AS-OUT + AS-IN.
 */

#ifndef USBD_UAC_H
#define USBD_UAC_H

#include "usbd_def.h"

#include <stdbool.h>
#include <stdint.h>

struct UsbAudioStream;

/* Interface numbers (CDC0 owns 0 and 1; see cdc_cli.h) */
#define UAC_IFACE_AC 2U
#define UAC_IFACE_AS_OUT 3U
#define UAC_IFACE_AS_IN 4U

#define UAC_AS_IN_EP 0x83U
/* AS-OUT must live on a different EP number from AS-IN. On STM32F303 the
 * double-buffered OUT mode reuses the ADDR_TX BTABLE slot as buf0, which is
 * also used by any IN endpoint on the same EP number. Putting AS-OUT on EP5
 * keeps EP3's ADDR_TX free for AS-IN. */
#define UAC_AS_OUT_EP 0x05U
#define UAC_FEEDBACK_EP 0x84U

/* Must match compute_sample_rate_hz() (main.c): 48000000 / 1185 = 40506. */
#define UAC_SAMPLE_RATE_HZ 40506U

/* One SOF (1 ms) worth of samples plus margin for feedback-driven rate
 * correction. */
#define UAC_SAMPLES_PER_MS ((UAC_SAMPLE_RATE_HZ / 1000U) + 2U)
#define UAC_AS_MAX_PACKET_BYTES (UAC_SAMPLES_PER_MS * 2U)
#define UAC_FEEDBACK_PACKET_BYTES 3U

/* Call once at init, before composite init runs. */
void usbd_uac_register_stream(struct UsbAudioStream *stream);

/* Composite dispatcher entry points (composite.c only) */
uint8_t usbd_uac_class_init(USBD_HandleTypeDef *pdev);
uint8_t usbd_uac_class_deinit(USBD_HandleTypeDef *pdev);
uint8_t usbd_uac_setup(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req);
uint8_t usbd_uac_data_in(USBD_HandleTypeDef *pdev, uint8_t epnum);
uint8_t usbd_uac_data_out(USBD_HandleTypeDef *pdev, uint8_t epnum);
uint8_t usbd_uac_sof(USBD_HandleTypeDef *pdev);

bool usbd_uac_owns_ep(uint8_t epnum);
bool usbd_uac_owns_iface(uint8_t iface);

/* USB-side diagnostics live in usb/uac_diag.h (safe for host-test builds). */
#include "usb/uac_diag.h"

#endif /* USBD_UAC_H */
