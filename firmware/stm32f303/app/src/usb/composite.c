#include "usb/composite.h"

#include "usb/cdc_cli.h"
#include "usb/uac.h"
#include "usbd_ctlreq.h"

#include <stddef.h>

// NOLINTBEGIN(readability-identifier-naming)

#define USB_COMPOSITE_CONFIG_DESC_SIZ 249U

/* Combined Full-Speed Configuration Descriptor:
 *  CDC0 (CLI):    Interface 0 (Comm) + Interface 1 (Data)
 *                 EP2 IN (Interrupt), EP1 OUT/IN (Bulk)
 *  UAC1 (Audio):  Interface 2 (Audio Control)
 *                 Interface 3 (Audio Streaming OUT, alt0/alt1)
 *                   EP3 OUT (Isoc, async data), EP4 IN (Isoc, feedback)
 *                 Interface 4 (Audio Streaming IN, alt0/alt1)
 *                   EP3 IN (Isoc, adaptive data)
 */
__ALIGN_BEGIN static uint8_t USBD_Composite_CfgFSDesc[USB_COMPOSITE_CONFIG_DESC_SIZ] __ALIGN_END =
    {
        /* ---- Configuration Descriptor (9 bytes) ---- */
        0x09,
        USB_DESC_TYPE_CONFIGURATION,
        LOBYTE(USB_COMPOSITE_CONFIG_DESC_SIZ),
        HIBYTE(USB_COMPOSITE_CONFIG_DESC_SIZ),
        0x05, /* bNumInterfaces = 5 */
        0x01, /* bConfigurationValue */
        0x00, /* iConfiguration */
        0xC0, /* bmAttributes: self-powered */
        0x32, /* bMaxPower: 100 mA */

        /* CDC0 (CLI) -- Interface Association Descriptor (groups if0+if1)   */
        0x08, 0x0B, 0x00, 0x02, 0x02, 0x02, 0x01, 0x00,

        /* CDC0 (CLI) -- Interface 0 - Communication Class                  */
        0x09, USB_DESC_TYPE_INTERFACE, 0x00, 0x00, 0x01, 0x02, 0x02, 0x01, 0x00,

        /* Header Functional Descriptor */
        0x05, 0x24, 0x00, 0x10, 0x01,

        /* Call Management Functional Descriptor */
        0x05, 0x24, 0x01, 0x00, 0x01, /* bDataInterface = 1 */

        /* ACM Functional Descriptor */
        0x04, 0x24, 0x02, 0x02,

        /* Union Functional Descriptor */
        0x05, 0x24, 0x06, 0x00, 0x01, /* master=if0, slave=if1 */

        /* Endpoint 2 IN Descriptor (Interrupt) */
        0x07, USB_DESC_TYPE_ENDPOINT, CDC0_CMD_EP, 0x03,
        LOBYTE(CDC_DUAL_CMD_PACKET_SIZE), HIBYTE(CDC_DUAL_CMD_PACKET_SIZE), 0x10,

        /* CDC0 (CLI) -- Interface 1 - Data Class Interface                 */
        0x09, USB_DESC_TYPE_INTERFACE, 0x01, 0x00, 0x02, 0x0A, 0x00, 0x00, 0x00,

        /* Endpoint 1 OUT Descriptor (Bulk) */
        0x07, USB_DESC_TYPE_ENDPOINT, CDC0_OUT_EP, 0x02,
        LOBYTE(CDC_DUAL_DATA_FS_PACKET_SIZE), HIBYTE(CDC_DUAL_DATA_FS_PACKET_SIZE), 0x00,

        /* Endpoint 1 IN Descriptor (Bulk) */
        0x07, USB_DESC_TYPE_ENDPOINT, CDC0_IN_EP, 0x02,
        LOBYTE(CDC_DUAL_DATA_FS_PACKET_SIZE), HIBYTE(CDC_DUAL_DATA_FS_PACKET_SIZE), 0x00,

        /* UAC1 -- Interface 2 - Audio Control (no endpoint)                 */
        0x09, USB_DESC_TYPE_INTERFACE, UAC_IFACE_AC, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00,

        /* AC Interface Header: bcdADC=1.00, wTotalLength=52 (this header +
         * 2x(Input Terminal + Output Terminal)), 2 streaming interfaces */
        0x0A, 0x24, 0x01, 0x00, 0x01, 0x34, 0x00, 0x02, UAC_IFACE_AS_OUT, UAC_IFACE_AS_IN,

        /* Input Terminal 1: USB streaming in (feeds the playback path) */
        0x0C, 0x24, 0x02, 0x01, 0x01, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,

        /* Output Terminal 2: Speaker, fed from Input Terminal 1 */
        0x09, 0x24, 0x03, 0x02, 0x01, 0x03, 0x00, 0x01, 0x00,

        /* Input Terminal 3: Microphone (feeds the capture path) */
        0x0C, 0x24, 0x02, 0x03, 0x01, 0x02, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,

        /* Output Terminal 4: USB streaming out, fed from Input Terminal 3 */
        0x09, 0x24, 0x03, 0x04, 0x01, 0x01, 0x00, 0x03, 0x00,

        /* UAC1 -- Interface 3 - Audio Streaming OUT (host->device) */
        /* Alt 0: zero-bandwidth */
        0x09, USB_DESC_TYPE_INTERFACE, UAC_IFACE_AS_OUT, 0x00, 0x00, 0x01, 0x02, 0x00, 0x00,

        /* Alt 1: isoc data OUT + explicit feedback IN */
        0x09, USB_DESC_TYPE_INTERFACE, UAC_IFACE_AS_OUT, 0x01, 0x02, 0x01, 0x02, 0x00, 0x00,

        /* Class-specific AS General: links to Input Terminal 1, PCM format */
        0x07, 0x24, 0x01, 0x01, 0x00, 0x01, 0x00,

        /* Format Type I: mono, 16-bit, one discrete sample rate */
        0x0B, 0x24, 0x02, 0x01, 0x01, 0x02, 0x10, 0x01,
        (uint8_t)(UAC_SAMPLE_RATE_HZ & 0xFFU),
        (uint8_t)((UAC_SAMPLE_RATE_HZ >> 8) & 0xFFU),
        (uint8_t)((UAC_SAMPLE_RATE_HZ >> 16) & 0xFFU),

        /* Isoc data OUT endpoint: Async, Data usage; bSynchAddress points at
         * the feedback endpoint below */
        0x09, USB_DESC_TYPE_ENDPOINT, UAC_AS_OUT_EP, 0x05,
        LOBYTE(UAC_AS_MAX_PACKET_BYTES), HIBYTE(UAC_AS_MAX_PACKET_BYTES),
        0x01, 0x00, UAC_FEEDBACK_EP,

        /* Class-specific AS endpoint descriptor (no sampling-freq/pitch control) */
        0x07, 0x25, 0x01, 0x00, 0x00, 0x00, 0x00,

        /* Explicit feedback endpoint: Isoc, no sync type, Feedback usage.
         * bRefresh=5 -> refresh every 2^5 = 32 ms. */
        0x09, USB_DESC_TYPE_ENDPOINT, UAC_FEEDBACK_EP, 0x11,
        LOBYTE(UAC_FEEDBACK_PACKET_BYTES), HIBYTE(UAC_FEEDBACK_PACKET_BYTES),
        0x01, 0x05, 0x00,

        /* UAC1 -- Interface 4 - Audio Streaming IN (device->host) */
        /* Alt 0: zero-bandwidth */
        0x09, USB_DESC_TYPE_INTERFACE, UAC_IFACE_AS_IN, 0x00, 0x00, 0x01, 0x02, 0x00, 0x00,

        /* Alt 1: isoc data IN */
        0x09, USB_DESC_TYPE_INTERFACE, UAC_IFACE_AS_IN, 0x01, 0x01, 0x01, 0x02, 0x00, 0x00,

        /* Class-specific AS General: links to Output Terminal 4, PCM format */
        0x07, 0x24, 0x01, 0x04, 0x00, 0x01, 0x00,

        /* Format Type I: mono, 16-bit, one discrete sample rate */
        0x0B, 0x24, 0x02, 0x01, 0x01, 0x02, 0x10, 0x01,
        (uint8_t)(UAC_SAMPLE_RATE_HZ & 0xFFU),
        (uint8_t)((UAC_SAMPLE_RATE_HZ >> 8) & 0xFFU),
        (uint8_t)((UAC_SAMPLE_RATE_HZ >> 16) & 0xFFU),

        /* Isoc data IN endpoint: Adaptive, Data usage, no explicit feedback */
        0x09, USB_DESC_TYPE_ENDPOINT, UAC_AS_IN_EP, 0x09,
        LOBYTE(UAC_AS_MAX_PACKET_BYTES), HIBYTE(UAC_AS_MAX_PACKET_BYTES),
        0x01, 0x00, 0x00,

        /* Class-specific AS endpoint descriptor */
        0x07, 0x25, 0x01, 0x00, 0x00, 0x00, 0x00,
};

