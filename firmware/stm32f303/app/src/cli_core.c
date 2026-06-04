#include "cli_core.h"
#include "cli_parse.h"

#include <stdio.h>
#include <string.h>

static bool write_line(const CliIo *io, const char *line)
{
    return io != NULL && io->write != NULL && io->write(line, io->context);
}

const char *cli_core_error_text(CliStatus status)
{
    switch (status)
    {
    case CLI_STATUS_UNKNOWN_COMMAND:
        return "err unknown-command\n";
    case CLI_STATUS_INVALID_ARGUMENTS:
        return "err invalid-arguments\n";
    case CLI_STATUS_PARSE_ERROR:
        return "err parse\n";
    case CLI_STATUS_UNSUPPORTED:
        return "err unsupported\n";
    case CLI_STATUS_SERVICE_ERROR:
        return "err service\n";
    case CLI_STATUS_OK:
    default:
        return NULL;
    }
}

static CliStatus handle_help(const CliIo *io)
{
    if (!write_line(io,
                    "ok commands: help ping override config rom log\n"))
    {
        return CLI_STATUS_SERVICE_ERROR;
    }

    return CLI_STATUS_OK;
}

static CliStatus handle_ping(const CliIo *io)
{
    if (!write_line(io, "ok pong\n"))
    {
        return CLI_STATUS_SERVICE_ERROR;
    }

    return CLI_STATUS_OK;
}

static CliStatus handle_override(char *tokens[CLI_PARSE_MAX_TOKENS], size_t token_count,
                                 const CliServices *services, const CliIo *io)
{
    if (token_count < 2)
    {
        return CLI_STATUS_INVALID_ARGUMENTS;
    }

    if (strcmp(tokens[1], "clear-all") == 0)
    {
        if (services->clear_all_overrides == NULL)
        {
            return CLI_STATUS_UNSUPPORTED;
        }

        if (!services->clear_all_overrides(services->context))
        {
            return CLI_STATUS_SERVICE_ERROR;
        }

        write_line(io, "ok override clear-all\n");
        return CLI_STATUS_OK;
    }

    if (token_count < 4)
    {
        return CLI_STATUS_INVALID_ARGUMENTS;
    }

    uint32_t index = 0;
    if (!cli_parse_u32(tokens[3], &index))
    {
        return CLI_STATUS_PARSE_ERROR;
    }

    if (strcmp(tokens[1], "pot") == 0)
    {
        if (strcmp(tokens[2], "set") == 0)
        {
            if (token_count != 5 || services->set_pot_override == NULL)
            {
                return token_count == 5 ? CLI_STATUS_UNSUPPORTED : CLI_STATUS_INVALID_ARGUMENTS;
            }

            uint32_t value = 0;
            if (!cli_parse_u32(tokens[4], &value) ||
                !services->set_pot_override((uint8_t)index, value, services->context))
            {
                return CLI_STATUS_SERVICE_ERROR;
            }

            write_line(io, "ok override pot set\n");
            return CLI_STATUS_OK;
        }

        if (strcmp(tokens[2], "clear") == 0)
        {
            if (services->clear_pot_override == NULL)
            {
                return CLI_STATUS_UNSUPPORTED;
            }

            if (!services->clear_pot_override((uint8_t)index, services->context))
            {
                return CLI_STATUS_SERVICE_ERROR;
            }

            write_line(io, "ok override pot clear\n");
            return CLI_STATUS_OK;
        }
    }

    if (strcmp(tokens[1], "switch") == 0)
    {
        if (strcmp(tokens[2], "set") == 0)
        {
            if (token_count != 5 || services->set_switch_override == NULL)
            {
                return token_count == 5 ? CLI_STATUS_UNSUPPORTED : CLI_STATUS_INVALID_ARGUMENTS;
            }

            bool enabled = false;
            if (!cli_parse_bool01(tokens[4], &enabled) ||
                !services->set_switch_override((uint8_t)index, enabled, services->context))
            {
                return CLI_STATUS_SERVICE_ERROR;
            }

            write_line(io, "ok override switch set\n");
            return CLI_STATUS_OK;
        }

        if (strcmp(tokens[2], "clear") == 0)
        {
            if (services->clear_switch_override == NULL)
            {
                return CLI_STATUS_UNSUPPORTED;
            }

            if (!services->clear_switch_override((uint8_t)index, services->context))
            {
                return CLI_STATUS_SERVICE_ERROR;
            }

            write_line(io, "ok override switch clear\n");
            return CLI_STATUS_OK;
        }
    }

    return CLI_STATUS_UNKNOWN_COMMAND;
}

