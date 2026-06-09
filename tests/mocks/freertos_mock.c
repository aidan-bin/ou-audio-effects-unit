#include "freertos_mock.h"

#include <string.h>

FreeRtosMockState g_free_rtos_mock_state;

static bool queue_config_valid(size_t item_size, size_t capacity)
{
    return item_size > 0 && item_size <= FREERTOS_MOCK_MAX_ITEM_SIZE && capacity > 0 &&
           capacity <= FREERTOS_MOCK_MAX_QUEUE_CAPACITY;
}

void freertos_mock_reset(void)
{
    memset(&g_free_rtos_mock_state, 0, sizeof(g_free_rtos_mock_state));

    freertos_mock_semaphore_init(&g_free_rtos_mock_state.semaphore);
    freertos_mock_notify_init(&g_free_rtos_mock_state.notify);
    freertos_mock_queue_init(&g_free_rtos_mock_state.queue, sizeof(uint32_t),
                             FREERTOS_MOCK_MAX_QUEUE_CAPACITY);
}

void freertos_mock_semaphore_init(FreeRtosMockBinarySemaphore *semaphore)
{
    if (semaphore == NULL)
    {
        return;
    }

    memset(semaphore, 0, sizeof(*semaphore));
}

void freertos_mock_semaphore_give(FreeRtosMockBinarySemaphore *semaphore)
{
    if (semaphore == NULL)
    {
        return;
    }

    semaphore->give_count++;
    semaphore->available = true;
}

FreeRtosMockStatus freertos_mock_semaphore_take(FreeRtosMockBinarySemaphore *semaphore)
{
    if (semaphore == NULL)
    {
        return FREERTOS_MOCK_ERROR;
    }

    semaphore->take_count++;

    if (!semaphore->available)
    {
        return FREERTOS_MOCK_TIMEOUT;
    }

    semaphore->available = false;
    return FREERTOS_MOCK_OK;
}

void freertos_mock_notify_init(FreeRtosMockTaskNotify *notify_state)
{
    if (notify_state == NULL)
    {
        return;
    }

    memset(notify_state, 0, sizeof(*notify_state));
}

void freertos_mock_task_notify(FreeRtosMockTaskNotify *notify_state, uint32_t value)
{
    if (notify_state == NULL)
    {
        return;
    }

    notify_state->notify_send_count++;
    notify_state->pending = true;
    notify_state->last_value = value;
}

FreeRtosMockStatus freertos_mock_task_notify_wait(FreeRtosMockTaskNotify *notify_state,
                                                  uint32_t *value_out)
{
    if (notify_state == NULL)
    {
        return FREERTOS_MOCK_ERROR;
    }

    notify_state->notify_wait_count++;

    if (!notify_state->pending)
    {
        return FREERTOS_MOCK_TIMEOUT;
    }

    if (value_out != NULL)
    {
        *value_out = notify_state->last_value;
    }

    notify_state->pending = false;
    return FREERTOS_MOCK_OK;
}

void freertos_mock_queue_init(FreeRtosMockQueue *queue, size_t item_size, size_t capacity)
{
    if (queue == NULL)
    {
        return;
    }

    memset(queue, 0, sizeof(*queue));

    if (!queue_config_valid(item_size, capacity))
    {
        return;
    }

    queue->item_size = item_size;
    queue->capacity = capacity;
}

FreeRtosMockStatus freertos_mock_queue_send(FreeRtosMockQueue *queue, const void *item)
{
    if (queue == NULL || item == NULL || queue->item_size == 0 || queue->capacity == 0)
    {
        return FREERTOS_MOCK_ERROR;
    }

    queue->send_count++;

    if (queue->count >= queue->capacity)
    {
        return FREERTOS_MOCK_TIMEOUT;
    }

    memcpy(queue->storage[queue->tail], item, queue->item_size);
    queue->tail = (queue->tail + 1U) % queue->capacity;
    queue->count++;
    return FREERTOS_MOCK_OK;
}

FreeRtosMockStatus freertos_mock_queue_receive(FreeRtosMockQueue *queue, void *item_out)
{
    if (queue == NULL || item_out == NULL || queue->item_size == 0 || queue->capacity == 0)
    {
        return FREERTOS_MOCK_ERROR;
    }

    queue->receive_count++;

    if (queue->count == 0)
    {
        return FREERTOS_MOCK_TIMEOUT;
    }

    memcpy(item_out, queue->storage[queue->head], queue->item_size);
    queue->head = (queue->head + 1U) % queue->capacity;
    queue->count--;
    return FREERTOS_MOCK_OK;
}
