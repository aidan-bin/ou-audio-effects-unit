#include "usbd_cdc_dual.h"
#include "usbd_ctlreq.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* The global device handle (declared in usb_device.c) */
// NOLINTBEGIN(readability-identifier-naming)
extern USBD_HandleTypeDef hUsbDeviceFS;

/* ------------------------------------------------------------------ */
/* Combined Full-Speed Configuration Descriptor                       */
/*                                                                  */
/*  CDC0 (CLI):   Interface 0 (Comm) + Interface 1 (Data)            */
/*                EP2 IN (Interrupt), EP1 OUT/IN (Bulk)              */
/*  CDC1 (Audio): Interface 2 (Comm) + Interface 3 (Data)            */
/*                EP4 IN (Interrupt), EP3 OUT/IN (Bulk)              */
/* ------------------------------------------------------------------ */
__ALIGN_BEGIN static uint8_t USBD_CDC_Dual_CfgFSDesc[USB_CDC_DUAL_CONFIG_DESC_SIZ] __ALIGN_END =
    {
        /* ---- Configuration Descriptor (9 bytes) ---- */
        0x09,
        USB_DESC_TYPE_CONFIGURATION,
        USB_CDC_DUAL_CONFIG_DESC_SIZ,
        0x00, /* wTotalLength hi */
        0x04, /* bNumInterfaces = 4 */
        0x01, /* bConfigurationValue */
        0x00, /* iConfiguration */
        0xC0, /* bmAttributes: self-powered */
        0x32, /* bMaxPower: 100 mA */

        /* ================================================================ */
        /* CDC0 (CLI) -- Interface Association Descriptor (groups if0+if1)   */
        /* ================================================================ */
        0x08, /* bLength */
        0x0B, /* bDescriptorType: Interface Association */
        0x00, /* bFirstInterface = 0 */
        0x02, /* bInterfaceCount = 2 */
        0x02, /* bFunctionClass: CDC Communication */
        0x02, /* bFunctionSubClass: Abstract Control Model */
        0x01, /* bFunctionProtocol: AT commands */
        0x00, /* iFunction */

        /* ================================================================ */
        /* CDC0 (CLI) -- Interface 0 - Communication Class                  */
        /* ================================================================ */
        0x09,
        USB_DESC_TYPE_INTERFACE,
        0x00, /* bInterfaceNumber = 0 */
        0x00, /* bAlternateSetting */
        0x01, /* bNumEndpoints: 1 (Interrupt IN) */
        0x02, /* bInterfaceClass: CDC Communication */
        0x02, /* bInterfaceSubClass: Abstract Control Model */
        0x01, /* bInterfaceProtocol: AT commands */
        0x00, /* iInterface */

        /* Header Functional Descriptor */
        0x05, 0x24, 0x00, 0x10, 0x01,

        /* Call Management Functional Descriptor */
        0x05, 0x24, 0x01, 0x00, 0x01, /* bDataInterface = 1 */

        /* ACM Functional Descriptor */
        0x04, 0x24, 0x02, 0x02,

        /* Union Functional Descriptor */
        0x05, 0x24, 0x06, 0x00, 0x01, /* master=if0, slave=if1 */

        /* Endpoint 2 IN Descriptor (Interrupt) */
        0x07,
        USB_DESC_TYPE_ENDPOINT,
        CDC0_CMD_EP, /* 0x82 */
        0x03,        /* bmAttributes: Interrupt */
        LOBYTE(CDC_DUAL_CMD_PACKET_SIZE),
        HIBYTE(CDC_DUAL_CMD_PACKET_SIZE),
        0x10, /* bInterval */

        /* ================================================================ */
        /* CDC0 (CLI) -- Interface 1 - Data Class Interface                 */
        /* ================================================================ */
        0x09,
        USB_DESC_TYPE_INTERFACE,
        0x01, /* bInterfaceNumber = 1 */
        0x00, /* bAlternateSetting */
        0x02, /* bNumEndpoints: 2 (Bulk OUT + IN) */
        0x0A, /* bInterfaceClass: CDC Data */
        0x00, /* bInterfaceSubClass */
        0x00, /* bInterfaceProtocol */
        0x00, /* iInterface */

        /* Endpoint 1 OUT Descriptor (Bulk) */
        0x07,
        USB_DESC_TYPE_ENDPOINT,
        CDC0_OUT_EP, /* 0x01 */
        0x02,        /* bmAttributes: Bulk */
        LOBYTE(CDC_DUAL_DATA_FS_PACKET_SIZE),
        HIBYTE(CDC_DUAL_DATA_FS_PACKET_SIZE),
        0x00, /* bInterval */

        /* Endpoint 1 IN Descriptor (Bulk) */
        0x07,
        USB_DESC_TYPE_ENDPOINT,
        CDC0_IN_EP, /* 0x81 */
        0x02,       /* bmAttributes: Bulk */
        LOBYTE(CDC_DUAL_DATA_FS_PACKET_SIZE),
        HIBYTE(CDC_DUAL_DATA_FS_PACKET_SIZE),
        0x00, /* bInterval */

        /* ================================================================ */
        /* CDC1 (Audio) -- Interface Association Descriptor (groups if2+if3) */
        /* ================================================================ */
        0x08, /* bLength */
        0x0B, /* bDescriptorType: Interface Association */
        0x02, /* bFirstInterface = 2 */
        0x02, /* bInterfaceCount = 2 */
        0x02, /* bFunctionClass: CDC Communication */
        0x02, /* bFunctionSubClass: Abstract Control Model */
        0x01, /* bFunctionProtocol: AT commands */
        0x00, /* iFunction */

        /* ================================================================ */
        /* CDC1 (Audio) -- Interface 2 - Communication Class                */
        /* ================================================================ */
        0x09,
        USB_DESC_TYPE_INTERFACE,
        0x02, /* bInterfaceNumber = 2 */
        0x00, /* bAlternateSetting */
        0x01, /* bNumEndpoints: 1 (Interrupt IN) */
        0x02, /* bInterfaceClass: CDC Communication */
        0x02, /* bInterfaceSubClass: Abstract Control Model */
        0x01, /* bInterfaceProtocol: AT commands */
        0x00, /* iInterface */

        /* Header Functional Descriptor */
        0x05, 0x24, 0x00, 0x10, 0x01,

        /* Call Management Functional Descriptor */
        0x05, 0x24, 0x01, 0x00, 0x03, /* bDataInterface = 3 */

        /* ACM Functional Descriptor */
        0x04, 0x24, 0x02, 0x02,

        /* Union Functional Descriptor */
        0x05, 0x24, 0x06, 0x02, 0x03, /* master=if2, slave=if3 */

        /* Endpoint 4 IN Descriptor (Interrupt) */
        0x07,
        USB_DESC_TYPE_ENDPOINT,
        CDC1_CMD_EP, /* 0x84 */
        0x03,        /* bmAttributes: Interrupt */
        LOBYTE(CDC_DUAL_CMD_PACKET_SIZE),
        HIBYTE(CDC_DUAL_CMD_PACKET_SIZE),
        0x10, /* bInterval */

        /* ================================================================ */
        /* CDC1 (Audio) -- Interface 3 - Data Class Interface               */
        /* ================================================================ */
        0x09,
        USB_DESC_TYPE_INTERFACE,
        0x03, /* bInterfaceNumber = 3 */
        0x00, /* bAlternateSetting */
        0x02, /* bNumEndpoints: 2 (Bulk OUT + IN) */
        0x0A, /* bInterfaceClass: CDC Data */
        0x00, /* bInterfaceSubClass */
        0x00, /* bInterfaceProtocol */
        0x00, /* iInterface */

        /* Endpoint 3 OUT Descriptor (Bulk) */
        0x07,
        USB_DESC_TYPE_ENDPOINT,
        CDC1_OUT_EP, /* 0x03 */
        0x02,        /* bmAttributes: Bulk */
        LOBYTE(CDC_DUAL_DATA_FS_PACKET_SIZE),
        HIBYTE(CDC_DUAL_DATA_FS_PACKET_SIZE),
        0x00, /* bInterval */

        /* Endpoint 3 IN Descriptor (Bulk) */
        0x07,
        USB_DESC_TYPE_ENDPOINT,
        CDC1_IN_EP, /* 0x83 */
        0x02,       /* bmAttributes: Bulk */
        LOBYTE(CDC_DUAL_DATA_FS_PACKET_SIZE),
        HIBYTE(CDC_DUAL_DATA_FS_PACKET_SIZE),
        0x00, /* bInterval */
};

