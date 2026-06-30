#include "usb_cdc_dual_init.h"

#include "usb_device.h"
#include "usbd_core.h"
#include "usbd_cdc_dual.h"
#include "usbd_desc.h"

/* String descriptor functions from CubeMX-generated usbd_desc.c */
// NOLINTBEGIN(readability-identifier-naming)
extern uint8_t *USBD_FS_LangIDStrDescriptor(USBD_SpeedTypeDef speed,
                                            uint16_t *length);
extern uint8_t *USBD_FS_ManufacturerStrDescriptor(USBD_SpeedTypeDef speed,
                                                  uint16_t *length);
extern uint8_t *USBD_FS_ProductStrDescriptor(USBD_SpeedTypeDef speed,
                                             uint16_t *length);
extern uint8_t *USBD_FS_SerialStrDescriptor(USBD_SpeedTypeDef speed,
                                            uint16_t *length);
extern uint8_t *USBD_FS_ConfigStrDescriptor(USBD_SpeedTypeDef speed,
                                            uint16_t *length);
extern uint8_t *USBD_FS_InterfaceStrDescriptor(USBD_SpeedTypeDef speed,
                                               uint16_t *length);

/* Dual CDC class handle (declared in usb_device.c) */
extern USBD_HandleTypeDef hUsbDeviceFS;

/* Dual CDC interface callbacks (defined in main.c) */
extern USBD_CDC_ItfTypeDef USBD_Interface_fops_FS_CLI;
extern USBD_CDC_ItfTypeDef USBD_Interface_fops_FS_Audio;

/* ---------------------------------------------------------------- */
/* Dual CDC device descriptor (IAD composite device)                */
/* ---------------------------------------------------------------- */
__ALIGN_BEGIN static uint8_t USBD_DualCDC_DeviceDesc[USB_LEN_DEV_DESC] __ALIGN_END = {
    0x12,                 /* bLength */
    USB_DESC_TYPE_DEVICE, /* bDescriptorType */
    0x00,                 /* bcdUSB */
    0x02,
    0xEF,             /* bDeviceClass: Miscellaneous (IAD device) */
    0x02,             /* bDeviceSubClass: Common Class */
    0x01,             /* bDeviceProtocol: IAD */
    USB_MAX_EP0_SIZE, /* bMaxPacketSize */
    0x83, 0x04,       /* idVendor = 1155 */
    0x40, 0x57,       /* idProduct = 22336 */
    0x00,             /* bcdDevice rel. 2.00 */
    0x02,
    USBD_IDX_MFC_STR,          /* iManufacturer */
    USBD_IDX_PRODUCT_STR,      /* iProduct */
    USBD_IDX_SERIAL_STR,       /* iSerialNumber */
    USBD_MAX_NUM_CONFIGURATION /* bNumConfigurations */
};

static uint8_t *DualCDC_DeviceDescriptor(USBD_SpeedTypeDef speed,
                                         uint16_t *length)
{
    (void)speed;
    *length = sizeof(USBD_DualCDC_DeviceDesc);
    return USBD_DualCDC_DeviceDesc;
}

/* Reuse CubeMX string descriptors, provide custom device descriptor */
static USBD_DescriptorsTypeDef DualCDC_Desc = {
    DualCDC_DeviceDescriptor,
    USBD_FS_LangIDStrDescriptor,
    USBD_FS_ManufacturerStrDescriptor,
    USBD_FS_ProductStrDescriptor,
    USBD_FS_SerialStrDescriptor,
    USBD_FS_ConfigStrDescriptor,
    USBD_FS_InterfaceStrDescriptor,
};
// NOLINTEND(readability-identifier-naming)

void usb_cdc_dual_init(void)
{
    if (USBD_Init(&hUsbDeviceFS, &DualCDC_Desc, DEVICE_FS) != USBD_OK)
    {
        Error_Handler();
    }
    if (USBD_RegisterClass(&hUsbDeviceFS, &USBD_CDC_Dual) != USBD_OK)
    {
        Error_Handler();
    }
    if (USBD_CDC_Dual_RegisterInterface(&hUsbDeviceFS,
                                        &USBD_Interface_fops_FS_CLI,
                                        &USBD_Interface_fops_FS_Audio) != USBD_OK)
    {
        Error_Handler();
    }
    if (USBD_Start(&hUsbDeviceFS) != USBD_OK)
    {
        Error_Handler();
    }
}
