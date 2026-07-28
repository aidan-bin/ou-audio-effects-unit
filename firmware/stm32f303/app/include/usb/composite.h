/*
 * usbd_composite — the one registered USBD_ClassTypeDef for this device.
 *
 * The ST USB core supports exactly one registered class per device handle,
 * so this dispatcher owns the combined configuration descriptor (CDC0 CLI +
 * UAC1 AC/AS-OUT/AS-IN) and forwards each core callback to cdc_cli.c or
 * uac.c by interface/endpoint number.
 */

#ifndef USBD_COMPOSITE_H
#define USBD_COMPOSITE_H

#include "usbd_def.h"

extern USBD_ClassTypeDef USBD_Composite;

#endif /* USBD_COMPOSITE_H */
