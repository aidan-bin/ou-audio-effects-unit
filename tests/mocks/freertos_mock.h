#ifndef FREERTOS_MOCK_H
#define FREERTOS_MOCK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FREERTOS_MOCK_MAX_QUEUE_CAPACITY 32
#define FREERTOS_MOCK_MAX_ITEM_SIZE 128

typedef enum {
    FREERTOS_MOCK_OK = 0,
    FREERTOS_MOCK_TIMEOUT = 1,
    FREERTOS_MOCK_ERROR = 2,
} FreeRtosMockStatus;

typedef struct {
    uint32_t giveCount;
    uint32_t takeCount;
    bool available;
} FreeRtosMockBinarySemaphore;

typedef struct {
    uint32_t notifySendCount;
    uint32_t notifyWaitCount;
    bool pending;
    uint32_t lastValue;
} FreeRtosMockTaskNotify;

typedef struct {
    uint32_t sendCount;
    uint32_t receiveCount;

    size_t itemSize;
    size_t capacity;
    size_t count;
    size_t head;
    size_t tail;

    uint8_t storage[FREERTOS_MOCK_MAX_QUEUE_CAPACITY][FREERTOS_MOCK_MAX_ITEM_SIZE];
} FreeRtosMockQueue;

typedef struct {
    FreeRtosMockBinarySemaphore semaphore;
    FreeRtosMockTaskNotify notify;
    FreeRtosMockQueue queue;
} FreeRtosMockState;

extern FreeRtosMockState g_freeRtosMockState;

void freertos_mock_reset(void);

void freertos_mock_semaphore_init(FreeRtosMockBinarySemaphore *semaphore);
void freertos_mock_semaphore_give(FreeRtosMockBinarySemaphore *semaphore);
FreeRtosMockStatus freertos_mock_semaphore_take(FreeRtosMockBinarySemaphore *semaphore);

void freertos_mock_notify_init(FreeRtosMockTaskNotify *notifyState);
void freertos_mock_task_notify(FreeRtosMockTaskNotify *notifyState, uint32_t value);
FreeRtosMockStatus freertos_mock_task_notify_wait(FreeRtosMockTaskNotify *notifyState,
                                                  uint32_t *valueOut);

void freertos_mock_queue_init(FreeRtosMockQueue *queue, size_t itemSize, size_t capacity);
FreeRtosMockStatus freertos_mock_queue_send(FreeRtosMockQueue *queue, const void *item);
FreeRtosMockStatus freertos_mock_queue_receive(FreeRtosMockQueue *queue, void *itemOut);

#endif
