#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "button_task.h"

#include "harness/expect.h"

int failures = 0;

typedef struct
{
    bool waitSucceeds;
    uint16_t pinToReturn;
    uint32_t dispatchCalls;
    uint16_t lastDispatchedPin;
} ButtonTaskTestState;

static bool wait_for_button(uint16_t *pinOut, void *context)
{
    ButtonTaskTestState *state = (ButtonTaskTestState *)context;

    if (!state->waitSucceeds || pinOut == NULL)
    {
        return false;
    }

    *pinOut = state->pinToReturn;
    return true;
}

static void dispatch_button(uint16_t pin, void *context)
{
    ButtonTaskTestState *state = (ButtonTaskTestState *)context;
    state->dispatchCalls++;
    state->lastDispatchedPin = pin;
}

static void test_dispatch_each_button_pin(void)
{
    const uint16_t pins[] = {1U, 2U, 3U};

    for (size_t i = 0; i < sizeof(pins) / sizeof(pins[0]); i++)
    {
        ButtonTaskTestState state = {
            .waitSucceeds = true,
            .pinToReturn = pins[i],
            .dispatchCalls = 0,
            .lastDispatchedPin = 0,
        };

        ButtonTaskOps ops = {
            .wait_for_button = wait_for_button,
            .dispatch_button = dispatch_button,
            .context = &state,
        };

        bool ok = button_task_step(&ops);
        expect_true(ok, "button step succeeds");
        expect_true(state.dispatchCalls == 1, "dispatch called once");
        expect_true(state.lastDispatchedPin == pins[i], "dispatch receives waited pin");
    }
}

static void test_wait_failure_skips_dispatch(void)
{
    ButtonTaskTestState state = {
        .waitSucceeds = false,
        .pinToReturn = 7,
    };

    ButtonTaskOps ops = {
        .wait_for_button = wait_for_button,
        .dispatch_button = dispatch_button,
        .context = &state,
    };

    bool ok = button_task_step(&ops);
    expect_true(!ok, "button step fails on wait timeout");
    expect_true(state.dispatchCalls == 0, "dispatch not called when wait fails");
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
