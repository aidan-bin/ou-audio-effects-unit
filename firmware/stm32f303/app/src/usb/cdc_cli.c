#include "usb/cdc_cli.h"
#include "usbd_ctlreq.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* The global device handle (declared in usb_device.c) */
// NOLINTBEGIN(readability-identifier-naming)
extern USBD_HandleTypeDef hUsbDeviceFS;

/* Singleton (exactly one CLI instance), so state is kept as static
 * storage here rather than threaded through the composite driver's
 * pClassData -- only the UAC sub-driver's state needs to live there
 * (see composite.c). */
typedef struct
{
    bool initialized;
    uint32_t cli_data[CDC_DUAL_DATA_FS_PACKET_SIZE / 4U];
    uint8_t cli_cmd_op_code;
    uint8_t cli_cmd_length;
    uint8_t *cli_rx_buffer;
    uint8_t *cli_tx_buffer;
    uint32_t cli_rx_length;
    uint32_t cli_tx_length;
    __IO uint32_t cli_tx_state;
    __IO uint32_t cli_rx_state;
    USBD_CDC_ItfTypeDef *cli_fops;
} UsbdCdcCliHandleTypeDef;

static UsbdCdcCliHandleTypeDef usbd_cdc_cli_handle;

/* Open/close order matters and must match the descriptors. */
static const struct
{
    uint8_t address;
    uint8_t type;
    uint16_t packet_size;
    bool is_in;
} cdc_cli_endpoints[] = {
    {CDC0_IN_EP, USBD_EP_TYPE_BULK, CDC_DUAL_DATA_FS_PACKET_SIZE, true},
    {CDC0_OUT_EP, USBD_EP_TYPE_BULK, CDC_DUAL_DATA_FS_PACKET_SIZE, false},
    {CDC0_CMD_EP, USBD_EP_TYPE_INTR, CDC_DUAL_CMD_PACKET_SIZE, true},
};

static void cdc_cli_set_ep_used(USBD_HandleTypeDef *pdev, size_t index, uint8_t used)
{
    uint8_t addr = cdc_cli_endpoints[index].address & 0x7FU;
    if (cdc_cli_endpoints[index].is_in)
    {
        pdev->ep_in[addr].is_used = used;
    }
    else
    {
        pdev->ep_out[addr].is_used = used;
    }
}

uint8_t usbd_cdc_cli_class_init(USBD_HandleTypeDef *pdev)
{
    UsbdCdcCliHandleTypeDef *h = &usbd_cdc_cli_handle;
    USBD_CDC_ItfTypeDef *cli_fops = h->cli_fops;
    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    memset(h, 0, sizeof(*h));
    h->cli_fops = cli_fops;
    h->initialized = true;

    for (size_t i = 0; i < sizeof(cdc_cli_endpoints) / sizeof(cdc_cli_endpoints[0]); i++)
    {
        USBD_LL_OpenEP(pdev, cdc_cli_endpoints[i].address, cdc_cli_endpoints[i].type,
                       cdc_cli_endpoints[i].packet_size);
        cdc_cli_set_ep_used(pdev, i, 1U);
    }

    h->cli_tx_state = 0U;
    h->cli_rx_state = 0U;

    if (h->cli_fops != NULL && h->cli_fops->Init != NULL)
    {
        h->cli_fops->Init();
    }

    USBD_LL_PrepareReceive(pdev, CDC0_OUT_EP, h->cli_rx_buffer,
                           CDC_DUAL_DATA_FS_PACKET_SIZE);

    return 0U;
}

uint8_t usbd_cdc_cli_class_deinit(USBD_HandleTypeDef *pdev)
{
    UsbdCdcCliHandleTypeDef *h = &usbd_cdc_cli_handle;

    for (size_t i = 0; i < sizeof(cdc_cli_endpoints) / sizeof(cdc_cli_endpoints[0]); i++)
    {
        USBD_LL_CloseEP(pdev, cdc_cli_endpoints[i].address);
        cdc_cli_set_ep_used(pdev, i, 0U);
    }

    if (h->cli_fops != NULL && h->cli_fops->DeInit != NULL)
    {
        h->cli_fops->DeInit();
    }

    h->initialized = false;
    return 0U;
}

