#include "cli_line_buffer.h"

#include <string.h>

static void queue_current_line(CliLineBuffer *buffer)
{
    if (buffer->drop_current_line || buffer->current_length == 0)
    {
        buffer->current_length = 0;
        buffer->drop_current_line = false;
        return;
    }

    if (buffer->count >= CLI_LINE_BUFFER_MAX_LINES)
    {
        buffer->head = (uint8_t)((buffer->head + 1U) % CLI_LINE_BUFFER_MAX_LINES);
        buffer->count--;
    }

    memcpy(buffer->lines[buffer->tail], buffer->current, buffer->current_length);
    buffer->lines[buffer->tail][buffer->current_length] = '\0';
    buffer->tail = (uint8_t)((buffer->tail + 1U) % CLI_LINE_BUFFER_MAX_LINES);
    buffer->count++;

    buffer->current_length = 0;
    buffer->drop_current_line = false;
}

void cli_line_buffer_init(CliLineBuffer *buffer)
{
    if (buffer == NULL)
    {
        return;
    }

    memset(buffer, 0, sizeof(*buffer));
}

void cli_line_buffer_push_bytes(CliLineBuffer *buffer, const uint8_t *bytes, size_t byte_count)
{
    if (buffer == NULL || bytes == NULL)
    {
        return;
    }

    for (size_t i = 0; i < byte_count; i++)
    {
        const uint8_t value = bytes[i];
        if (value == '\n' || value == '\r')
        {
            queue_current_line(buffer);
            continue;
        }

        if (buffer->drop_current_line)
        {
            continue;
        }

        if (buffer->current_length >= CLI_MAX_LINE_LENGTH)
        {
            buffer->drop_current_line = true;
            continue;
        }

        buffer->current[buffer->current_length++] = (char)value;
    }
}

bool cli_line_buffer_pop_line(CliLineBuffer *buffer, char *line_out, size_t line_out_size)
{
    if (buffer == NULL || line_out == NULL || line_out_size == 0)
    {
        return false;
    }

    if (buffer->count == 0)
    {
        return false;
    }

    const char *line = buffer->lines[buffer->head];
    const size_t line_length = strnlen(line, CLI_MAX_LINE_LENGTH + 1);
    if (line_length + 1 > line_out_size)
    {
        return false;
    }

    memcpy(line_out, line, line_length + 1);
    buffer->head = (uint8_t)((buffer->head + 1U) % CLI_LINE_BUFFER_MAX_LINES);
    buffer->count--;
    return true;
}