/* Other-speed descriptor (HS-capable device reporting FS config). Generated on
 * demand from the FS descriptor. */
__ALIGN_BEGIN static uint8_t
    USBD_CDC_Dual_OtherSpeedCfgDesc[USB_CDC_DUAL_CONFIG_DESC_SIZ] __ALIGN_END;

/* Device Qualifier descriptor */
__ALIGN_BEGIN static uint8_t USBD_CDC_Dual_DeviceQualifierDesc
    [USB_LEN_DEV_QUALIFIER_DESC] __ALIGN_END =
        {
            USB_LEN_DEV_QUALIFIER_DESC,
            USB_DESC_TYPE_DEVICE_QUALIFIER,
            0x00,
            0x02,
            0x00,
            0x00,
            0x00,
            0x40,
            0x01,
            0x00,
};

/* ------------------------------------------------------------------ */
/* Static class-data block (replaces USBD_malloc)                     */
/* ------------------------------------------------------------------ */
static USBD_CDC_Dual_HandleTypeDef usbd_cdc_dual_handle;

/* ------------------------------------------------------------------ */
/* Forward declarations                                               */
/* ------------------------------------------------------------------ */
static uint8_t USBD_CDC_Dual_Init(USBD_HandleTypeDef *pdev,
                                  uint8_t cfgidx);
