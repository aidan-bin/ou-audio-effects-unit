/*
 * usbd_device_init — registers the composite USB class (CDC0 CLI + UAC1) and starts the device.
 */

#ifndef USB_DEVICE_INIT_H
#define USB_DEVICE_INIT_H

#ifdef __cplusplus
extern "C"
{
#endif

    void usbd_device_init(void);

#ifdef __cplusplus
}
#endif

#endif /* USB_DEVICE_INIT_H */
