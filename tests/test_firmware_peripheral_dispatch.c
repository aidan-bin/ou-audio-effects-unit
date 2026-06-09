#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "peripheral_dispatch.h"
#include "harness/expect.h"

int failures = 0;

typedef struct
{
    uint32_t notify_count;
    void *last_notified_task;
    uint32_t last_notify_value;

    uint32_t give_count;
    void *last_given_semaphore;

    uint32_t adc_get_count;
    uint32_t next_adc_value;

    uint32_t i2c_signal_count;
    void *last_i2_c_context;
    bool last_i2_c_transfer_failed;

    uint32_t semaphore_take_count;
    uint32_t semaphore_take_successes_remaining;

    uint32_t dma_start_count;
    bool dma_start_result;

    uint32_t adc_config_count;
    bool adc_config_result;

    uint32_t adc_start_count;
    bool adc_start_result;
} PeripheralDispatchTestState;

static bool test_semaphore_take(void *semaphore_handle, uint32_t timeout_ticks, void *context)
{
    (void)semaphore_handle;

    PeripheralDispatchTestState *state = (PeripheralDispatchTestState *)context;
    state->semaphore_take_count++;

    if (timeout_ticks > 0)
    {
        return true;
    }

    if (state->semaphore_take_successes_remaining == 0)
    {
        return false;
    }

    state->semaphore_take_successes_remaining--;
    return true;
}

static void test_notify_task_from_isr(void *task_handle, uint32_t value, void *context)
{
    PeripheralDispatchTestState *state = (PeripheralDispatchTestState *)context;
    state->notify_count++;
    state->last_notified_task = task_handle;
    state->last_notify_value = value;
}

static void test_give_semaphore_from_isr(void *semaphore_handle, void *context)
{
    PeripheralDispatchTestState *state = (PeripheralDispatchTestState *)context;
    state->give_count++;
    state->last_given_semaphore = semaphore_handle;
}

static bool test_dma_start_it(void *dma_handle, uint32_t src_address, uint32_t dst_address,
                              uint32_t data_length, void *context)
{
    (void)dma_handle;
    (void)src_address;
    (void)dst_address;
    (void)data_length;

    PeripheralDispatchTestState *state = (PeripheralDispatchTestState *)context;
    state->dma_start_count++;
    return state->dma_start_result;
}

static bool test_adc_config_channel(void *adc_handle, void *config, void *context)
{
    (void)adc_handle;
    (void)config;

    PeripheralDispatchTestState *state = (PeripheralDispatchTestState *)context;
    state->adc_config_count++;
    return state->adc_config_result;
}

static bool test_adc_start_it(void *adc_handle, void *context)
{
    (void)adc_handle;

    PeripheralDispatchTestState *state = (PeripheralDispatchTestState *)context;
    state->adc_start_count++;
    return state->adc_start_result;
}

static uint32_t test_adc_get_value(void *adc_handle, void *context)
{
    (void)adc_handle;

    PeripheralDispatchTestState *state = (PeripheralDispatchTestState *)context;
    state->adc_get_count++;
    return state->next_adc_value;
}

static void test_i2c_signal_completion_from_isr(void *i2c_task_support_context, bool transfer_failed,
                                                void *context)
{
    PeripheralDispatchTestState *state = (PeripheralDispatchTestState *)context;
    state->i2c_signal_count++;
    state->last_i2_c_context = i2c_task_support_context;
    state->last_i2_c_transfer_failed = transfer_failed;
}

static PeripheralDispatchOps make_ops(PeripheralDispatchTestState *state)
{
    PeripheralDispatchOps ops = {
        .semaphore_take = test_semaphore_take,
        .task_notify_from_isr = test_notify_task_from_isr,
        .semaphore_give_from_isr = test_give_semaphore_from_isr,
        .dma_start_it = test_dma_start_it,
        .adc_config_channel = test_adc_config_channel,
        .adc_start_it = test_adc_start_it,
        .adc_get_value = test_adc_get_value,
        .i2c_signal_completion_from_isr = test_i2c_signal_completion_from_isr,
        .context = state,
    };

    return ops;
}

static PeripheralDispatchContext make_context(void)
{
    PeripheralDispatchContext dispatch = {
        .effects_task_handle = (void *)0x1001,
        .pot_handler_task_handle = (void *)0x1002,
        .btn_handler_task_handle = (void *)0x1003,
        .effects_adc_handle = (void *)0x2001,
        .pot_adc_handle = (void *)0x2002,
        .i2c_handle = (void *)0x3001,
        .mem_to_mem_dma_handle = (void *)0x4001,
        .i2c_task_support_context = (void *)0x5001,
        .delay_samples_dma_semaphore_handle = (void *)0x6001,
        .adc_buf_a = (void *)0x7001,
        .adc_buf_b = (void *)0x7002,
        .dac_buf_a = (void *)0x8001,
        .dac_buf_b = (void *)0x8002,
    };

    return dispatch;
}

