#include "cli_parse.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static int hex_nibble(char c)
{
    if (c >= '0' && c <= '9')
    {
        return c - '0';
    }

    c = (char)tolower((unsigned char)c);
    if (c >= 'a' && c <= 'f')
    {
        return 10 + (c - 'a');
    }

    return -1;
}

size_t cli_parse_tokenize(char *line, char *tokens[CLI_PARSE_MAX_TOKENS])
{
    size_t count = 0;
    char *cursor = line;

    while (*cursor != '\0' && count < CLI_PARSE_MAX_TOKENS)
    {
        while (*cursor != '\0' && isspace((unsigned char)*cursor))
        {
            cursor++;
        }

        if (*cursor == '\0')
        {
            break;
        }

        tokens[count++] = cursor;
        while (*cursor != '\0' && !isspace((unsigned char)*cursor))
        {
            cursor++;
        }

        if (*cursor == '\0')
        {
            break;
        }

        *cursor = '\0';
        cursor++;
    }

    return count;
}

bool cli_parse_u32(const char *text, uint32_t *value_out)
{
    if (text == NULL || value_out == NULL)
    {
        return false;
    }

    char *end = NULL;
    unsigned long value = strtoul(text, &end, 0);
    if (end == text || *end != '\0')
    {
        return false;
    }

    *value_out = (uint32_t)value;
    return true;
}

bool cli_parse_i32(const char *text, int32_t *value_out)
{
    if (text == NULL || value_out == NULL)
    {
        return false;
    }

    char *end = NULL;
    long value = strtol(text, &end, 0);
    if (end == text || *end != '\0')
    {
        return false;
    }

    *value_out = (int32_t)value;
    return true;
}

bool cli_parse_bool01(const char *text, bool *value_out)
{
    uint32_t value = 0;
    if (!cli_parse_u32(text, &value) || value > 1)
    {
        return false;
    }

    *value_out = value == 1U;
    return true;
}

bool cli_parse_hex_bytes(const char *text, uint8_t *bytes_out, size_t bytes_capacity,
                         size_t *bytes_out_count)
{
    if (text == NULL || bytes_out == NULL || bytes_out_count == NULL)
    {
        return false;
    }

    size_t len = strlen(text);
    if (len == 0 || (len % 2) != 0)
    {
        return false;
    }

    size_t count = len / 2;
    if (count > bytes_capacity)
    {
        return false;
    }

    for (size_t i = 0; i < count; i++)
    {
        int high = hex_nibble(text[i * 2]);
        int low = hex_nibble(text[i * 2 + 1]);
        if (high < 0 || low < 0)
        {
            return false;
        }

        bytes_out[i] = (uint8_t)((high << 4) | low);
    }

    *bytes_out_count = count;
    return true;
}