#ifndef TEST_EXPECT_H
#define TEST_EXPECT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

extern int failures;

static inline void expect_true(bool condition, const char *label)
{
    if (!condition)
    {
        fprintf(stderr, "FAIL: %s expected=true actual=false\n", label);
        failures++;
    }
}

static inline void expect_false(bool condition, const char *label)
{
    if (condition)
    {
        fprintf(stderr, "FAIL: %s expected=false actual=true\n", label);
        failures++;
    }
}

static inline void expect_eq_u8(uint8_t expected, uint8_t actual, const char *label)
{
    if (expected != actual)
    {
        fprintf(stderr, "FAIL: %s expected=%u actual=%u\n", label, expected, actual);
        failures++;
    }
}

static inline void expect_eq_u16(uint16_t expected, uint16_t actual, const char *label)
{
    if (expected != actual)
    {
        fprintf(stderr, "FAIL: %s expected=%u actual=%u\n", label, expected, actual);
        failures++;
    }
}

static inline void expect_eq_u32(uint32_t expected, uint32_t actual, const char *label)
{
    if (expected != actual)
    {
        fprintf(stderr, "FAIL: %s expected=%u actual=%u\n", label, expected, actual);
        failures++;
    }
}

static inline void expect_eq_size(size_t expected, size_t actual, const char *label)
{
    if (expected != actual)
    {
        fprintf(stderr, "FAIL: %s expected=%zu actual=%zu\n", label, expected, actual);
        failures++;
    }
}

#endif