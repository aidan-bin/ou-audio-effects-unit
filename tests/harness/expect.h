#ifndef TEST_EXPECT_H
#define TEST_EXPECT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

extern int failures;

// For void test functions: fail and return early if expr is non-zero.
#define EXPECT_OK(expr, label)                      \
    do                                              \
    {                                               \
        if ((expr) != 0)                            \
        {                                           \
            fprintf(stderr, "FAIL: %s\n", (label)); \
            failures++;                             \
            return;                                 \
        }                                           \
    } while (0)


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

static inline void expect_eq_i16(int16_t expected, int16_t actual, const char *label)
{
    if (expected != actual)
    {
        fprintf(stderr, "FAIL: %s expected=%d actual=%d\n", label, expected, actual);
        failures++;
    }
}

static inline void expect_eq_u16_array(const uint16_t *expected, const uint16_t *actual,
                                       size_t count, const char *label)
{
    for (size_t i = 0; i < count; i++)
    {
        if (expected[i] != actual[i])
        {
            fprintf(stderr, "FAIL: %s[%zu] expected=%u actual=%u\n", label, i, expected[i],
                    actual[i]);
            failures++;
        }
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

static inline void expect_ptr(const void *expected, const void *actual, const char *label)
{
    if (expected != actual)
    {
        fprintf(stderr, "FAIL: %s expected=%p actual=%p\n", label, expected, actual);
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