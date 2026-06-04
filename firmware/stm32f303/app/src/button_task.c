#include "button_task.h"

#include <stddef.h>

bool button_task_step(const ButtonTaskOps *ops)
{
    if (ops == NULL || ops->wait_for_button == NULL || ops->dispatch_button == NULL)
    {
        return false;
    }

    uint16_t pin = 0;
    if (!ops->wait_for_button(&pin, ops->context))
    {
        return false;
    }

    ops->dispatch_button(pin, ops->context);
    return true;
}