static uint8_t USBD_CDC_Dual_DeInit(USBD_HandleTypeDef *pdev,
                                    uint8_t cfgidx);
static uint8_t USBD_CDC_Dual_Setup(USBD_HandleTypeDef *pdev,
                                   USBD_SetupReqTypedef *req);
static uint8_t USBD_CDC_Dual_EP0_RxReady(USBD_HandleTypeDef *pdev);
static uint8_t USBD_CDC_Dual_DataIn(USBD_HandleTypeDef *pdev,
                                    uint8_t epnum);
static uint8_t USBD_CDC_Dual_DataOut(USBD_HandleTypeDef *pdev,
                                     uint8_t epnum);
static uint8_t *USBD_CDC_Dual_GetFSCfgDesc(uint16_t *length);
static uint8_t *USBD_CDC_Dual_GetHSCfgDesc(uint16_t *length);
static uint8_t *USBD_CDC_Dual_GetOtherSpeedCfgDesc(uint16_t *length);
static uint8_t *USBD_CDC_Dual_GetDeviceQualifierDesc(uint16_t *length);
// NOLINTEND(readability-identifier-naming)

/* ------------------------------------------------------------------ */
/* Class callback struct                                              */
/* ------------------------------------------------------------------ */
USBD_ClassTypeDef USBD_CDC_Dual =
    {
        USBD_CDC_Dual_Init,
        USBD_CDC_Dual_DeInit,
        USBD_CDC_Dual_Setup,
        NULL, /* EP0_TxSent */
        USBD_CDC_Dual_EP0_RxReady,
        USBD_CDC_Dual_DataIn,
        USBD_CDC_Dual_DataOut,
        NULL, /* SOF */
        NULL, /* IsoINIncomplete */
        NULL, /* IsoOUTIncomplete */
        USBD_CDC_Dual_GetHSCfgDesc,
        USBD_CDC_Dual_GetFSCfgDesc,
        USBD_CDC_Dual_GetOtherSpeedCfgDesc,
        USBD_CDC_Dual_GetDeviceQualifierDesc,
};

