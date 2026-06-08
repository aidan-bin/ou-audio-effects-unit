#include "cli_core.h"
#include "cli_parse.h"

#include <stdio.h>
#include <string.h>

typedef CliStatus (*CliCommandHandler)(char **tokens,
                                       size_t token_count,
                                       const CliServices *services,
                                       const CliIo *io,
                                       CliCommandResult *result);

typedef struct
{
    const char *name;
    CliCommandHandler handler;
} CliCommandDefinition;

static bool write_line(const CliIo *io, const char *line)
{
    return io != NULL && io->write != NULL && io->write(line, io->context);
}

static const char *test_vector_name(uint8_t vector)
{
    return vector == 1U ? "lut" : "sine";
}

static const char *command_help_text(const char *command)
{
    if (command == NULL)
    {
        return "commands: help ping override config rom log test\n";
    }

    if (strcmp(command, "help") == 0)
    {
        return "help [command]\n";
    }
    if (strcmp(command, "ping") == 0)
    {
        return "ping\n";
    }
    if (strcmp(command, "override") == 0)
    {
        return "override pot|switch set|clear <index> [value] | clear-all\n";
    }
    if (strcmp(command, "config") == 0)
    {
        return "config set|get <key> [value]\n";
    }
    if (strcmp(command, "rom") == 0)
    {
        return "rom save-state|load-state|read <addr> <len>|write <addr> <hex>\n";
    }
    if (strcmp(command, "log") == 0)
    {
        return "log enable <0|1> | level <0-255> | stream [0|1] [batch <N>] (q to stop) | stats [reset|timing]\n";
    }
    if (strcmp(command, "test") == 0)
    {
        return "test mode <0|1> | vector <sine|lut> | freq <hz> | amp <value> | status\n";
    }

    return NULL;
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

static CliStatus handle_help(char **tokens,
                             size_t token_count,
                             const CliServices *services,
                             const CliIo *io,
                             CliCommandResult *result)
{
    (void)services;
    (void)result;

    if (token_count > 2)
    {
        return CLI_STATUS_INVALID_ARGUMENTS;
    }

    const char *help = command_help_text(token_count == 2 ? tokens[1] : NULL);
    if (help == NULL)
    {
        return CLI_STATUS_UNKNOWN_COMMAND;
    }

    if (!write_line(io, help))
    {
        return CLI_STATUS_SERVICE_ERROR;
    }

    return CLI_STATUS_OK;
}

static CliStatus handle_ping(char **tokens,
                             size_t token_count,
                             const CliServices *services,
                             const CliIo *io,
                             CliCommandResult *result)
{
    (void)tokens;
    (void)token_count;
    (void)services;
    (void)result;

    if (!write_line(io, "pong\n"))
    {
        return CLI_STATUS_SERVICE_ERROR;
    }

    return CLI_STATUS_OK;
}

static CliStatus handle_override(char **tokens,
                                 size_t token_count,
                                 const CliServices *services,
                                 const CliIo *io,
                                 CliCommandResult *result)
{
    (void)io;
    (void)result;

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
            return CLI_STATUS_OK;
        }
    }

    return CLI_STATUS_UNKNOWN_COMMAND;
}

static CliStatus handle_config(char **tokens,
                               size_t token_count,
                               const CliServices *services,
                               const CliIo *io,
                               CliCommandResult *result)
{
    (void)result;

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
        snprintf(line, sizeof(line), "config %ld\n", (long)value);
        write_line(io, line);
        return CLI_STATUS_OK;
    }

    return CLI_STATUS_UNKNOWN_COMMAND;
}

static CliStatus handle_rom(char **tokens,
                            size_t token_count,
                            const CliServices *services,
                            const CliIo *io,
                            CliCommandResult *result)
{
    (void)result;

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
        size_t used = (size_t)snprintf(line, sizeof(line), "rom data ");
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
        return CLI_STATUS_OK;
    }

    return CLI_STATUS_UNKNOWN_COMMAND;
}