static void test_callbacks_dispatch_expected_notifications(void)
{
    PeripheralDispatchTestState state = {0};
    state.next_adc_value = 123U;

    PeripheralDispatchOps ops = make_ops(&state);
    PeripheralDispatchContext dispatch = make_context();

    peripheral_on_gpio_exti(&dispatch, &ops, 9U);
    expect_eq_u32(1, state.notify_count, "exti notifies button task");
    expect_ptr(dispatch.btn_handler_task_handle, state.last_notified_task, "exti task handle");
    expect_eq_u32(9U, state.last_notify_value, "exti pin value");

    peripheral_on_adc_half_complete(&dispatch, &ops, dispatch.effects_adc_handle);
    expect_eq_u32(2, state.notify_count, "adc half complete notifies effects task");
    expect_ptr(dispatch.effects_task_handle, state.last_notified_task, "adc half task handle");
    expect_eq_u32((uint32_t)(uintptr_t)dispatch.adc_buf_a, state.last_notify_value,
                  "adc half buffer value");

    peripheral_on_adc_complete(&dispatch, &ops, dispatch.pot_adc_handle);
    expect_eq_u32(3, state.notify_count, "pot adc complete notifies pot task");
    expect_ptr(dispatch.pot_handler_task_handle, state.last_notified_task, "pot adc task handle");
    expect_eq_u32(123U, state.last_notify_value, "pot adc value");
    expect_eq_u32(1, state.adc_get_count, "pot adc reads value once");

    peripheral_on_adc_complete(&dispatch, &ops, dispatch.effects_adc_handle);
    expect_eq_u32(4, state.notify_count, "effects adc complete notifies effects task");
    expect_eq_u32((uint32_t)(uintptr_t)dispatch.adc_buf_b, state.last_notify_value,
                  "effects adc buffer value");

    peripheral_on_dac_half_complete(&dispatch, &ops, (void *)0xDEAD);
    expect_eq_u32(5, state.notify_count, "dac half complete notifies effects task");
    expect_eq_u32((uint32_t)(uintptr_t)dispatch.dac_buf_a, state.last_notify_value,
                  "dac half buffer value");

    peripheral_on_dac_complete(&dispatch, &ops, (void *)0xBEEF);
    expect_eq_u32(6, state.notify_count, "dac complete notifies effects task");
    expect_eq_u32((uint32_t)(uintptr_t)dispatch.dac_buf_b, state.last_notify_value,
                  "dac complete buffer value");

    peripheral_on_i2c_tx_complete(&dispatch, &ops, dispatch.i2c_handle);
    expect_eq_u32(1, state.i2c_signal_count, "i2c tx signals completion");
    expect_ptr(dispatch.i2c_task_support_context, state.last_i2_c_context, "i2c context pointer");
    expect_true(!state.last_i2_c_transfer_failed, "i2c tx marks success");

    peripheral_on_i2c_rx_complete(&dispatch, &ops, dispatch.i2c_handle);
    expect_eq_u32(2, state.i2c_signal_count, "i2c rx signals completion");
    expect_true(!state.last_i2_c_transfer_failed, "i2c rx marks success");

    peripheral_on_i2c_error(&dispatch, &ops, dispatch.i2c_handle);
    expect_eq_u32(3, state.i2c_signal_count, "i2c error signals completion");
    expect_true(state.last_i2_c_transfer_failed, "i2c error marks failure");

    peripheral_on_i2c_abort_complete(&dispatch, &ops, dispatch.i2c_handle);
    expect_eq_u32(4, state.i2c_signal_count, "i2c abort signals completion");
    expect_true(state.last_i2_c_transfer_failed, "i2c abort marks failure");

    peripheral_on_dma_complete(&dispatch, &ops, dispatch.mem_to_mem_dma_handle);
    expect_eq_u32(1, state.give_count, "dma complete gives semaphore");
    expect_ptr(dispatch.delay_samples_dma_semaphore_handle, state.last_given_semaphore,
               "dma semaphore handle");
}

static void test_callbacks_ignore_unrelated_handles(void)
{
    PeripheralDispatchTestState state = {0};
    PeripheralDispatchOps ops = make_ops(&state);
    PeripheralDispatchContext dispatch = make_context();

    peripheral_on_adc_half_complete(&dispatch, &ops, (void *)0xAAAA);
    peripheral_on_adc_complete(&dispatch, &ops, (void *)0xBBBB);
    peripheral_on_i2c_tx_complete(&dispatch, &ops, (void *)0xCCCC);
    peripheral_on_i2c_rx_complete(&dispatch, &ops, (void *)0xCCCC);
    peripheral_on_i2c_error(&dispatch, &ops, (void *)0xCCCC);
    peripheral_on_i2c_abort_complete(&dispatch, &ops, (void *)0xCCCC);
    peripheral_on_dma_complete(&dispatch, &ops, (void *)0xDDDD);

    expect_eq_u32(0, state.notify_count, "unrelated adc handles ignored");
    expect_eq_u32(0, state.i2c_signal_count, "unrelated i2c handle ignored");
    expect_eq_u32(0, state.give_count, "unrelated dma handle ignored");
}

static void test_helper_functions(void)
{
    PeripheralDispatchTestState state = {0};
    state.semaphore_take_successes_remaining = 2;
    state.dma_start_result = true;
    state.adc_config_result = true;
    state.adc_start_result = true;

    PeripheralDispatchOps ops = make_ops(&state);

    peripheral_drain_semaphore((void *)0x1111, &ops);
    expect_eq_u32(3, state.semaphore_take_count, "drain consumes until empty");

    state.semaphore_take_successes_remaining = 2;
    bool dma_ok =
        peripheral_start_dma_transfer_and_wait((void *)0x2222, 1, 2, 3, (void *)0x3333, 4, &ops);
    expect_true(dma_ok, "dma helper succeeds");
    expect_eq_u32(1, state.dma_start_count, "dma start called once");

    bool adc_ok = peripheral_start_adc_conversion_it((void *)0x4444, (void *)0x5555, &ops);
    expect_true(adc_ok, "adc helper succeeds");
    expect_eq_u32(1, state.adc_config_count, "adc config called once");
    expect_eq_u32(1, state.adc_start_count, "adc start called once");
}

int main(void)
{
    test_callbacks_dispatch_expected_notifications();
    test_callbacks_ignore_unrelated_handles();
    test_helper_functions();

    if (failures != 0)
    {
        fprintf(stderr, "test_firmware_peripheral_dispatch: %d failure(s)\n", failures);
        return 1;
    }

    return 0;
}
