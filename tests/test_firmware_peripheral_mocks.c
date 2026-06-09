#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "mocks/freertos_mock.h"
#include "mocks/hal_mock.h"
#include "harness/expect.h"

int failures = 0;
static int g_i2c_tx_callback_hits = 0;

static void on_i2c_tx_complete(void *context)
{
    (void)context;
    g_i2c_tx_callback_hits++;
}

static void test_hal_i2c_tx_records_payload(void)
{
    hal_mock_reset();

    const uint8_t payload[] = {0xAA, 0xBB, 0xCC, 0xDD};
    HalMockStatus status = hal_mock_i2c_master_transmit_dma(0x3C, payload, sizeof(payload));

    expect_eq_u32(HAL_MOCK_STATUS_OK, status, "i2c tx status ok");
    expect_eq_u32(1, g_hal_mock_state.i2c.tx_count, "i2c tx count");
    expect_eq_u16(0x3C, g_hal_mock_state.i2c.last_tx_address, "i2c tx addr");
    expect_eq_u32(sizeof(payload), g_hal_mock_state.i2c.last_tx_size, "i2c tx size");
    expect_eq_u8(0xAA, g_hal_mock_state.i2c.last_tx_bytes[0], "i2c tx first byte");
    expect_eq_u8(0xDD, g_hal_mock_state.i2c.last_tx_bytes[3], "i2c tx last byte");
}

static void test_hal_i2c_rx_copies_buffer(void)
{
    hal_mock_reset();

    g_hal_mock_state.i2c.next_rx_bytes[0] = 0x11;
    g_hal_mock_state.i2c.next_rx_bytes[1] = 0x22;

    uint8_t rx[2] = {0};
    HalMockStatus status = hal_mock_i2c_master_receive_dma(0xA0, rx, sizeof(rx));

    expect_eq_u32(HAL_MOCK_STATUS_OK, status, "i2c rx status ok");
    expect_eq_u32(1, g_hal_mock_state.i2c.rx_count, "i2c rx count");
    expect_eq_u8(0x11, rx[0], "i2c rx byte0");
    expect_eq_u8(0x22, rx[1], "i2c rx byte1");
}

static void test_hal_i2c_auto_callback_on_success(void)
{
    hal_mock_reset();
    g_i2c_tx_callback_hits = 0;

    HalMockI2CCallbacks callbacks = {
        .tx_complete = on_i2c_tx_complete,
        .rx_complete = NULL,
        .error = NULL,
        .context = NULL,
    };

    hal_mock_set_i2c_callbacks(callbacks);
    g_hal_mock_state.i2c.auto_invoke_tx_complete = true;

    const uint8_t payload = 0x5A;
    (void)hal_mock_i2c_master_transmit_dma(0x3C, &payload, 1);

    expect_eq_u32(1, (uint32_t)g_i2c_tx_callback_hits, "i2c tx callback invoked once");
}

static void test_freertos_semaphore_give_take(void)
{
    freertos_mock_reset();

    expect_eq_u32(FREERTOS_MOCK_TIMEOUT,
                  freertos_mock_semaphore_take(&g_free_rtos_mock_state.semaphore),
                  "semaphore empty times out");

    freertos_mock_semaphore_give(&g_free_rtos_mock_state.semaphore);

    expect_eq_u32(FREERTOS_MOCK_OK, freertos_mock_semaphore_take(&g_free_rtos_mock_state.semaphore),
                  "semaphore give then take succeeds");
    expect_eq_u32(1, g_free_rtos_mock_state.semaphore.give_count, "semaphore give count");
    expect_eq_u32(2, g_free_rtos_mock_state.semaphore.take_count, "semaphore take count");
}

static void test_freertos_notify_wait_round_trip(void)
{
    freertos_mock_reset();

    uint32_t value = 0;
    expect_eq_u32(FREERTOS_MOCK_TIMEOUT,
                  freertos_mock_task_notify_wait(&g_free_rtos_mock_state.notify, &value),
                  "notify wait empty times out");

    freertos_mock_task_notify(&g_free_rtos_mock_state.notify, 42);

    expect_eq_u32(FREERTOS_MOCK_OK,
                  freertos_mock_task_notify_wait(&g_free_rtos_mock_state.notify, &value),
                  "notify wait receives value");
    expect_eq_u32(42, value, "notify value");
}

static void test_freertos_queue_fifo(void)
{
    freertos_mock_reset();

    FreeRtosMockQueue queue;
    freertos_mock_queue_init(&queue, sizeof(uint32_t), 2);

    uint32_t first = 10;
    uint32_t second = 20;
    uint32_t out = 0;

    expect_eq_u32(FREERTOS_MOCK_OK, freertos_mock_queue_send(&queue, &first), "queue send first");
    expect_eq_u32(FREERTOS_MOCK_OK, freertos_mock_queue_send(&queue, &second), "queue send second");
    expect_eq_u32(FREERTOS_MOCK_TIMEOUT, freertos_mock_queue_send(&queue, &second),
                  "queue full times out");

    expect_eq_u32(FREERTOS_MOCK_OK, freertos_mock_queue_receive(&queue, &out),
                  "queue receive first");
    expect_eq_u32(10, out, "queue first value");

    expect_eq_u32(FREERTOS_MOCK_OK, freertos_mock_queue_receive(&queue, &out),
                  "queue receive second");
    expect_eq_u32(20, out, "queue second value");

    expect_eq_u32(FREERTOS_MOCK_TIMEOUT, freertos_mock_queue_receive(&queue, &out),
                  "queue empty times out");
}

int main(void)
{
    test_hal_i2c_tx_records_payload();
    test_hal_i2c_rx_copies_buffer();
    test_hal_i2c_auto_callback_on_success();
    test_freertos_semaphore_give_take();
    test_freertos_notify_wait_round_trip();
    test_freertos_queue_fifo();

    expect_true(failures == 0, "all peripheral mock tests pass");

    if (failures != 0)
    {
        fprintf(stderr, "Peripheral mock tests failed: %d\n", failures);
        return 1;
    }

    puts("Peripheral mock tests passed");
    return 0;
}
