#include "usb/device_init.h"
#include "usb/cdc_cli.h"
#include "usb/composite.h"
#include "usb_device.h"
#include "usbd_core.h"
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

/* CDC0 (CLI) interface callbacks (defined in main.c) */
extern USBD_CDC_ItfTypeDef USBD_Interface_fops_FS_CLI;

/* ---------------------------------------------------------------- */
/* Composite device descriptor (IAD)                                */
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

void usbd_device_init(void)
{
    if (USBD_Init(&hUsbDeviceFS, &DualCDC_Desc, DEVICE_FS) != USBD_OK)
    {
        Error_Handler();
    }
    if (USBD_RegisterClass(&hUsbDeviceFS, &USBD_Composite) != USBD_OK)
    {
        Error_Handler();
    }
    if (USBD_CDC_CLI_RegisterInterface(&hUsbDeviceFS,
                                       &USBD_Interface_fops_FS_CLI) != USBD_OK)
    {
        Error_Handler();
    }
    if (USBD_Start(&hUsbDeviceFS) != USBD_OK)
    {
        Error_Handler();
    }

    /* Drive D+/D- LOW for 200 ms to force a clean USB disconnect after warm reset. */
    USB->CNTR |= (uint16_t)USB_CNTR_PDWN;

    GPIO_InitTypeDef gpio_off = {
        .Pin = GPIO_PIN_11 | GPIO_PIN_12,
        .Mode = GPIO_MODE_OUTPUT_PP,
        .Pull = GPIO_NOPULL,
        .Speed = GPIO_SPEED_FREQ_LOW,
    };
    HAL_GPIO_Init(GPIOA, &gpio_off);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_11 | GPIO_PIN_12, GPIO_PIN_RESET);

    HAL_Delay(200);

    GPIO_InitTypeDef gpio_usb = {
        .Pin = GPIO_PIN_11 | GPIO_PIN_12,
        .Mode = GPIO_MODE_AF_PP,
        .Pull = GPIO_NOPULL,
        .Speed = GPIO_SPEED_FREQ_HIGH,
        .Alternate = GPIO_AF14_USB,
    };
    HAL_GPIO_Init(GPIOA, &gpio_usb);

    USB->CNTR &= ~((uint16_t)USB_CNTR_PDWN);
    USB->ISTR = 0U;
    USB_EnableGlobalInt(USB);
}