static CliStatus handle_log(char **tokens,
                            size_t token_count,
                            const CliServices *services,
                            const CliIo *io,
                            CliCommandResult *result)
{

    if (token_count < 2)
    {
        return CLI_STATUS_INVALID_ARGUMENTS;
    }

    if (strcmp(tokens[1], "stats") == 0)
    {
        if (token_count == 3 && strcmp(tokens[2], "reset") == 0)
        {
            if (services->log_reset_stats == NULL)
            {
                return CLI_STATUS_UNSUPPORTED;
            }
            if (!services->log_reset_stats(services->context))
            {
                return CLI_STATUS_SERVICE_ERROR;
            }
            return CLI_STATUS_OK;
        }

        if (token_count == 3 && strcmp(tokens[2], "timing") == 0)
        {
            if (services->log_get_stats == NULL)
            {
                return CLI_STATUS_UNSUPPORTED;
            }

            CliLogStats stats = {0};
            if (!services->log_get_stats(&stats, services->context))
            {
                return CLI_STATUS_SERVICE_ERROR;
            }

            char line[128];
            if (stats.measuredFrameCount > 0)
            {
                uint32_t avg_us = stats.frameTimeTotalUs / stats.measuredFrameCount;
                (void)snprintf(line, sizeof(line),
                               "log timing mn=%lu mx=%lu avg=%lu meas=%lu over=%lu drop=%lu bs=%u q=%u\n",
                               (unsigned long)stats.frameTimeMinUs,
                               (unsigned long)stats.frameTimeMaxUs,
                               (unsigned long)avg_us, (unsigned long)stats.measuredFrameCount,
                               (unsigned long)stats.overrunCount,
                               (unsigned long)stats.streamDropCount,
                               stats.streamBatchSize,
                               stats.streamQueueCount);
            }
            else
            {
                (void)snprintf(line, sizeof(line),
                               "log timing drop=%lu bs=%u q=%u\n",
                               (unsigned long)stats.streamDropCount,
                               stats.streamBatchSize,
                               stats.streamQueueCount);
            }
            return write_line(io, line) ? CLI_STATUS_OK : CLI_STATUS_SERVICE_ERROR;
        }

        if (token_count != 2)
        {
            return CLI_STATUS_INVALID_ARGUMENTS;
        }

        if (services->log_get_stats == NULL)
        {
            return CLI_STATUS_UNSUPPORTED;
        }

        CliLogStats stats = {0};
        if (!services->log_get_stats(&stats, services->context))
        {
            return CLI_STATUS_SERVICE_ERROR;
        }

        char line[128];
        (void)snprintf(line, sizeof(line),
                       "log stats enabled=%u stream=%u level=%u frames=%lu failures=%lu"
                       " stepfail=%lu streak=%lu\n",
                       stats.enabled ? 1U : 0U, stats.streamEnabled ? 1U : 0U, stats.level,
                       (unsigned long)stats.frameCount,
                       (unsigned long)stats.failureCount,
                       (unsigned long)stats.stepFailureCount,
                       (unsigned long)stats.stepFailureStreak);
        if (!write_line(io, line))
        {
            return CLI_STATUS_SERVICE_ERROR;
        }

        return CLI_STATUS_OK;
    }

    if (strcmp(tokens[1], "stream") == 0)
    {
        if (services->log_set_stream == NULL)
        {
            return CLI_STATUS_UNSUPPORTED;
        }

        bool enabled = true;
        size_t idx = 2;
        if (token_count > idx)
        {
            if (strcmp(tokens[idx], "batch") != 0)
            {
                if (!cli_parse_bool01(tokens[idx], &enabled))
                {
                    return CLI_STATUS_PARSE_ERROR;
                }
                idx++;
            }
        }

        if (token_count > idx)
        {
            if (token_count != idx + 2 || strcmp(tokens[idx], "batch") != 0)
            {
                return CLI_STATUS_INVALID_ARGUMENTS;
            }
            uint32_t batch = 0;
            if (!cli_parse_u32(tokens[idx + 1], &batch) || batch == 0 || batch > 255)
            {
                return CLI_STATUS_PARSE_ERROR;
            }
            if (services->log_set_stream_batch == NULL)
            {
                return CLI_STATUS_UNSUPPORTED;
            }
            if (!services->log_set_stream_batch((uint8_t)batch, services->context))
            {
                return CLI_STATUS_SERVICE_ERROR;
            }
        }

        if (!services->log_set_stream(enabled, services->context))
        {
            return CLI_STATUS_SERVICE_ERROR;
        }

        if (!write_line(io, enabled ? "log stream active (press q to stop)\n"
                                    : "log stream stopped; logging disabled\n"))
        {
            return CLI_STATUS_SERVICE_ERROR;
        }

        if (enabled && result != NULL)
        {
            result->action = CLI_COMMAND_ACTION_ENTER_LOG_STREAM;
        }

        return CLI_STATUS_OK;
    }

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
        return CLI_STATUS_OK;
    }

    return CLI_STATUS_UNKNOWN_COMMAND;
}

