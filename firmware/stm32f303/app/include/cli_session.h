/*
 * cli_session — byte-stream front end for one CLI connection.
 */

#ifndef CLI_SESSION_H
#define CLI_SESSION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "cli_core.h"

#define CLI_SESSION_PROMPT "> "

typedef struct
{
    bool (*write)(const char *text, void *context);
    void *context;
} CliSessionTransport;

typedef struct
{
    const CliServices *services;
    CliSessionTransport transport;
    CliIo core_io;

    char current_line[CLI_MAX_LINE_LENGTH + 1];
    size_t current_length;
    bool drop_current_line;
    bool started;
    bool last_byte_was_cr;
    bool command_stream_mode;
    bool escape_sequence_active;
    bool escape_sequence_csi;
} CliSession;

void cli_session_init(CliSession *session, const CliServices *services,
                      const CliSessionTransport *transport);
bool cli_session_start(CliSession *session);
void cli_session_poll(CliSession *session);
void cli_session_push_bytes(CliSession *session, const uint8_t *bytes, size_t byte_count);

#endif