uint8_t usbd_cdc_cli_setup(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req)
{
    UsbdCdcCliHandleTypeDef *h = &usbd_cdc_cli_handle;
    uint16_t status_info = 0U;
    uint8_t ifalt = 0U;
    uint8_t ret = USBD_OK;

    if (!h->initialized)
    {
        USBD_CtlError(pdev, req);
        return USBD_FAIL;
    }

    switch (req->bmRequest & USB_REQ_TYPE_MASK)
    {
    case USB_REQ_TYPE_CLASS:
        if (req->wLength)
        {
            if (req->bmRequest & 0x80U)
            {
                if (h->cli_fops != NULL && h->cli_fops->Control != NULL)
                {
                    h->cli_fops->Control(req->bRequest,
                                         (uint8_t *)(void *)h->cli_data, // NOLINT(bugprone-casting-through-void)
                                         req->wLength);
                }
                // NOLINTNEXTLINE(bugprone-casting-through-void)
                USBD_CtlSendData(pdev, (uint8_t *)(void *)h->cli_data,
                                 req->wLength);
            }
            else
            {
                h->cli_cmd_op_code = req->bRequest;
                h->cli_cmd_length = (uint8_t)req->wLength;
                USBD_CtlPrepareRx(pdev,
                                  (uint8_t *)(void *)h->cli_data, // NOLINT(bugprone-casting-through-void)
                                  req->wLength);
            }
        }
        else
        {
            if (h->cli_fops != NULL && h->cli_fops->Control != NULL)
            {
                // NOLINTNEXTLINE(bugprone-casting-through-void)
                h->cli_fops->Control(req->bRequest, (uint8_t *)(void *)req, 0U);
            }
        }
        break;

    case USB_REQ_TYPE_STANDARD:
        switch (req->bRequest)
        {
        case USB_REQ_GET_STATUS:
            if (pdev->dev_state == USBD_STATE_CONFIGURED)
            {
                // NOLINTNEXTLINE(bugprone-casting-through-void)
                USBD_CtlSendData(pdev, (uint8_t *)(void *)&status_info, 2U);
            }
            else
            {
                USBD_CtlError(pdev, req);
                ret = USBD_FAIL;
            }
            break;

        case USB_REQ_GET_INTERFACE:
            if (pdev->dev_state == USBD_STATE_CONFIGURED)
            {
                USBD_CtlSendData(pdev, &ifalt, 1U);
            }
            else
            {
                USBD_CtlError(pdev, req);
                ret = USBD_FAIL;
            }
            break;

        case USB_REQ_SET_INTERFACE:
            if (pdev->dev_state != USBD_STATE_CONFIGURED)
            {
                USBD_CtlError(pdev, req);
                ret = USBD_FAIL;
            }
            break;

        default:
            USBD_CtlError(pdev, req);
            ret = USBD_FAIL;
            break;
        }
        break;

    default:
        USBD_CtlError(pdev, req);
        ret = USBD_FAIL;
        break;
    }

    return ret;
}

uint8_t usbd_cdc_cli_ep0_rxready(USBD_HandleTypeDef *pdev)
{
    (void)pdev;
    UsbdCdcCliHandleTypeDef *h = &usbd_cdc_cli_handle;
    if (!h->initialized)
    {
        return USBD_FAIL;
    }

    if (h->cli_cmd_op_code != 0xFFU && h->cli_fops != NULL && h->cli_fops->Control != NULL)
    {
        // NOLINTNEXTLINE(bugprone-casting-through-void)
        h->cli_fops->Control(h->cli_cmd_op_code, (uint8_t *)(void *)h->cli_data,
                             (uint16_t)h->cli_cmd_length);
        h->cli_cmd_op_code = 0xFFU;
    }

    return USBD_OK;
}

