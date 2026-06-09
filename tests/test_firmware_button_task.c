#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "button_task.h"

#include "harness/expect.h"

int failures = 0;

typedef struct
{
    bool wait_succeeds;
    uint16_t pin_to_return;
    uint32_t dispatch_calls;
    uint16_t last_dispatched_pin;
} ButtonTaskTestState;

static bool wait_for_button(uint16_t *pin_out, void *context)
{
    ButtonTaskTestState *state = (ButtonTaskTestState *)context;

    if (!state->wait_succeeds || pin_out == NULL)
    {
        return false;
    }

    *pin_out = state->pin_to_return;
    return true;
}

static void dispatch_button(uint16_t pin, void *context)
{
    ButtonTaskTestState *state = (ButtonTaskTestState *)context;
    state->dispatch_calls++;
    state->last_dispatched_pin = pin;
}

static void test_dispatch_each_button_pin(void)
{
    const uint16_t pins[] = {1U, 2U, 3U};

    for (size_t i = 0; i < sizeof(pins) / sizeof(pins[0]); i++)
    {
        ButtonTaskTestState state = {
            .wait_succeeds = true,
            .pin_to_return = pins[i],
            .dispatch_calls = 0,
            .last_dispatched_pin = 0,
        };

        ButtonTaskOps ops = {
            .wait_for_button = wait_for_button,
            .dispatch_button = dispatch_button,
            .context = &state,
        };

        bool ok = button_task_step(&ops);
        expect_true(ok, "button step succeeds");
        expect_true(state.dispatch_calls == 1, "dispatch called once");
        expect_true(state.last_dispatched_pin == pins[i], "dispatch receives waited pin");
    }
}

static void test_wait_failure_skips_dispatch(void)
{
    ButtonTaskTestState state = {
        .wait_succeeds = false,
        .pin_to_return = 7,
    };

    ButtonTaskOps ops = {
        .wait_for_button = wait_for_button,
        .dispatch_button = dispatch_button,
        .context = &state,
    };

    bool ok = button_task_step(&ops);
    expect_true(!ok, "button step fails on wait timeout");
    expect_true(state.dispatch_calls == 0, "dispatch not called when wait fails");
}

int main(void)
{
    test_dispatch_each_button_pin();
    test_wait_failure_skips_dispatch();

    if (failures != 0)
    {
        fprintf(stderr, "test_firmware_button_task: %d failure(s)\n", failures);
        return 1;
    }

    return 0;
}