static CliStatus handle_config(char *tokens[CLI_PARSE_MAX_TOKENS], size_t token_count,
                               const CliServices *services, const CliIo *io)
{
    if (token_count < 3)
    {
        return CLI_STATUS_INVALID_ARGUMENTS;
    }

    if (strcmp(tokens[1], "set") == 0)
    {
        if (token_count != 4)
        {
            return CLI_STATUS_INVALID_ARGUMENTS;
        }

        if (services->config_set == NULL)
        {
            return CLI_STATUS_UNSUPPORTED;
        }

        int32_t value = 0;
        if (!cli_parse_i32(tokens[3], &value) ||
            !services->config_set(tokens[2], value, services->context))
        {
            return CLI_STATUS_SERVICE_ERROR;
        }

        write_line(io, "ok config set\n");
        return CLI_STATUS_OK;
    }

    if (strcmp(tokens[1], "get") == 0)
    {
        if (token_count != 3)
        {
            return CLI_STATUS_INVALID_ARGUMENTS;
        }

        if (services->config_get == NULL)
        {
            return CLI_STATUS_UNSUPPORTED;
        }

        int32_t value = 0;
        if (!services->config_get(tokens[2], &value, services->context))
        {
            return CLI_STATUS_SERVICE_ERROR;
        }

        char line[48];
        snprintf(line, sizeof(line), "ok config %ld\n", (long)value);
        write_line(io, line);
        return CLI_STATUS_OK;
    }

    return CLI_STATUS_UNKNOWN_COMMAND;
}

static CliStatus handle_rom(char *tokens[CLI_PARSE_MAX_TOKENS], size_t token_count,
                            const CliServices *services, const CliIo *io)
{
    if (token_count < 2)
    {
        return CLI_STATUS_INVALID_ARGUMENTS;
    }

    if (strcmp(tokens[1], "save-state") == 0)
    {
        if (services->rom_save_state == NULL)
        {
            return CLI_STATUS_UNSUPPORTED;
        }

        if (!services->rom_save_state(services->context))
        {
            return CLI_STATUS_SERVICE_ERROR;
        }

        write_line(io, "ok rom save-state\n");
        return CLI_STATUS_OK;
    }

    if (strcmp(tokens[1], "load-state") == 0)
    {
        if (services->rom_load_state == NULL)
        {
            return CLI_STATUS_UNSUPPORTED;
        }

        if (!services->rom_load_state(services->context))
        {
            return CLI_STATUS_SERVICE_ERROR;
        }

        write_line(io, "ok rom load-state\n");
        return CLI_STATUS_OK;
    }

    if (strcmp(tokens[1], "read") == 0)
    {
        if (token_count != 4)
        {
            return CLI_STATUS_INVALID_ARGUMENTS;
        }

        if (services->rom_read_raw == NULL)
        {
            return CLI_STATUS_UNSUPPORTED;
        }

        uint32_t address = 0;
        uint32_t length = 0;
        if (!cli_parse_u32(tokens[2], &address) || !cli_parse_u32(tokens[3], &length) ||
            length > CLI_MAX_ROM_BYTES)
        {
            return CLI_STATUS_PARSE_ERROR;
        }

        uint8_t bytes[CLI_MAX_ROM_BYTES] = {0};
        size_t bytes_count = 0;
        if (!services->rom_read_raw((uint16_t)address, (uint16_t)length, bytes, sizeof(bytes),
                                    &bytes_count, services->context))
        {
            return CLI_STATUS_SERVICE_ERROR;
        }

        char line[(CLI_MAX_ROM_BYTES * 2) + 16];
        size_t used = (size_t)snprintf(line, sizeof(line), "ok rom data ");
        for (size_t i = 0; i < bytes_count && used + 2 < sizeof(line); i++)
        {
            used += (size_t)snprintf(&line[used], sizeof(line) - used, "%02X", bytes[i]);
        }
        snprintf(&line[used], sizeof(line) - used, "\n");
        write_line(io, line);
        return CLI_STATUS_OK;
    }

    if (strcmp(tokens[1], "write") == 0)
    {
        if (token_count != 4)
        {
            return CLI_STATUS_INVALID_ARGUMENTS;
        }

        if (services->rom_write_raw == NULL)
        {
            return CLI_STATUS_UNSUPPORTED;
        }

        uint32_t address = 0;
        if (!cli_parse_u32(tokens[2], &address))
        {
            return CLI_STATUS_PARSE_ERROR;
        }

        uint8_t bytes[CLI_MAX_ROM_BYTES] = {0};
        size_t bytes_count = 0;
        if (!cli_parse_hex_bytes(tokens[3], bytes, sizeof(bytes), &bytes_count) ||
            !services->rom_write_raw((uint16_t)address, bytes, bytes_count, services->context))
        {
            return CLI_STATUS_SERVICE_ERROR;
        }

        write_line(io, "ok rom write\n");
        return CLI_STATUS_OK;
    }

    return CLI_STATUS_UNKNOWN_COMMAND;
}

