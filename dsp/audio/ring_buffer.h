/*
 * ring_buffer — circular buffer index helpers over a caller-owned uint16_t array.
 */

#ifndef OU_RING_BUFFER_H
#define OU_RING_BUFFER_H

#include <stddef.h>
#include <stdint.h>

static inline size_t ring_wrap(size_t index, size_t len)
{
    return index % len;
}

static inline size_t ring_advance(size_t index, size_t steps, size_t len)
{
    return (index + steps) % len;
}

static inline size_t ring_rewind(size_t index, size_t steps, size_t len)
{
    return (index + len - (steps % len)) % len;
}

static inline uint16_t ring_read(const uint16_t *buf, size_t len, size_t index)
{
    return buf[ring_wrap(index, len)];
}

static inline void ring_write(uint16_t *buf, size_t len, size_t index, uint16_t value)
{
    buf[ring_wrap(index, len)] = value;
}

static inline void ring_fill(uint16_t *buf, size_t len, uint16_t value)
{
    for (size_t i = 0; i < len; i++)
    {
        buf[i] = value;
    }
}

#endif
