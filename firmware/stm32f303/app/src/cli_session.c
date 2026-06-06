#include "cli_session.h"

#include <string.h>

#define CLI_SESSION_BANNER_TITLE "OU Audio Effects CLI"
#define CLI_SESSION_BANNER_HELP "Type help for commands"

static bool session_write(CliSession *session, const char *text)
{
    if (session == NULL || text == NULL || session->transport.write == NULL)
    {
        return false;
    }

    return session->transport.write(text, session->transport.context);
}

static bool session_write_core_text_normalized(CliSession *session, const char *text)
{
    if (session == NULL || text == NULL)
    {
        return false;
    }

    char chunk[64] = {0};
    size_t used = 0;

    for (size_t i = 0; text[i] != '\0'; i++)
    {
        if (text[i] == '\n')
        {
            if (used > 0)
            {
                chunk[used] = '\0';
                if (!session_write(session, chunk))
                {
                    return false;
                }
                used = 0;
            }

            if (!session_write(session, "\r\n"))
            {
                return false;
            }

            continue;
        }

        chunk[used++] = text[i];
        if (used >= (sizeof(chunk) - 1U))
        {
            chunk[used] = '\0';
            if (!session_write(session, chunk))
            {
                return false;
            }
            used = 0;
        }
    }

    if (used > 0)
    {
        chunk[used] = '\0';
        if (!session_write(session, chunk))
        {
            return false;
        }
    }

    return true;
}

static bool session_core_write(const char *text, void *context)
{
    return session_write_core_text_normalized((CliSession *)context, text);
}

static bool session_write_prompt(CliSession *session)
{
    return session_write(session, CLI_SESSION_PROMPT);
}

static bool session_write_char(CliSession *session, char value)
{
    char text[2] = {value, '\0'};
    return session_write(session, text);
}

static bool is_printable_ascii(uint8_t value)
{
    return value >= 0x20U && value <= 0x7EU;
}

static void cli_session_handle_line_complete(CliSession *session)
{
    if (session == NULL)
    {
        return;
    }

    (void)session_write(session, "\r\n");

    if (!session->drop_current_line && session->current_length > 0)
    {
        session->current_line[session->current_length] = '\0';
        (void)cli_core_process_line(session->current_line, session->services, &session->core_io);
    }

    session->current_length = 0;
    session->drop_current_line = false;
    (void)session_write_prompt(session);
}

void cli_session_init(CliSession *session, const CliServices *services,
                      const CliSessionTransport *transport)
{
    if (session == NULL)
    {
        return;
    }

    memset(session, 0, sizeof(*session));

    if (transport != NULL)
    {
        session->transport = *transport;
    }

    session->services = services;
    session->core_io.write = session_core_write;
    session->core_io.context = session;
}

bool cli_session_start(CliSession *session)
{
    if (session == NULL || session->services == NULL || session->transport.write == NULL)
    {
        return false;
    }

    if (session->started)
    {
        return true;
    }

    if (!session_write(session, CLI_SESSION_BANNER_TITLE "\r\n") ||
        !session_write(session, CLI_SESSION_BANNER_HELP "\r\n") ||
        !session_write_prompt(session))
    {
        return false;
    }

    session->started = true;
    return true;
}

void cli_session_push_bytes(CliSession *session, const uint8_t *bytes, size_t byte_count)
{
    if (session == NULL || bytes == NULL || byte_count == 0)
    {
        return;
    }

    for (size_t i = 0; i < byte_count; i++)
    {
        uint8_t value = bytes[i];

        if (value == '\r')
        {
            cli_session_handle_line_complete(session);
            session->last_byte_was_cr = true;
            continue;
        }

        if (value == '\n')
        {
            if (!session->last_byte_was_cr)
            {
                cli_session_handle_line_complete(session);
            }
            session->last_byte_was_cr = false;
            continue;
        }

        session->last_byte_was_cr = false;

        if (value == '\b' || value == 0x7FU)
        {
            if (session->current_length > 0 && !session->drop_current_line)
            {
                session->current_length--;
                (void)session_write(session, "\b \b");
            }
            continue;
        }

        if (!is_printable_ascii(value) || session->drop_current_line)
        {
            continue;
        }

        if (session->current_length >= CLI_MAX_LINE_LENGTH)
        {
            session->drop_current_line = true;
            continue;
        }

        session->current_line[session->current_length++] = (char)value;
        (void)session_write_char(session, (char)value);
    }
}