uint8_t usbd_cdc_cli_data_in(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
    UsbdCdcCliHandleTypeDef *h = &usbd_cdc_cli_handle;
    if (!h->initialized)
    {
        return USBD_FAIL;
    }

    PCD_HandleTypeDef *hpcd = pdev->pData;

    if ((pdev->ep_in[epnum & 0x7FU].total_length > 0U) &&
        ((pdev->ep_in[epnum & 0x7FU].total_length %
          hpcd->IN_ep[epnum & 0x7FU].maxpacket) == 0U))
    {
        pdev->ep_in[epnum & 0x7FU].total_length = 0U;
        USBD_LL_Transmit(pdev, epnum, NULL, 0U);
    }
    else
    {
        h->cli_tx_state = 0U;
    }

    return USBD_OK;
}

uint8_t usbd_cdc_cli_data_out(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
    (void)epnum;
    UsbdCdcCliHandleTypeDef *h = &usbd_cdc_cli_handle;
    if (!h->initialized)
    {
        return USBD_FAIL;
    }

    h->cli_rx_length = USBD_LL_GetRxDataSize(pdev, CDC0_OUT_EP);
    if (h->cli_fops != NULL && h->cli_fops->Receive != NULL)
    {
        h->cli_fops->Receive(h->cli_rx_buffer, &h->cli_rx_length);
    }

    return USBD_OK;
}

bool usbd_cdc_cli_owns_ep(uint8_t epnum)
{
    uint8_t addr = epnum & 0x7FU;
    return addr == (CDC0_IN_EP & 0x7FU) || addr == (CDC0_OUT_EP & 0x7FU) ||
           addr == (CDC0_CMD_EP & 0x7FU);
}

bool usbd_cdc_cli_owns_iface(uint8_t iface)
{
    return iface <= 1U;
}

uint8_t USBD_CDC_CLI_RegisterInterface(USBD_HandleTypeDef *pdev,
                                       USBD_CDC_ItfTypeDef *fops_cli)
{
    (void)pdev;
    if (fops_cli != NULL)
    {
        usbd_cdc_cli_handle.cli_fops = fops_cli;
    }
    return USBD_OK;
}

uint8_t USBD_CDC_CLI_Transmit_FS(uint8_t *buf, uint16_t len)
{
    UsbdCdcCliHandleTypeDef *h = &usbd_cdc_cli_handle;

    /* Claim the pipe atomically */
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    if (h->cli_tx_state != 0U)
    {
        __set_PRIMASK(primask);
        return USBD_BUSY;
    }
    h->cli_tx_state = 1U;
    __set_PRIMASK(primask);

    h->cli_tx_buffer = buf;
    h->cli_tx_length = len;
    hUsbDeviceFS.ep_in[CDC0_IN_EP & 0x7FU].total_length = len;
    return USBD_LL_Transmit(&hUsbDeviceFS, CDC0_IN_EP, buf, len);
}

void USBD_CDC_CLI_SetRxBuffer(uint8_t *pbuff)
{
    usbd_cdc_cli_handle.cli_rx_buffer = pbuff;
}

void USBD_CDC_CLI_SetTxBuffer(uint8_t *pbuff, uint16_t len)
{
    usbd_cdc_cli_handle.cli_tx_buffer = pbuff;
    usbd_cdc_cli_handle.cli_tx_length = len;
}

void USBD_CDC_CLI_ReceivePacket(void)
{
    USBD_LL_PrepareReceive(&hUsbDeviceFS, CDC0_OUT_EP, usbd_cdc_cli_handle.cli_rx_buffer,
                           CDC_DUAL_DATA_FS_PACKET_SIZE);
}
// NOLINTEND(readability-identifier-naming)