static CliStatus handle_log(char *tokens[CLI_PARSE_MAX_TOKENS], size_t token_count,
                            const CliServices *services, const CliIo *io)
{
    if (token_count != 3)
    {
        return CLI_STATUS_INVALID_ARGUMENTS;
    }

    if (strcmp(tokens[1], "level") == 0)
    {
        if (services->log_set_level == NULL)
        {
            return CLI_STATUS_UNSUPPORTED;
        }

        uint32_t level = 0;
        if (!cli_parse_u32(tokens[2], &level) || level > UINT8_MAX ||
            !services->log_set_level((uint8_t)level, services->context))
        {
            return CLI_STATUS_SERVICE_ERROR;
        }

        write_line(io, "ok log level\n");
        return CLI_STATUS_OK;
    }

    if (strcmp(tokens[1], "enable") == 0)
    {
        if (services->log_set_enabled == NULL)
        {
            return CLI_STATUS_UNSUPPORTED;
        }

        bool enabled = false;
        if (!cli_parse_bool01(tokens[2], &enabled) ||
            !services->log_set_enabled(enabled, services->context))
        {
            return CLI_STATUS_SERVICE_ERROR;
        }

        write_line(io, "ok log enable\n");
        return CLI_STATUS_OK;
    }

    return CLI_STATUS_UNKNOWN_COMMAND;
}

CliStatus cli_core_process_line(const char *line, const CliServices *services, const CliIo *io)
{
    if (line == NULL || services == NULL || io == NULL || io->write == NULL)
    {
        return CLI_STATUS_INVALID_ARGUMENTS;
    }

    size_t line_length = strnlen(line, CLI_MAX_LINE_LENGTH + 1);
    if (line_length == 0 || line_length > CLI_MAX_LINE_LENGTH)
    {
        return CLI_STATUS_INVALID_ARGUMENTS;
    }

    char mutable_line[CLI_MAX_LINE_LENGTH + 1];
    memcpy(mutable_line, line, line_length);
    mutable_line[line_length] = '\0';

    char *tokens[CLI_PARSE_MAX_TOKENS] = {0};
    size_t token_count = cli_parse_tokenize(mutable_line, tokens);
    if (token_count == 0)
    {
        return CLI_STATUS_INVALID_ARGUMENTS;
    }

    CliStatus status = CLI_STATUS_UNKNOWN_COMMAND;

    if (strcmp(tokens[0], "help") == 0)
    {
        status = handle_help(io);
    }
    else if (strcmp(tokens[0], "ping") == 0)
    {
        status = handle_ping(io);
    }
    else if (strcmp(tokens[0], "override") == 0)
    {
        status = handle_override(tokens, token_count, services, io);
    }
    else if (strcmp(tokens[0], "config") == 0)
    {
        status = handle_config(tokens, token_count, services, io);
    }
    else if (strcmp(tokens[0], "rom") == 0)
    {
        status = handle_rom(tokens, token_count, services, io);
    }
    else if (strcmp(tokens[0], "log") == 0)
    {
        status = handle_log(tokens, token_count, services, io);
    }

    const char *error = cli_core_error_text(status);
    if (error != NULL)
    {
        write_line(io, error);
    }

    return status;
}