/* Other-speed descriptor (HS-capable device reporting FS config). Generated on
 * demand from the FS descriptor. */
__ALIGN_BEGIN static uint8_t
    USBD_Composite_OtherSpeedCfgDesc[USB_COMPOSITE_CONFIG_DESC_SIZ] __ALIGN_END;

/* Device Qualifier descriptor */
__ALIGN_BEGIN static uint8_t USBD_Composite_DeviceQualifierDesc
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

static uint8_t USBD_Composite_Init(USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t USBD_Composite_DeInit(USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t USBD_Composite_Setup(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req);
static uint8_t USBD_Composite_EP0_RxReady(USBD_HandleTypeDef *pdev);
static uint8_t USBD_Composite_DataIn(USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t USBD_Composite_DataOut(USBD_HandleTypeDef *pdev, uint8_t epnum);
static void USBD_Composite_SOF(USBD_HandleTypeDef *pdev);
static uint8_t *USBD_Composite_GetFSCfgDesc(uint16_t *length);
static uint8_t *USBD_Composite_GetHSCfgDesc(uint16_t *length);
static uint8_t *USBD_Composite_GetOtherSpeedCfgDesc(uint16_t *length);
static uint8_t *USBD_Composite_GetDeviceQualifierDesc(uint16_t *length);

USBD_ClassTypeDef USBD_Composite =
    {
        USBD_Composite_Init,
        USBD_Composite_DeInit,
        USBD_Composite_Setup,
        NULL, /* EP0_TxSent */
        USBD_Composite_EP0_RxReady,
        USBD_Composite_DataIn,
        USBD_Composite_DataOut,
        USBD_Composite_SOF,
        NULL, /* IsoINIncomplete */
        NULL, /* IsoOUTIncomplete */
        USBD_Composite_GetHSCfgDesc,
        USBD_Composite_GetFSCfgDesc,
        USBD_Composite_GetOtherSpeedCfgDesc,
        USBD_Composite_GetDeviceQualifierDesc,
};

/* Trivial non-NULL marker so USBD_Stop()/etc.'s `if (pdev->pClassData)`
 * checks behave; the CDC0 and UAC1 sub-drivers each keep their own static
 * singleton state and don't need anything stored through this pointer. */
static uint8_t composite_class_data_marker;

static uint8_t USBD_Composite_Init(USBD_HandleTypeDef *pdev, uint8_t cfgidx)
{
    (void)cfgidx;
    pdev->pClassData = &composite_class_data_marker;

    (void)usbd_cdc_cli_class_init(pdev);
    (void)usbd_uac_class_init(pdev);

    return 0U;
}

static uint8_t USBD_Composite_DeInit(USBD_HandleTypeDef *pdev, uint8_t cfgidx)
{
    (void)cfgidx;

    (void)usbd_cdc_cli_class_deinit(pdev);
    (void)usbd_uac_class_deinit(pdev);

    pdev->pClassData = NULL;
    return 0U;
}

static uint8_t USBD_Composite_Setup(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req)
{
    uint8_t recipient = req->bmRequest & USB_REQ_RECIPIENT_MASK;
    uint8_t target = LOBYTE(req->wIndex); /* endpoint address or interface number */

    if (recipient == USB_REQ_RECIPIENT_ENDPOINT)
    {
        if (usbd_cdc_cli_owns_ep(target))
        {
            return usbd_cdc_cli_setup(pdev, req);
        }
        if (usbd_uac_owns_ep(target))
        {
            return usbd_uac_setup(pdev, req);
        }
    }
    else
    {
        if (usbd_cdc_cli_owns_iface(target))
        {
            return usbd_cdc_cli_setup(pdev, req);
        }
        if (usbd_uac_owns_iface(target))
        {
            return usbd_uac_setup(pdev, req);
        }
    }

    USBD_CtlError(pdev, req);
    return USBD_FAIL;
}

static uint8_t USBD_Composite_EP0_RxReady(USBD_HandleTypeDef *pdev)
{
    /* Both sub-drivers may have a pending control-OUT transfer latched from
     * Setup; each checks its own latch and no-ops otherwise. */
    (void)usbd_cdc_cli_ep0_rxready(pdev);
    return USBD_OK;
}

static uint8_t USBD_Composite_DataIn(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
    if (usbd_cdc_cli_owns_ep(epnum))
    {
        return usbd_cdc_cli_data_in(pdev, epnum);
    }
    if (usbd_uac_owns_ep(epnum))
    {
        return usbd_uac_data_in(pdev, epnum);
    }
    return USBD_FAIL;
}

static uint8_t USBD_Composite_DataOut(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
    if (usbd_cdc_cli_owns_ep(epnum))
    {
        return usbd_cdc_cli_data_out(pdev, epnum);
    }
    if (usbd_uac_owns_ep(epnum))
    {
        return usbd_uac_data_out(pdev, epnum);
    }
    return USBD_FAIL;
}

static void USBD_Composite_SOF(USBD_HandleTypeDef *pdev)
{
    usbd_uac_sof(pdev);
}

static uint8_t *USBD_Composite_GetFSCfgDesc(uint16_t *length)
{
    *length = sizeof(USBD_Composite_CfgFSDesc);
    return USBD_Composite_CfgFSDesc;
}

static uint8_t *USBD_Composite_GetHSCfgDesc(uint16_t *length)
{
    *length = sizeof(USBD_Composite_CfgFSDesc);
    return USBD_Composite_CfgFSDesc;
}

static uint8_t *USBD_Composite_GetOtherSpeedCfgDesc(uint16_t *length)
{
    for (size_t i = 0; i < sizeof(USBD_Composite_OtherSpeedCfgDesc); i++)
    {
        USBD_Composite_OtherSpeedCfgDesc[i] = USBD_Composite_CfgFSDesc[i];
    }
    USBD_Composite_OtherSpeedCfgDesc[1] = USB_DESC_TYPE_OTHER_SPEED_CONFIGURATION;

    *length = sizeof(USBD_Composite_OtherSpeedCfgDesc);
    return USBD_Composite_OtherSpeedCfgDesc;
}

static uint8_t *USBD_Composite_GetDeviceQualifierDesc(uint16_t *length)
{
    *length = sizeof(USBD_Composite_DeviceQualifierDesc);
    return USBD_Composite_DeviceQualifierDesc;
}
// NOLINTEND(readability-identifier-naming)