/* ------------------------------------------------------------------ */
/* Helper: get dual handle from device handle                         */
/* ------------------------------------------------------------------ */
static USBD_CDC_Dual_HandleTypeDef *get_dual(USBD_HandleTypeDef *pdev)
{
    return (USBD_CDC_Dual_HandleTypeDef *)pdev->pClassData;
}

/* ------------------------------------------------------------------ */
/* Helper: which CDC instance owns this endpoint?                      */
/*   Returns 0 for CDC0 (CLI), 1 for CDC1 (Audio), -1 for unknown.    */
/* ------------------------------------------------------------------ */
static int ep_to_instance(uint8_t epnum)
{
    uint8_t addr = epnum & 0x7FU;
    if (addr == (CDC0_IN_EP & 0x7FU) || addr == (CDC0_OUT_EP & 0x7FU) ||
        addr == (CDC0_CMD_EP & 0x7FU))
    {
        return 0;
    }
    if (addr == (CDC1_IN_EP & 0x7FU) || addr == (CDC1_OUT_EP & 0x7FU) ||
        addr == (CDC1_CMD_EP & 0x7FU))
    {
        return 1;
    }
    return -1;
}

/* ------------------------------------------------------------------ */
/* Helper: which CDC instance handles this interface?                  */
/* ------------------------------------------------------------------ */
static int iface_to_instance(uint8_t iface)
{
    if (iface <= 1U)
    {
        return 0;
    }
    if (iface == 2U || iface == 3U)
    {
        return 1;
    }
    return -1;
}

/* ------------------------------------------------------------------ */
/* Endpoint table (open/close order matters and must match descriptors) */
/* ------------------------------------------------------------------ */
static const struct
{
    uint8_t address;
    uint8_t type;
    uint16_t packet_size;
    bool is_in;
} cdc_dual_endpoints[] = {
    {CDC0_IN_EP, USBD_EP_TYPE_BULK, CDC_DUAL_DATA_FS_PACKET_SIZE, true},
    {CDC0_OUT_EP, USBD_EP_TYPE_BULK, CDC_DUAL_DATA_FS_PACKET_SIZE, false},
    {CDC0_CMD_EP, USBD_EP_TYPE_INTR, CDC_DUAL_CMD_PACKET_SIZE, true},
    {CDC1_IN_EP, USBD_EP_TYPE_BULK, CDC_DUAL_DATA_FS_PACKET_SIZE, true},
    {CDC1_OUT_EP, USBD_EP_TYPE_BULK, CDC_DUAL_DATA_FS_PACKET_SIZE, false},
    {CDC1_CMD_EP, USBD_EP_TYPE_INTR, CDC_DUAL_CMD_PACKET_SIZE, true},
};

static void cdc_dual_set_ep_used(USBD_HandleTypeDef *pdev, size_t index, uint8_t used)
{
    uint8_t addr = cdc_dual_endpoints[index].address & 0x7FU;
    if (cdc_dual_endpoints[index].is_in)
    {
        pdev->ep_in[addr].is_used = used;
    }
    else
    {
        pdev->ep_out[addr].is_used = used;
    }
}

