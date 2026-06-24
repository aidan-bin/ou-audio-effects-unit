#include "cli_core.h"
#include "cli_parse.h"

#include <stdio.h>
#include <string.h>

#define CONFIG_LINE_BUF_SIZE 48
#define LOG_LINE_BUF_SIZE 128
#define LOG_STREAM_BATCH_MAX 255
#define TEST_STATUS_LINE_BUF_SIZE 112
#define INFO_LINE_BUF_SIZE 96

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
    if (vector == 1U)
        return "lut";
    if (vector == 2U)
        return "sweep";
    return "sine";
}

static const char *command_help_text(const char *command)
{
    if (command == NULL)
    {
        return "commands: help ping override config rom log test i2c reboot info\n";
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
    if (strcmp(command, "reboot") == 0)
    {
        return "reboot\n";
    }
    if (strcmp(command, "i2c") == 0)
    {
        return "i2c scan [start] [end] | ping <addr> | read <addr> <reg> [len] | write <addr> <reg> <hex> | send <addr> <hex> | recv <addr> <len>\n";
    }
    if (strcmp(command, "info") == 0)
    {
        return "info\n";
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

static CliStatus handle_reboot(char **tokens,
                               size_t token_count,
                               const CliServices *services,
                               const CliIo *io,
                               CliCommandResult *result)
{
    (void)tokens;
    (void)token_count;
    (void)result;

    if (services->system_reboot == NULL)
    {
        return CLI_STATUS_UNSUPPORTED;
    }

    write_line(io, "reboot ok\n");
    services->system_reboot(services->context);
    return CLI_STATUS_OK;
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

        char line[CONFIG_LINE_BUF_SIZE];
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

            char line[LOG_LINE_BUF_SIZE];
            if (stats.measured_frame_count > 0)
            {
                uint32_t avg_us = stats.frame_time_total_us / stats.measured_frame_count;
                (void)snprintf(line, sizeof(line),
                               "log timing mn=%lu mx=%lu avg=%lu meas=%lu over=%lu drop=%lu bs=%u q=%u\n",
                               (unsigned long)stats.frame_time_min_us,
                               (unsigned long)stats.frame_time_max_us,
                               (unsigned long)avg_us, (unsigned long)stats.measured_frame_count,
                               (unsigned long)stats.overrun_count,
                               (unsigned long)stats.stream_drop_count,
                               stats.stream_batch_size,
                               stats.stream_queue_count);
            }
            else
            {
                (void)snprintf(line, sizeof(line),
                               "log timing drop=%lu bs=%u q=%u\n",
                               (unsigned long)stats.stream_drop_count,
                               stats.stream_batch_size,
                               stats.stream_queue_count);
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

        char line[LOG_LINE_BUF_SIZE];
        (void)snprintf(line, sizeof(line),
                       "log stats enabled=%u stream=%u level=%u frames=%lu failures=%lu"
                       " stepfail=%lu streak=%lu\n",
                       stats.enabled ? 1U : 0U, stats.stream_enabled ? 1U : 0U, stats.level,
                       (unsigned long)stats.frame_count,
                       (unsigned long)stats.failure_count,
                       (unsigned long)stats.step_failure_count,
                       (unsigned long)stats.step_failure_streak);
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
            if (!cli_parse_u32(tokens[idx + 1], &batch) || batch == 0 || batch > LOG_STREAM_BATCH_MAX)
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

static CliStatus handle_test_mode(char **tokens,
                                  size_t token_count,
                                  bool (*set_mode)(bool, void *),
                                  bool (*set_vector)(uint8_t, void *),
                                  bool (*set_freq)(uint16_t, void *),
                                  bool (*set_amp)(uint16_t, void *),
                                  bool (*get_status)(CliTestModeStatus *, void *),
                                  void *context,
                                  const CliIo *io)
{
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

        if (get_status == NULL)
        {
            return CLI_STATUS_UNSUPPORTED;
        }

        CliTestModeStatus status = {0};
        if (!get_status(&status, context))
        {
            return CLI_STATUS_SERVICE_ERROR;
        }

        char line[TEST_STATUS_LINE_BUF_SIZE];
        snprintf(line, sizeof(line),
                 "mode=%u vector=%s freq_hz=%u amp=%u\n",
                 status.enabled ? 1U : 0U, test_vector_name(status.vector), status.frequency_hz,
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
        if (set_mode == NULL)
        {
            return CLI_STATUS_UNSUPPORTED;
        }

        bool enabled = false;
        if (!cli_parse_bool01(tokens[2], &enabled) || !set_mode(enabled, context))
        {
            return CLI_STATUS_SERVICE_ERROR;
        }

        return CLI_STATUS_OK;
    }

    if (strcmp(tokens[1], "vector") == 0)
    {
        if (set_vector == NULL)
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
        else if (strcmp(tokens[2], "sweep") == 0)
        {
            vector = 2;
        }
        else
        {
            return CLI_STATUS_PARSE_ERROR;
        }

        if (!set_vector(vector, context))
        {
            return CLI_STATUS_SERVICE_ERROR;
        }

        return CLI_STATUS_OK;
    }

    if (strcmp(tokens[1], "freq") == 0)
    {
        if (set_freq == NULL)
        {
            return CLI_STATUS_UNSUPPORTED;
        }

        uint32_t frequency_hz = 0;
        if (!cli_parse_u32(tokens[2], &frequency_hz) || frequency_hz > UINT16_MAX ||
            !set_freq((uint16_t)frequency_hz, context))
        {
            return CLI_STATUS_SERVICE_ERROR;
        }

        return CLI_STATUS_OK;
    }

    if (strcmp(tokens[1], "amp") == 0)
    {
        if (set_amp == NULL)
        {
            return CLI_STATUS_UNSUPPORTED;
        }

        uint32_t amplitude = 0;
        if (!cli_parse_u32(tokens[2], &amplitude) || amplitude > UINT16_MAX ||
            !set_amp((uint16_t)amplitude, context))
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

    if (strcmp(tokens[1], "input") == 0)
    {
        return handle_test_mode(tokens + 1, token_count - 1,
                                services->test_set_input_mode,
                                services->test_set_input_vector,
                                services->test_set_input_frequency_hz,
                                services->test_set_input_amplitude,
                                services->test_get_input_status,
                                services->context, io);
    }

    if (strcmp(tokens[1], "output") == 0)
    {
        return handle_test_mode(tokens + 1, token_count - 1,
                                services->test_set_output_mode,
                                services->test_set_output_vector,
                                services->test_set_output_frequency_hz,
                                services->test_set_output_amplitude,
                                services->test_get_output_status,
                                services->context, io);
    }

    if (strcmp(tokens[1], "status") == 0)
    {
        if (token_count != 2)
        {
            return CLI_STATUS_INVALID_ARGUMENTS;
        }

        write_line(io, "test input ");
        CliStatus input_status = handle_test_mode(tokens, token_count,
                                                  services->test_set_input_mode,
                                                  services->test_set_input_vector,
                                                  services->test_set_input_frequency_hz,
                                                  services->test_set_input_amplitude,
                                                  services->test_get_input_status,
                                                  services->context, io);
        if (input_status != CLI_STATUS_OK)
        {
            return input_status;
        }

        write_line(io, "test output ");
        return handle_test_mode(tokens, token_count,
                                services->test_set_output_mode,
                                services->test_set_output_vector,
                                services->test_set_output_frequency_hz,
                                services->test_set_output_amplitude,
                                services->test_get_output_status,
                                services->context, io);
    }

    return handle_test_mode(tokens, token_count,
                            services->test_set_input_mode,
                            services->test_set_input_vector,
                            services->test_set_input_frequency_hz,
                            services->test_set_input_amplitude,
                            services->test_get_input_status,
                            services->context, io);
}

static CliStatus handle_i2c(char **tokens,
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

    if (strcmp(tokens[1], "scan") == 0)
    {
        if (services->i2c_scan == NULL)
        {
            return CLI_STATUS_UNSUPPORTED;
        }

        uint32_t start_addr = 0x03;
        uint32_t end_addr = 0x77;
        if (token_count >= 3)
        {
            if (!cli_parse_u32(tokens[2], &start_addr))
            {
                return CLI_STATUS_PARSE_ERROR;
            }
        }
        if (token_count >= 4)
        {
            if (!cli_parse_u32(tokens[3], &end_addr))
            {
                return CLI_STATUS_PARSE_ERROR;
            }
        }
        if (start_addr > end_addr || start_addr > 0x7F || end_addr > 0x7F)
        {
            return CLI_STATUS_INVALID_ARGUMENTS;
        }

        uint8_t found[128] = {0};
        size_t count = 0;
        if (!services->i2c_scan((uint8_t)start_addr, (uint8_t)end_addr,
                                found, &count, sizeof(found), services->context))
        {
            return CLI_STATUS_SERVICE_ERROR;
        }

        char line[256];
        size_t used = (size_t)snprintf(line, sizeof(line), "i2c scan");
        for (size_t i = 0; i < count && used + 6 < sizeof(line); i++)
        {
            used += (size_t)snprintf(&line[used], sizeof(line) - used, " 0x%02X", found[i]);
        }
        snprintf(&line[used], sizeof(line) - used, "\n");
        return write_line(io, line) ? CLI_STATUS_OK : CLI_STATUS_SERVICE_ERROR;
    }

    if (strcmp(tokens[1], "ping") == 0)
    {
        if (token_count != 3)
        {
            return CLI_STATUS_INVALID_ARGUMENTS;
        }
        if (services->i2c_ping == NULL)
        {
            return CLI_STATUS_UNSUPPORTED;
        }

        uint32_t addr = 0;
        if (!cli_parse_u32(tokens[2], &addr) || addr > 0x7F)
        {
            return CLI_STATUS_PARSE_ERROR;
        }

        if (!services->i2c_ping((uint8_t)addr, services->context))
        {
            return CLI_STATUS_SERVICE_ERROR;
        }

        return write_line(io, "i2c ping ok\n") ? CLI_STATUS_OK : CLI_STATUS_SERVICE_ERROR;
    }

    if (strcmp(tokens[1], "read") == 0)
    {
        if (token_count < 4 || token_count > 5)
        {
            return CLI_STATUS_INVALID_ARGUMENTS;
        }
        if (services->i2c_transfer == NULL)
        {
            return CLI_STATUS_UNSUPPORTED;
        }

        uint32_t addr = 0, reg = 0;
        if (!cli_parse_u32(tokens[2], &addr) || addr > 0x7F ||
            !cli_parse_u32(tokens[3], &reg) || reg > 0xFF)
        {
            return CLI_STATUS_PARSE_ERROR;
        }

        uint32_t len = 1;
        if (token_count >= 5)
        {
            if (!cli_parse_u32(tokens[4], &len) || len == 0 || len > 64)
            {
                return CLI_STATUS_PARSE_ERROR;
            }
        }

        uint8_t tx_buf[1] = {(uint8_t)reg};
        uint8_t rx_buf[64] = {0};
        size_t rx_len = (size_t)len;
        if (!services->i2c_transfer((uint8_t)addr, tx_buf, sizeof(tx_buf),
                                    rx_buf, &rx_len, 100, services->context))
        {
            return CLI_STATUS_SERVICE_ERROR;
        }

        char line[256];
        size_t used = (size_t)snprintf(line, sizeof(line), "i2c data");
        for (size_t i = 0; i < rx_len && used + 4 < sizeof(line); i++)
        {
            used += (size_t)snprintf(&line[used], sizeof(line) - used, " %02X", rx_buf[i]);
        }
        snprintf(&line[used], sizeof(line) - used, "\n");
        return write_line(io, line) ? CLI_STATUS_OK : CLI_STATUS_SERVICE_ERROR;
    }

    if (strcmp(tokens[1], "write") == 0)
    {
        if (token_count != 5)
        {
            return CLI_STATUS_INVALID_ARGUMENTS;
        }
        if (services->i2c_transfer == NULL)
        {
            return CLI_STATUS_UNSUPPORTED;
        }

        uint32_t addr = 0, reg = 0;
        if (!cli_parse_u32(tokens[2], &addr) || addr > 0x7F ||
            !cli_parse_u32(tokens[3], &reg) || reg > 0xFF)
        {
            return CLI_STATUS_PARSE_ERROR;
        }

        uint8_t data[64] = {0};
        size_t data_count = 0;
        if (!cli_parse_hex_bytes(tokens[4], data, sizeof(data), &data_count))
        {
            return CLI_STATUS_PARSE_ERROR;
        }

        uint8_t tx_buf[65] = {(uint8_t)reg};
        memcpy(tx_buf + 1, data, data_count);
        size_t tx_len = 1 + data_count;
        size_t rx_len = 0;
        if (!services->i2c_transfer((uint8_t)addr, tx_buf, tx_len,
                                    NULL, &rx_len, 100, services->context))
        {
            return CLI_STATUS_SERVICE_ERROR;
        }
        return CLI_STATUS_OK;
    }

    if (strcmp(tokens[1], "send") == 0)
    {
        if (token_count != 4)
        {
            return CLI_STATUS_INVALID_ARGUMENTS;
        }
        if (services->i2c_transfer == NULL)
        {
            return CLI_STATUS_UNSUPPORTED;
        }

        uint32_t addr = 0;
        if (!cli_parse_u32(tokens[2], &addr) || addr > 0x7F)
        {
            return CLI_STATUS_PARSE_ERROR;
        }

        uint8_t tx_buf[64] = {0};
        size_t tx_count = 0;
        if (!cli_parse_hex_bytes(tokens[3], tx_buf, sizeof(tx_buf), &tx_count))
        {
            return CLI_STATUS_PARSE_ERROR;
        }

        size_t rx_len = 0;
        if (!services->i2c_transfer((uint8_t)addr, tx_buf, tx_count,
                                    NULL, &rx_len, 100, services->context))
        {
            return CLI_STATUS_SERVICE_ERROR;
        }
        return CLI_STATUS_OK;
    }

    if (strcmp(tokens[1], "recv") == 0)
    {
        if (token_count != 4)
        {
            return CLI_STATUS_INVALID_ARGUMENTS;
        }
        if (services->i2c_transfer == NULL)
        {
            return CLI_STATUS_UNSUPPORTED;
        }

        uint32_t addr = 0, len = 0;
        if (!cli_parse_u32(tokens[2], &addr) || addr > 0x7F ||
            !cli_parse_u32(tokens[3], &len) || len == 0 || len > 64)
        {
            return CLI_STATUS_PARSE_ERROR;
        }

        uint8_t rx_buf[64] = {0};
        size_t rx_len = (size_t)len;
        if (!services->i2c_transfer((uint8_t)addr, NULL, 0,
                                    rx_buf, &rx_len, 100, services->context))
        {
            return CLI_STATUS_SERVICE_ERROR;
        }

        char line[256];
        size_t used = (size_t)snprintf(line, sizeof(line), "i2c data");
        for (size_t i = 0; i < rx_len && used + 4 < sizeof(line); i++)
        {
            used += (size_t)snprintf(&line[used], sizeof(line) - used, " %02X", rx_buf[i]);
        }
        snprintf(&line[used], sizeof(line) - used, "\n");
        return write_line(io, line) ? CLI_STATUS_OK : CLI_STATUS_SERVICE_ERROR;
    }

    return CLI_STATUS_UNKNOWN_COMMAND;
}

static CliStatus handle_info(char **tokens,
                             size_t token_count,
                             const CliServices *services,
                             const CliIo *io,
                             CliCommandResult *result)
{
    (void)result;

    if (token_count != 1)
    {
        return CLI_STATUS_INVALID_ARGUMENTS;
    }

    if (services->audio_get_info == NULL)
    {
        return CLI_STATUS_UNSUPPORTED;
    }

    CliAudioStatus status = {0};
    if (!services->audio_get_info(&status, services->context))
    {
        return CLI_STATUS_SERVICE_ERROR;
    }

    char line[INFO_LINE_BUF_SIZE];
    (void)snprintf(line, sizeof(line),
                   "info sr=%lu sf=%lu sp=%lu ds=%lu sl=%lu\n",
                   (unsigned long)status.sample_rate_hz,
                   (unsigned long)status.sample_buf_len,
                   (unsigned long)status.sampling_period_us,
                   (unsigned long)status.delay_samples_len,
                   (unsigned long)status.processing_slack_ms);
    return write_line(io, line) ? CLI_STATUS_OK : CLI_STATUS_SERVICE_ERROR;
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
        {.name = "i2c", .handler = handle_i2c},
        {.name = "reboot", .handler = handle_reboot},
        {.name = "info", .handler = handle_info},
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