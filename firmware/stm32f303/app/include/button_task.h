/*
 * button_task — waits for a button event and dispatches it.
 */

#ifndef BUTTON_TASK_H
#define BUTTON_TASK_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    bool (*wait_for_button)(uint16_t *pin_out, void *context);
    void (*dispatch_button)(uint16_t pin, void *context);
    void *context;
} ButtonTaskOps;

bool button_task_step(const ButtonTaskOps *ops);

#endif