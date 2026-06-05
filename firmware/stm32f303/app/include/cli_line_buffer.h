#ifndef CLI_LINE_BUFFER_H
#define CLI_LINE_BUFFER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "cli_core.h"

#define CLI_LINE_BUFFER_MAX_LINES 8

typedef struct
{
    char lines[CLI_LINE_BUFFER_MAX_LINES][CLI_MAX_LINE_LENGTH + 1];
    uint8_t head;
    uint8_t tail;
    uint8_t count;
    char current[CLI_MAX_LINE_LENGTH + 1];
    size_t current_length;
    bool drop_current_line;
} CliLineBuffer;

void cli_line_buffer_init(CliLineBuffer *buffer);
void cli_line_buffer_push_bytes(CliLineBuffer *buffer, const uint8_t *bytes, size_t byte_count);
bool cli_line_buffer_pop_line(CliLineBuffer *buffer, char *line_out, size_t line_out_size);

#endif