/* ------------------------------------------------------------------ */
/* USBD_CDC_Dual_Init                                                 */
/* ------------------------------------------------------------------ */
static uint8_t USBD_CDC_Dual_Init(USBD_HandleTypeDef *pdev,
                                  uint8_t cfgidx)
{
    (void)cfgidx;

    USBD_CDC_Dual_HandleTypeDef *hdual = &usbd_cdc_dual_handle;
    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    memset(hdual, 0, sizeof(*hdual));
    pdev->pClassData = hdual;

    /* Open all CDC0/CDC1 endpoints */
    for (size_t i = 0; i < sizeof(cdc_dual_endpoints) / sizeof(cdc_dual_endpoints[0]); i++)
    {
        USBD_LL_OpenEP(pdev, cdc_dual_endpoints[i].address, cdc_dual_endpoints[i].type,
                       cdc_dual_endpoints[i].packet_size);
        cdc_dual_set_ep_used(pdev, i, 1U);
    }

    /* Init Tx/Rx states */
    hdual->cli_tx_state = 0U;
    hdual->cli_rx_state = 0U;
    hdual->audio_tx_state = 0U;
    hdual->audio_rx_state = 0U;

    /* Call user Init callbacks */
    if (hdual->cli_fops != NULL && hdual->cli_fops->Init != NULL)
    {
        hdual->cli_fops->Init();
    }
    if (hdual->audio_fops != NULL && hdual->audio_fops->Init != NULL)
    {
        hdual->audio_fops->Init();
    }

    /* Prepare OUT endpoints for reception */
    USBD_LL_PrepareReceive(pdev, CDC0_OUT_EP, hdual->cli_rx_buffer,
                           CDC_DUAL_DATA_FS_PACKET_SIZE);
    USBD_LL_PrepareReceive(pdev, CDC1_OUT_EP, hdual->audio_rx_buffer,
                           CDC_DUAL_DATA_FS_PACKET_SIZE);

    return 0U;
}

/* ------------------------------------------------------------------ */
/* USBD_CDC_Dual_DeInit                                               */
/* ------------------------------------------------------------------ */
static uint8_t USBD_CDC_Dual_DeInit(USBD_HandleTypeDef *pdev,
                                    uint8_t cfgidx)
{
    (void)cfgidx;

    /* Close all CDC0/CDC1 endpoints */
    for (size_t i = 0; i < sizeof(cdc_dual_endpoints) / sizeof(cdc_dual_endpoints[0]); i++)
    {
        USBD_LL_CloseEP(pdev, cdc_dual_endpoints[i].address);
        cdc_dual_set_ep_used(pdev, i, 0U);
    }

    USBD_CDC_Dual_HandleTypeDef *hdual = get_dual(pdev);
    if (hdual != NULL)
    {
        if (hdual->cli_fops != NULL && hdual->cli_fops->DeInit != NULL)
        {
            hdual->cli_fops->DeInit();
        }
        if (hdual->audio_fops != NULL && hdual->audio_fops->DeInit != NULL)
        {
            hdual->audio_fops->DeInit();
        }
    }

    pdev->pClassData = NULL;
    return 0U;
}

