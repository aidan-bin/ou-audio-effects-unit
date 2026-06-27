#ifndef USBD_CDC_DUAL_H
#define USBD_CDC_DUAL_H

#include "usbd_cdc.h"

/* CDC0 (CLI) endpoint addresses */
#define CDC0_IN_EP 0x81U
#define CDC0_OUT_EP 0x01U
#define CDC0_CMD_EP 0x82U

/* CDC1 (Audio) endpoint addresses */
#define CDC1_IN_EP 0x83U
#define CDC1_OUT_EP 0x03U
#define CDC1_CMD_EP 0x84U

/* Packet sizes (Full Speed) */
#define CDC_DUAL_DATA_FS_PACKET_SIZE 64U
#define CDC_DUAL_CMD_PACKET_SIZE 8U

/* Combined configuration descriptor size */
#define USB_CDC_DUAL_CONFIG_DESC_SIZ 125U

/*
 * Dual CDC handle: contains state for two logical CDC ACM instances.
 * pClassData on the USBD handle points to one of these.
 */
typedef struct
{
    /* ---- CDC0 (CLI) state ---- */
    uint32_t cli_data[CDC_DUAL_DATA_FS_PACKET_SIZE / 4U];
    uint8_t cli_cmd_op_code;
    uint8_t cli_cmd_length;
    uint8_t *cli_rx_buffer;
    uint8_t *cli_tx_buffer;
    uint32_t cli_rx_length;
    uint32_t cli_tx_length;
    __IO uint32_t cli_tx_state;
    __IO uint32_t cli_rx_state;

    /* ---- CDC1 (Audio) state ---- */
    uint32_t audio_data[CDC_DUAL_DATA_FS_PACKET_SIZE / 4U];
    uint8_t audio_cmd_op_code;
    uint8_t audio_cmd_length;
    uint8_t *audio_rx_buffer;
    uint8_t *audio_tx_buffer;
    uint32_t audio_rx_length;
    uint32_t audio_tx_length;
    __IO uint32_t audio_tx_state;
    __IO uint32_t audio_rx_state;

    /* User callbacks (one USBD_CDC_ItfTypeDef per instance) */
    USBD_CDC_ItfTypeDef *cli_fops;
    USBD_CDC_ItfTypeDef *audio_fops;
} USBD_CDC_Dual_HandleTypeDef;

/* The class driver exported for use with USBD_RegisterClass */
extern USBD_ClassTypeDef USBD_CDC_Dual;

/*
 * Register user callbacks for the two CDC instances.
 * fops_cli receives text commands; fops_audio receives raw audio samples.
 */
uint8_t USBD_CDC_Dual_RegisterInterface(USBD_HandleTypeDef *pdev,
                                        USBD_CDC_ItfTypeDef *fops_cli,
                                        USBD_CDC_ItfTypeDef *fops_audio);

/*
 * Transmit functions.  These mirror CDC_Transmit_FS but target
 * their respective endpoints and track per-pipe TxState.
 *
 * hUsbDeviceFS must be declared in the calling translation unit.
 */
uint8_t USBD_CDC_Dual_Transmit_FS_CLI(uint8_t *buf, uint16_t len);
uint8_t USBD_CDC_Dual_Transmit_FS_Audio(uint8_t *buf, uint16_t len);

/*
 * Buffer configuration helpers.  Called from the Init callbacks
 * (USBD_CDC_ItfTypeDef.Init) to set per-instance RX/TX buffers.
 * These access the dual handle via the global hUsbDeviceFS.
 */
void USBD_CDC_Dual_SetRxBuffer_CLI(uint8_t *pbuff);
void USBD_CDC_Dual_SetTxBuffer_CLI(uint8_t *pbuff, uint16_t len);
void USBD_CDC_Dual_SetRxBuffer_Audio(uint8_t *pbuff);
void USBD_CDC_Dual_SetTxBuffer_Audio(uint8_t *pbuff, uint16_t len);

/*
 * Re-arm OUT endpoint for reception after processing received data.
 */
void USBD_CDC_Dual_ReceivePacket_CLI(void);
void USBD_CDC_Dual_ReceivePacket_Audio(void);

#endif /* USBD_CDC_DUAL_H */