static CliStatus handle_test(char **tokens,
                             size_t token_count,
                             const CliServices *services,
                             const CliIo *io,
                             CliCommandResult *result)
{
    (void)result;

    if (token_count < 2)
    {
        return CLI_STATUS_INVALID_ARGUMENTS;
    }

    if (strcmp(tokens[1], "status") == 0)
    {
        if (token_count != 2)
        {
            return CLI_STATUS_INVALID_ARGUMENTS;
        }

        if (services->test_get_status == NULL)
        {
            return CLI_STATUS_UNSUPPORTED;
        }

        CliTestModeStatus status = {0};
        if (!services->test_get_status(&status, services->context))
        {
            return CLI_STATUS_SERVICE_ERROR;
        }

        char line[112];
        snprintf(line, sizeof(line),
                 "test status mode=%u vector=%s freq_hz=%u amp=%u\n",
                 status.enabled ? 1U : 0U, test_vector_name(status.vector), status.frequencyHz,
                 status.amplitude);
        write_line(io, line);
        return CLI_STATUS_OK;
    }

    if (token_count != 3)
    {
        return CLI_STATUS_INVALID_ARGUMENTS;
    }

    if (strcmp(tokens[1], "mode") == 0)
    {
        if (services->test_set_mode == NULL)
        {
            return CLI_STATUS_UNSUPPORTED;
        }

        bool enabled = false;
        if (!cli_parse_bool01(tokens[2], &enabled) ||
            !services->test_set_mode(enabled, services->context))
        {
            return CLI_STATUS_SERVICE_ERROR;
        }

        return CLI_STATUS_OK;
    }

    if (strcmp(tokens[1], "vector") == 0)
    {
        if (services->test_set_vector == NULL)
        {
            return CLI_STATUS_UNSUPPORTED;
        }

        uint8_t vector = 0;
        if (strcmp(tokens[2], "sine") == 0)
        {
            vector = 0;
        }
        else if (strcmp(tokens[2], "lut") == 0)
        {
            vector = 1;
        }
        else
        {
            return CLI_STATUS_PARSE_ERROR;
        }

        if (!services->test_set_vector(vector, services->context))
        {
            return CLI_STATUS_SERVICE_ERROR;
        }

        return CLI_STATUS_OK;
    }

    if (strcmp(tokens[1], "freq") == 0)
    {
        if (services->test_set_frequency_hz == NULL)
        {
            return CLI_STATUS_UNSUPPORTED;
        }

        uint32_t frequency_hz = 0;
        if (!cli_parse_u32(tokens[2], &frequency_hz) || frequency_hz > UINT16_MAX ||
            !services->test_set_frequency_hz((uint16_t)frequency_hz, services->context))
        {
            return CLI_STATUS_SERVICE_ERROR;
        }

        return CLI_STATUS_OK;
    }

    if (strcmp(tokens[1], "amp") == 0)
    {
        if (services->test_set_amplitude == NULL)
        {
            return CLI_STATUS_UNSUPPORTED;
        }

        uint32_t amplitude = 0;
        if (!cli_parse_u32(tokens[2], &amplitude) || amplitude > UINT16_MAX ||
            !services->test_set_amplitude((uint16_t)amplitude, services->context))
        {
            return CLI_STATUS_SERVICE_ERROR;
        }

        return CLI_STATUS_OK;
    }

    return CLI_STATUS_UNKNOWN_COMMAND;
}

CliStatus cli_core_process_line(const char *line, const CliServices *services, const CliIo *io)
{
    return cli_core_process_line_ex(line, services, io, NULL);
}

bool cli_core_stop_log_stream(const CliServices *services)
{
    if (services == NULL || services->log_set_stream == NULL)
    {
        return false;
    }

    return services->log_set_stream(false, services->context);
}

CliStatus cli_core_process_line_ex(const char *line,
                                   const CliServices *services,
                                   const CliIo *io,
                                   CliCommandResult *result_out)
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

    CliCommandResult local_result = {.action = CLI_COMMAND_ACTION_NONE};
    CliCommandResult *result = result_out == NULL ? &local_result : result_out;
    result->action = CLI_COMMAND_ACTION_NONE;

    static const CliCommandDefinition command_table[] = {
        {.name = "help", .handler = handle_help},
        {.name = "ping", .handler = handle_ping},
        {.name = "override", .handler = handle_override},
        {.name = "config", .handler = handle_config},
        {.name = "rom", .handler = handle_rom},
        {.name = "log", .handler = handle_log},
        {.name = "test", .handler = handle_test},
    };

    CliStatus status = CLI_STATUS_UNKNOWN_COMMAND;
    for (size_t i = 0; i < (sizeof(command_table) / sizeof(command_table[0])); i++)
    {
        if (strcmp(tokens[0], command_table[i].name) == 0)
        {
            status = command_table[i].handler(tokens, token_count, services, io, result);
            break;
        }
    }

    const char *error = cli_core_error_text(status);
    if (error != NULL)
    {
        write_line(io, error);
    }

    return status;
}