#ifndef HOST_OPS_H
#define HOST_OPS_H

#include "host_harness.h"
#include "peripheral_dispatch.h"

void host_ops_init_peripheral_dispatch(const HostHarness *harness, PeripheralDispatchContext *dispatch,
    PeripheralDispatchOps *ops);

#endif