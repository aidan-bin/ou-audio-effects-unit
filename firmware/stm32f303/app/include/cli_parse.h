/*
 * cli_parse — pure tokenizer/parser for the CLI.
 */

#ifndef CLI_PARSE_H
#define CLI_PARSE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CLI_PARSE_MAX_TOKENS 8

size_t cli_parse_tokenize(char *line, char *tokens[CLI_PARSE_MAX_TOKENS]);
bool cli_parse_u32(const char *text, uint32_t *value_out);
bool cli_parse_i32(const char *text, int32_t *value_out);
bool cli_parse_bool01(const char *text, bool *value_out);
bool cli_parse_hex_bytes(const char *text, uint8_t *bytes_out, size_t bytes_capacity,
                         size_t *bytes_out_count);

#endif