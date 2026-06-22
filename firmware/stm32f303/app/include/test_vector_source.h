#ifndef TEST_VECTOR_SOURCE_H
#define TEST_VECTOR_SOURCE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "cli_core.h"

#define TEST_VECTOR_SINE 0U
#define TEST_VECTOR_USER 1U
#define TEST_VECTOR_SWEEP 2U

typedef struct
{
    uint32_t sample_rate_hz;
    uint32_t phase_q16;
    uint32_t sweep_freq_hz;
} TestVectorSource;

void test_vector_source_init(TestVectorSource *source, uint32_t sample_rate_hz);
bool test_vector_source_fill_buffer(TestVectorSource *source, const CliTestModeStatus *status,
                                    uint16_t *buf, size_t count);

#endif