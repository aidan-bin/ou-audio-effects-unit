/*
 * usbd_cdc_cli — CDC0 (CLI) sub-driver, not a standalone USBD_ClassTypeDef.
 *
 * The ST core only supports one registered class per device, so
 * composite.* dispatches Setup/DataIn/DataOut/EP0_RxReady here by
 * interface/endpoint.
 */

#ifndef USB_CDC_CLI_H
#define USB_CDC_CLI_H

#include "usbd_cdc.h"

#define CDC0_IN_EP 0x81U
#define CDC0_OUT_EP 0x01U
#define CDC0_CMD_EP 0x82U

#define CDC_DUAL_DATA_FS_PACKET_SIZE 64U
#define CDC_DUAL_CMD_PACKET_SIZE 8U

/* Public API: register the CLI text-command callbacks and drive TX/RX. */
uint8_t USBD_CDC_CLI_RegisterInterface(USBD_HandleTypeDef *pdev,
                                       USBD_CDC_ItfTypeDef *fops_cli);
uint8_t USBD_CDC_CLI_Transmit_FS(uint8_t *buf, uint16_t len);
void USBD_CDC_CLI_SetRxBuffer(uint8_t *pbuff);
void USBD_CDC_CLI_SetTxBuffer(uint8_t *pbuff, uint16_t len);
void USBD_CDC_CLI_ReceivePacket(void);

/* Composite dispatcher entry points (composite.c only) */
uint8_t usbd_cdc_cli_class_init(USBD_HandleTypeDef *pdev);uint8_t usbd_cdc_cli_class_deinit(USBD_HandleTypeDef *pdev);
uint8_t usbd_cdc_cli_setup(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req);
uint8_t usbd_cdc_cli_ep0_rxready(USBD_HandleTypeDef *pdev);
uint8_t usbd_cdc_cli_data_in(USBD_HandleTypeDef *pdev, uint8_t epnum);
uint8_t usbd_cdc_cli_data_out(USBD_HandleTypeDef *pdev, uint8_t epnum);

bool usbd_cdc_cli_owns_ep(uint8_t epnum);
bool usbd_cdc_cli_owns_iface(uint8_t iface);

#endif /* USB_CDC_CLI_H */