/* ------------------------------------------------------------------ */
/* USBD_CDC_Dual_Setup  -- dispatch by interface number                */
/* ------------------------------------------------------------------ */
static uint8_t USBD_CDC_Dual_Setup(USBD_HandleTypeDef *pdev,
                                   USBD_SetupReqTypedef *req)
{
    USBD_CDC_Dual_HandleTypeDef *hdual = get_dual(pdev);
    uint8_t iface = LOBYTE(req->wIndex);
    int inst = iface_to_instance(iface);
    uint16_t status_info = 0U;
    uint8_t ifalt = 0U;
    uint8_t ret = USBD_OK;

    switch (req->bmRequest & USB_REQ_TYPE_MASK)
    {
    case USB_REQ_TYPE_CLASS:
        if (inst < 0)
        {
            USBD_CtlError(pdev, req);
            return USBD_FAIL;
        }

        {
            USBD_CDC_ItfTypeDef *fops =
                (inst == 0) ? hdual->cli_fops : hdual->audio_fops;
            uint32_t *data_buf =
                (inst == 0) ? hdual->cli_data : hdual->audio_data;
            uint8_t *cmd_op_code =
                (inst == 0) ? &hdual->cli_cmd_op_code : &hdual->audio_cmd_op_code;
            uint8_t *cmd_length =
                (inst == 0) ? &hdual->cli_cmd_length : &hdual->audio_cmd_length;

            if (req->wLength)
            {
                if (req->bmRequest & 0x80U)
                {
                    if (fops != NULL && fops->Control != NULL)
                    {
                        fops->Control(req->bRequest,
                                      (uint8_t *)(void *)data_buf, // NOLINT(bugprone-casting-through-void)
                                      req->wLength);
                    }
                    // NOLINTNEXTLINE(bugprone-casting-through-void)
                    USBD_CtlSendData(pdev, (uint8_t *)(void *)data_buf,
                                     req->wLength);
                }
                else
                {
                    *cmd_op_code = req->bRequest;
                    *cmd_length = (uint8_t)req->wLength;
                    USBD_CtlPrepareRx(pdev,
                                      (uint8_t *)(void *)data_buf, // NOLINT(bugprone-casting-through-void)
                                      req->wLength);
                }
            }
            else
            {
                if (fops != NULL && fops->Control != NULL)
                {
                    // NOLINTNEXTLINE(bugprone-casting-through-void)
                    fops->Control(req->bRequest, (uint8_t *)(void *)req,
                                  0U);
                }
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
                USBD_CtlSendData(pdev, (uint8_t *)(void *)&status_info,
                                 2U);
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

/* ------------------------------------------------------------------ */
/* USBD_CDC_Dual_EP0_RxReady                                          */
/* ------------------------------------------------------------------ */
static void dispatch_pending_control(USBD_CDC_ItfTypeDef *fops, uint8_t *op_code,
                                     uint32_t *data, uint8_t length)
{
    if (*op_code != 0xFFU && fops != NULL && fops->Control != NULL)
    {
        // NOLINTNEXTLINE(bugprone-casting-through-void)
        fops->Control(*op_code, (uint8_t *)(void *)data, (uint16_t)length);
        *op_code = 0xFFU;
    }
}

static uint8_t USBD_CDC_Dual_EP0_RxReady(USBD_HandleTypeDef *pdev)
{
    USBD_CDC_Dual_HandleTypeDef *hdual = get_dual(pdev);
    if (hdual == NULL)
    {
        return USBD_FAIL;
    }

    dispatch_pending_control(hdual->cli_fops, &hdual->cli_cmd_op_code, hdual->cli_data,
                             hdual->cli_cmd_length);
    dispatch_pending_control(hdual->audio_fops, &hdual->audio_cmd_op_code, hdual->audio_data,
                             hdual->audio_cmd_length);

    return USBD_OK;
}

/* ------------------------------------------------------------------ */
/* USBD_CDC_Dual_DataIn  -- dispatch by endpoint number                */
/* ------------------------------------------------------------------ */
static uint8_t USBD_CDC_Dual_DataIn(USBD_HandleTypeDef *pdev,
                                    uint8_t epnum)
{
    USBD_CDC_Dual_HandleTypeDef *hdual = get_dual(pdev);
    if (hdual == NULL)
    {
        return USBD_FAIL;
    }

    PCD_HandleTypeDef *hpcd = pdev->pData;
    int inst = ep_to_instance(epnum);

    if (inst < 0)
    {
        return USBD_FAIL;
    }

    if ((pdev->ep_in[epnum & 0x7FU].total_length > 0U) &&
        ((pdev->ep_in[epnum & 0x7FU].total_length %
          hpcd->IN_ep[epnum & 0x7FU].maxpacket) == 0U))
    {
        pdev->ep_in[epnum & 0x7FU].total_length = 0U;
        USBD_LL_Transmit(pdev, epnum, NULL, 0U);
    }
    else
    {
        if (inst == 0)
        {
            hdual->cli_tx_state = 0U;
        }
        else
        {
            hdual->audio_tx_state = 0U;
        }
    }

    return USBD_OK;
}

/* ------------------------------------------------------------------ */
/* USBD_CDC_Dual_DataOut -- dispatch by endpoint number                */
/* ------------------------------------------------------------------ */
static uint8_t USBD_CDC_Dual_DataOut(USBD_HandleTypeDef *pdev,
                                     uint8_t epnum)
{
    USBD_CDC_Dual_HandleTypeDef *hdual = get_dual(pdev);
    if (hdual == NULL)
    {
        return USBD_FAIL;
    }

    uint32_t rx_len = USBD_LL_GetRxDataSize(pdev, epnum);
    int inst = ep_to_instance(epnum);

    if (inst == 0)
    {
        hdual->cli_rx_length = rx_len;
        if (hdual->cli_fops != NULL && hdual->cli_fops->Receive != NULL)
        {
            hdual->cli_fops->Receive(hdual->cli_rx_buffer, &hdual->cli_rx_length);
        }
    }
    else if (inst == 1)
    {
        hdual->audio_rx_length = rx_len;
        if (hdual->audio_fops != NULL &&
            hdual->audio_fops->Receive != NULL)
        {
            hdual->audio_fops->Receive(hdual->audio_rx_buffer,
                                       &hdual->audio_rx_length);
        }
    }

    return USBD_OK;
}

/* ------------------------------------------------------------------ */
/* Descriptor accessors                                               */
/* ------------------------------------------------------------------ */
static uint8_t *USBD_CDC_Dual_GetFSCfgDesc(uint16_t *length)
{
    *length = sizeof(USBD_CDC_Dual_CfgFSDesc);
    return USBD_CDC_Dual_CfgFSDesc;
}

static uint8_t *USBD_CDC_Dual_GetHSCfgDesc(uint16_t *length)
{
    *length = sizeof(USBD_CDC_Dual_CfgFSDesc);
    return USBD_CDC_Dual_CfgFSDesc;
}

static uint8_t *USBD_CDC_Dual_GetOtherSpeedCfgDesc(uint16_t *length)
{
    // Same layout as the FS config descriptor, only the descriptor-type byte differs.
    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    memcpy(USBD_CDC_Dual_OtherSpeedCfgDesc, USBD_CDC_Dual_CfgFSDesc,
           sizeof(USBD_CDC_Dual_OtherSpeedCfgDesc));
    USBD_CDC_Dual_OtherSpeedCfgDesc[1] = USB_DESC_TYPE_OTHER_SPEED_CONFIGURATION;

    *length = sizeof(USBD_CDC_Dual_OtherSpeedCfgDesc);
    return USBD_CDC_Dual_OtherSpeedCfgDesc;
}

static uint8_t *USBD_CDC_Dual_GetDeviceQualifierDesc(uint16_t *length)
{
    *length = sizeof(USBD_CDC_Dual_DeviceQualifierDesc);
    return USBD_CDC_Dual_DeviceQualifierDesc;
}

/* ------------------------------------------------------------------ */
/* USBD_CDC_Dual_RegisterInterface                                    */
/* ------------------------------------------------------------------ */
uint8_t USBD_CDC_Dual_RegisterInterface(USBD_HandleTypeDef *pdev,
                                        USBD_CDC_ItfTypeDef *fops_cli,
                                        USBD_CDC_ItfTypeDef *fops_audio)
{
    USBD_CDC_Dual_HandleTypeDef *hdual = &usbd_cdc_dual_handle;

    if (fops_cli != NULL)
    {
        hdual->cli_fops = fops_cli;
    }
    if (fops_audio != NULL)
    {
        hdual->audio_fops = fops_audio;
    }

    return USBD_OK;
}

/* ------------------------------------------------------------------ */
/* Internal: set buffers for a specific instance                       */
/* ------------------------------------------------------------------ */
static void set_tx_buffer(USBD_CDC_Dual_HandleTypeDef *hdual, int inst,
                          uint8_t *pbuff, uint16_t length)
{
    if (inst == 0)
    {
        hdual->cli_tx_buffer = pbuff;
        hdual->cli_tx_length = length;
    }
    else
    {
        hdual->audio_tx_buffer = pbuff;
        hdual->audio_tx_length = length;
    }
}

static void set_rx_buffer(USBD_CDC_Dual_HandleTypeDef *hdual, int inst, uint8_t *pbuff)
{
    if (inst == 0)
    {
        hdual->cli_rx_buffer = pbuff;
    }
    else
    {
        hdual->audio_rx_buffer = pbuff;
    }
}

static void prepare_receive(USBD_HandleTypeDef *pdev, int inst)
{
    USBD_CDC_Dual_HandleTypeDef *hdual = get_dual(pdev);
    if (hdual == NULL)
    {
        return;
    }

    if (inst == 0)
    {
        USBD_LL_PrepareReceive(pdev, CDC0_OUT_EP, hdual->cli_rx_buffer,
                               CDC_DUAL_DATA_FS_PACKET_SIZE);
    }
    else
    {
        USBD_LL_PrepareReceive(pdev, CDC1_OUT_EP,
                               hdual->audio_rx_buffer,
                               CDC_DUAL_DATA_FS_PACKET_SIZE);
    }
}

static uint8_t do_transmit(USBD_HandleTypeDef *pdev, int inst,
                           uint8_t *buf, uint16_t len)
{
    USBD_CDC_Dual_HandleTypeDef *hdual = get_dual(pdev);
    if (hdual == NULL)
    {
        return USBD_FAIL;
    }

    uint8_t ep_addr = (inst == 0) ? CDC0_IN_EP : CDC1_IN_EP;
    __IO uint32_t *tx_state =
        (inst == 0) ? &hdual->cli_tx_state : &hdual->audio_tx_state;

    // Claim the pipe atomically
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    if (*tx_state != 0U)
    {
        __set_PRIMASK(primask);
        return USBD_BUSY;
    }
    *tx_state = 1U;
    __set_PRIMASK(primask);

    set_tx_buffer(hdual, inst, buf, len);
    pdev->ep_in[ep_addr & 0x7FU].total_length = len;
    return USBD_LL_Transmit(pdev, ep_addr, buf, len);
}

/* ------------------------------------------------------------------ */
/* Public transmit functions                                           */
/* ------------------------------------------------------------------ */
uint8_t USBD_CDC_Dual_Transmit_FS_CLI(uint8_t *buf, uint16_t len)
{
    return do_transmit(&hUsbDeviceFS, 0, buf, len);
}

uint8_t USBD_CDC_Dual_Transmit_FS_Audio(uint8_t *buf, uint16_t len)
{
    return do_transmit(&hUsbDeviceFS, 1, buf, len);
}

/* ------------------------------------------------------------------ */
/* Public buffer / receive helpers                                    */
/* ------------------------------------------------------------------ */
void USBD_CDC_Dual_SetRxBuffer_CLI(uint8_t *pbuff)
{
    USBD_CDC_Dual_HandleTypeDef *hdual = get_dual(&hUsbDeviceFS);
    if (hdual != NULL)
    {
        set_rx_buffer(hdual, 0, pbuff);
    }
}

void USBD_CDC_Dual_SetTxBuffer_CLI(uint8_t *pbuff, uint16_t len)
{
    USBD_CDC_Dual_HandleTypeDef *hdual = get_dual(&hUsbDeviceFS);
    if (hdual != NULL)
    {
        set_tx_buffer(hdual, 0, pbuff, len);
    }
}

void USBD_CDC_Dual_SetRxBuffer_Audio(uint8_t *pbuff)
{
    USBD_CDC_Dual_HandleTypeDef *hdual = get_dual(&hUsbDeviceFS);
    if (hdual != NULL)
    {
        set_rx_buffer(hdual, 1, pbuff);
    }
}

void USBD_CDC_Dual_SetTxBuffer_Audio(uint8_t *pbuff, uint16_t len)
{
    USBD_CDC_Dual_HandleTypeDef *hdual = get_dual(&hUsbDeviceFS);
    if (hdual != NULL)
    {
        set_tx_buffer(hdual, 1, pbuff, len);
    }
}

void USBD_CDC_Dual_ReceivePacket_CLI(void)
{
    prepare_receive(&hUsbDeviceFS, 0);
}

void USBD_CDC_Dual_ReceivePacket_Audio(void)
{
    prepare_receive(&hUsbDeviceFS, 1);
}
