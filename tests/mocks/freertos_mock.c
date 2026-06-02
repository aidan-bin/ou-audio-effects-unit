#include "freertos_mock.h"

#include <string.h>

FreeRtosMockState g_freeRtosMockState;

static bool queue_config_valid(size_t itemSize, size_t capacity) {
    return itemSize > 0 && itemSize <= FREERTOS_MOCK_MAX_ITEM_SIZE && capacity > 0 &&
           capacity <= FREERTOS_MOCK_MAX_QUEUE_CAPACITY;
}

void freertos_mock_reset(void) {
    memset(&g_freeRtosMockState, 0, sizeof(g_freeRtosMockState));

    freertos_mock_semaphore_init(&g_freeRtosMockState.semaphore);
    freertos_mock_notify_init(&g_freeRtosMockState.notify);
    freertos_mock_queue_init(&g_freeRtosMockState.queue, sizeof(uint32_t),
                             FREERTOS_MOCK_MAX_QUEUE_CAPACITY);
}

void freertos_mock_semaphore_init(FreeRtosMockBinarySemaphore *semaphore) {
    if (semaphore == NULL) {
        return;
    }

    memset(semaphore, 0, sizeof(*semaphore));
}

void freertos_mock_semaphore_give(FreeRtosMockBinarySemaphore *semaphore) {
    if (semaphore == NULL) {
        return;
    }

    semaphore->giveCount++;
    semaphore->available = true;
}

FreeRtosMockStatus freertos_mock_semaphore_take(FreeRtosMockBinarySemaphore *semaphore) {
    if (semaphore == NULL) {
        return FREERTOS_MOCK_ERROR;
    }

    semaphore->takeCount++;

    if (!semaphore->available) {
        return FREERTOS_MOCK_TIMEOUT;
    }

    semaphore->available = false;
    return FREERTOS_MOCK_OK;
}

void freertos_mock_notify_init(FreeRtosMockTaskNotify *notifyState) {
    if (notifyState == NULL) {
        return;
    }

    memset(notifyState, 0, sizeof(*notifyState));
}

void freertos_mock_task_notify(FreeRtosMockTaskNotify *notifyState, uint32_t value) {
    if (notifyState == NULL) {
        return;
    }

    notifyState->notifySendCount++;
    notifyState->pending = true;
    notifyState->lastValue = value;
}

FreeRtosMockStatus freertos_mock_task_notify_wait(FreeRtosMockTaskNotify *notifyState,
                                                  uint32_t *valueOut) {
    if (notifyState == NULL) {
        return FREERTOS_MOCK_ERROR;
    }

    notifyState->notifyWaitCount++;

    if (!notifyState->pending) {
        return FREERTOS_MOCK_TIMEOUT;
    }

    if (valueOut != NULL) {
        *valueOut = notifyState->lastValue;
    }

    notifyState->pending = false;
    return FREERTOS_MOCK_OK;
}

void freertos_mock_queue_init(FreeRtosMockQueue *queue, size_t itemSize, size_t capacity) {
    if (queue == NULL) {
        return;
    }

    memset(queue, 0, sizeof(*queue));

    if (!queue_config_valid(itemSize, capacity)) {
        return;
    }

    queue->itemSize = itemSize;
    queue->capacity = capacity;
}

FreeRtosMockStatus freertos_mock_queue_send(FreeRtosMockQueue *queue, const void *item) {
    if (queue == NULL || item == NULL || queue->itemSize == 0 || queue->capacity == 0) {
        return FREERTOS_MOCK_ERROR;
    }

    queue->sendCount++;

    if (queue->count >= queue->capacity) {
        return FREERTOS_MOCK_TIMEOUT;
    }

    memcpy(queue->storage[queue->tail], item, queue->itemSize);
    queue->tail = (queue->tail + 1U) % queue->capacity;
    queue->count++;
    return FREERTOS_MOCK_OK;
}

FreeRtosMockStatus freertos_mock_queue_receive(FreeRtosMockQueue *queue, void *itemOut) {
    if (queue == NULL || itemOut == NULL || queue->itemSize == 0 || queue->capacity == 0) {
        return FREERTOS_MOCK_ERROR;
    }

    queue->receiveCount++;

    if (queue->count == 0) {
        return FREERTOS_MOCK_TIMEOUT;
    }

    memcpy(itemOut, queue->storage[queue->head], queue->itemSize);
    queue->head = (queue->head + 1U) % queue->capacity;
    queue->count--;
    return FREERTOS_MOCK_OK;
}
