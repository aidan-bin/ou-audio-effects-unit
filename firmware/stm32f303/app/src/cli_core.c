#include "cli_core.h"
#include "cli_parse.h"

#include <stdio.h>
#include <string.h>

#include "sysinfo.h"

static inline size_t cli_strnlen(const char *s, size_t maxlen)
{
    size_t n = 0;
    while (n < maxlen && s[n] != '\0')
    {
        n++;
    }
    return n;
}

#define CONFIG_LINE_BUF_SIZE 48
#define SLOT_LINE_BUF_SIZE 128
#define CLI_SLOT_EMPTY 0xFFu
#define LOG_LINE_BUF_SIZE 128
#define LOG_STREAM_BATCH_MAX 255
#define TEST_STATUS_LINE_BUF_SIZE 112
#define INFO_LINE_BUF_SIZE 96

// Bail if unsupported service is requested
#define REQUIRE_SERVICE(callback)          \
    do                                     \
    {                                      \
        if ((callback) == NULL)            \
        {                                  \
            return CLI_STATUS_UNSUPPORTED; \
        }                                  \
    } while (0)

typedef CliStatus (*CliCommandHandler)(char **tokens,
                                       size_t token_count,
                                       const CliServices *services,
                                       const CliIo *io,
                                       CliCommandResult *result);

typedef struct
{
    const char *name;
    CliCommandHandler handler;
    const char *usage;
} CliCommandDefinition;

static const CliCommandDefinition *cli_commands(size_t *count);

static bool write_line(const CliIo *io, const char *line)
{
    return io != NULL && io->write != NULL && io->write(line, io->context);
}

static CliStatus write_byte_list(const CliIo *io, const char *prefix, const char *byte_format,
                                 size_t reserve, const uint8_t *data, size_t count)
{
    char line[256];
    size_t used = (size_t)snprintf(line, sizeof(line), "%s", prefix);
    for (size_t i = 0; i < count && used + reserve < sizeof(line); i++)
    {
        used += (size_t)snprintf(&line[used], sizeof(line) - used, byte_format, data[i]);
    }
    snprintf(&line[used], sizeof(line) - used, "\n");
    return write_line(io, line) ? CLI_STATUS_OK : CLI_STATUS_SERVICE_ERROR;
}

static bool parse_u32_max(const char *token, uint32_t max, uint32_t *out)
{
    return cli_parse_u32(token, out) && *out <= max;
}

static const struct
{
    const char *name;
    uint8_t id;
} test_vectors[] = {
    {"sine", 0U},
    {"lut", 1U},
    {"sweep", 2U},
    {"wav", 3U},
    {"impulse", 4U},
    {"usb", 5U},
};

static const char *test_vector_name(uint8_t vector)
{
    for (size_t i = 0; i < sizeof(test_vectors) / sizeof(test_vectors[0]); i++)
    {
        if (test_vectors[i].id == vector)
        {
            return test_vectors[i].name;
        }
    }
    return "sine";
}

static bool test_vector_id(const char *name, uint8_t *vector_out)
{
    for (size_t i = 0; i < sizeof(test_vectors) / sizeof(test_vectors[0]); i++)
    {
        if (strcmp(test_vectors[i].name, name) == 0)
        {
            *vector_out = test_vectors[i].id;
            return true;
        }
    }
    return false;
}

// EffectType name <-> id table
static const struct
{
    const char *name;
    uint8_t id;
} effect_names[] = {
    {"overdrive", 0U},
    {"echo", 1U},
    {"compression", 2U},
};

static const char *effect_name(uint8_t id)
{
    for (size_t i = 0; i < sizeof(effect_names) / sizeof(effect_names[0]); i++)
    {
        if (effect_names[i].id == id)
        {
            return effect_names[i].name;
        }
    }
    return "?";
}

static bool effect_id(const char *name, uint8_t *id_out)
{
    for (size_t i = 0; i < sizeof(effect_names) / sizeof(effect_names[0]); i++)
    {
        if (strcmp(effect_names[i].name, name) == 0)
        {
            *id_out = effect_names[i].id;
            return true;
        }
    }
    return false;
}

static const char *command_help_text(const char *command)
{
    size_t count = 0;
    const CliCommandDefinition *commands = cli_commands(&count);

    if (command == NULL)
    {
        return NULL;
    }

    for (size_t i = 0; i < count; i++)
    {
        if (strcmp(commands[i].name, command) == 0)
        {
            return commands[i].usage;
        }
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

static CliStatus handle_crash(char **tokens,
                              size_t token_count,
                              const CliServices *services,
                              const CliIo *io,
                              CliCommandResult *result)
{
    (void)services;
    (void)io;
    (void)result;

    if (token_count != 2)
    {
        return CLI_STATUS_INVALID_ARGUMENTS;
    }

    if (strcmp(tokens[1], "null") == 0)
    {
        volatile uint32_t *volatile p = (volatile uint32_t *)(uintptr_t)0;
        *p = 0xDEADBEEF;
    }
    else if (strcmp(tokens[1], "udf") == 0)
    {
#ifdef __arm__
        __asm volatile("udf #0");
#endif
    }
    else if (strcmp(tokens[1], "div0") == 0)
    {
        volatile int x = 1;
        volatile int y = 0;
        volatile int z = x / y;
        (void)z;
    }
    else
    {
        return CLI_STATUS_INVALID_ARGUMENTS;
    }

    return CLI_STATUS_OK;
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

    REQUIRE_SERVICE(services->system_reboot);

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

    if (token_count == 2)
    {
        const char *help = command_help_text(tokens[1]);
        if (help == NULL)
        {
            return CLI_STATUS_UNKNOWN_COMMAND;
        }
        return write_line(io, help) ? CLI_STATUS_OK : CLI_STATUS_SERVICE_ERROR;
    }

    // No argument: list every command name from the single command table.
    size_t count = 0;
    const CliCommandDefinition *commands = cli_commands(&count);

    char summary[160];
    size_t len = 0;
    int written = snprintf(summary, sizeof(summary), "commands:");
    len = (written > 0) ? (size_t)written : 0;
    for (size_t i = 0; i < count && len < sizeof(summary); i++)
    {
        written = snprintf(summary + len, sizeof(summary) - len, " %s", commands[i].name);
        if (written > 0)
        {
            len += (size_t)written;
        }
    }
    if (len < sizeof(summary))
    {
        (void)snprintf(summary + len, sizeof(summary) - len, "\n");
    }

    return write_line(io, summary) ? CLI_STATUS_OK : CLI_STATUS_SERVICE_ERROR;
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
        REQUIRE_SERVICE(services->clear_all_overrides);

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
            REQUIRE_SERVICE(services->clear_pot_override);

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
            REQUIRE_SERVICE(services->clear_switch_override);

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

        REQUIRE_SERVICE(services->config_set);

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

        REQUIRE_SERVICE(services->config_get);

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

static CliStatus slot_print(const char *prefix, const CliServices *services, const CliIo *io)
{
    uint8_t slots[CLI_PARSE_MAX_TOKENS] = {0};
    uint8_t enabled[CLI_PARSE_MAX_TOKENS] = {0};
    size_t count = 0;
    if (!services->slot_get(slots, enabled, sizeof(slots), &count, services->context))
    {
        return CLI_STATUS_SERVICE_ERROR;
    }

    char line[SLOT_LINE_BUF_SIZE];
    size_t used = (size_t)snprintf(line, sizeof(line), "%s", prefix);
    for (size_t i = 0; i < count && used + 24U < sizeof(line); i++)
    {
        if (slots[i] == CLI_SLOT_EMPTY)
        {
            used += (size_t)snprintf(&line[used], sizeof(line) - used, " %u=empty", (unsigned)i);
        }
        else
        {
            used += (size_t)snprintf(&line[used], sizeof(line) - used, " %u=%s(%s)", (unsigned)i,
                                     effect_name(slots[i]), enabled[i] ? "on" : "off");
        }
    }
    snprintf(&line[used], sizeof(line) - used, "\n");
    return write_line(io, line) ? CLI_STATUS_OK : CLI_STATUS_SERVICE_ERROR;
}

static CliStatus handle_slot(char **tokens,
                             size_t token_count,
                             const CliServices *services,
                             const CliIo *io,
                             CliCommandResult *result)
{
    (void)result;

    if (token_count == 1 || (token_count == 2 && strcmp(tokens[1], "get") == 0))
    {
        REQUIRE_SERVICE(services->slot_get);
        return slot_print("slots", services, io);
    }

    if (strcmp(tokens[1], "set") == 0)
    {
        if (token_count != 4)
        {
            return CLI_STATUS_INVALID_ARGUMENTS;
        }
        REQUIRE_SERVICE(services->slot_assign);

        uint32_t pos = 0;
        uint8_t effect = 0;
        if (!parse_u32_max(tokens[2], UINT8_MAX - 1U, &pos) || !effect_id(tokens[3], &effect))
        {
            return CLI_STATUS_PARSE_ERROR;
        }
        if (!services->slot_assign((uint8_t)pos, effect, services->context))
        {
            return CLI_STATUS_SERVICE_ERROR;
        }
        return CLI_STATUS_OK;
    }

    if (strcmp(tokens[1], "clear") == 0)
    {
        if (token_count != 3)
        {
            return CLI_STATUS_INVALID_ARGUMENTS;
        }
        REQUIRE_SERVICE(services->slot_clear);

        uint32_t pos = 0;
        if (!parse_u32_max(tokens[2], UINT8_MAX - 1U, &pos))
        {
            return CLI_STATUS_PARSE_ERROR;
        }
        if (!services->slot_clear((uint8_t)pos, services->context))
        {
            return CLI_STATUS_SERVICE_ERROR;
        }
        return CLI_STATUS_OK;
    }

    if (strcmp(tokens[1], "swap") == 0)
    {
        if (token_count != 4)
        {
            return CLI_STATUS_INVALID_ARGUMENTS;
        }
        REQUIRE_SERVICE(services->slot_swap);

        uint32_t a = 0;
        uint32_t b = 0;
        if (!parse_u32_max(tokens[2], UINT8_MAX - 1U, &a) ||
            !parse_u32_max(tokens[3], UINT8_MAX - 1U, &b))
        {
            return CLI_STATUS_PARSE_ERROR;
        }
        if (!services->slot_swap((uint8_t)a, (uint8_t)b, services->context))
        {
            return CLI_STATUS_SERVICE_ERROR;
        }
        return CLI_STATUS_OK;
    }

    if (strcmp(tokens[1], "enable") == 0)
    {
        if (token_count != 4)
        {
            return CLI_STATUS_INVALID_ARGUMENTS;
        }
        REQUIRE_SERVICE(services->slot_set_enabled);

        uint32_t pos = 0;
        bool enabled = false;
        if (!parse_u32_max(tokens[2], UINT8_MAX - 1U, &pos) || !cli_parse_bool01(tokens[3], &enabled))
        {
            return CLI_STATUS_PARSE_ERROR;
        }
        if (!services->slot_set_enabled((uint8_t)pos, enabled, services->context))
        {
            return CLI_STATUS_SERVICE_ERROR;
        }
        return CLI_STATUS_OK;
    }

    if (strcmp(tokens[1], "active") == 0)
    {
        if (token_count == 2)
        {
            REQUIRE_SERVICE(services->slot_get_active);
            REQUIRE_SERVICE(services->slot_get);

            uint8_t pos = 0;
            if (!services->slot_get_active(&pos, services->context))
            {
                return CLI_STATUS_SERVICE_ERROR;
            }

            uint8_t slots[CLI_PARSE_MAX_TOKENS] = {0};
            uint8_t enabled[CLI_PARSE_MAX_TOKENS] = {0};
            size_t count = 0;
            if (!services->slot_get(slots, enabled, sizeof(slots), &count, services->context))
            {
                return CLI_STATUS_SERVICE_ERROR;
            }

            const char *name = (pos < count && slots[pos] != CLI_SLOT_EMPTY)
                                   ? effect_name(slots[pos])
                                   : "empty";
            char line[SLOT_LINE_BUF_SIZE];
            (void)snprintf(line, sizeof(line), "active slot %u %s\n", (unsigned)pos, name);
            return write_line(io, line) ? CLI_STATUS_OK : CLI_STATUS_SERVICE_ERROR;
        }

        if (token_count == 3)
        {
            REQUIRE_SERVICE(services->slot_set_active);

            uint32_t pos = 0;
            if (!parse_u32_max(tokens[2], UINT8_MAX - 1U, &pos))
            {
                return CLI_STATUS_PARSE_ERROR;
            }
            if (!services->slot_set_active((uint8_t)pos, services->context))
            {
                return CLI_STATUS_SERVICE_ERROR;
            }
            return CLI_STATUS_OK;
        }

        return CLI_STATUS_INVALID_ARGUMENTS;
    }

    return CLI_STATUS_UNKNOWN_COMMAND;
}

static CliStatus handle_effects(char **tokens,
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
    REQUIRE_SERVICE(services->slot_get);

    uint8_t slots[CLI_PARSE_MAX_TOKENS] = {0};
    uint8_t enabled[CLI_PARSE_MAX_TOKENS] = {0};
    size_t count = 0;
    if (!services->slot_get(slots, enabled, sizeof(slots), &count, services->context))
    {
        return CLI_STATUS_SERVICE_ERROR;
    }

    char line[SLOT_LINE_BUF_SIZE];
    size_t used = (size_t)snprintf(line, sizeof(line), "effects");
    for (size_t e = 0; e < sizeof(effect_names) / sizeof(effect_names[0]) &&
                       used + 24U < sizeof(line);
         e++)
    {
        size_t pos = count;
        for (size_t i = 0; i < count; i++)
        {
            if (slots[i] == effect_names[e].id)
            {
                pos = i;
                break;
            }
        }
        if (pos < count)
        {
            used += (size_t)snprintf(&line[used], sizeof(line) - used, " %s@%u",
                                     effect_names[e].name, (unsigned)pos);
        }
        else
        {
            used += (size_t)snprintf(&line[used], sizeof(line) - used, " %s(unassigned)",
                                     effect_names[e].name);
        }
    }
    snprintf(&line[used], sizeof(line) - used, "\n");
    return write_line(io, line) ? CLI_STATUS_OK : CLI_STATUS_SERVICE_ERROR;
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
        REQUIRE_SERVICE(services->rom_save_state);

        if (!services->rom_save_state(services->context))
        {
            return CLI_STATUS_SERVICE_ERROR;
        }
        return CLI_STATUS_OK;
    }

    if (strcmp(tokens[1], "load-state") == 0)
    {
        REQUIRE_SERVICE(services->rom_load_state);

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

        REQUIRE_SERVICE(services->rom_read_raw);

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

        return write_byte_list(io, "rom data ", "%02X", 2, bytes, bytes_count);
    }

    if (strcmp(tokens[1], "write") == 0)
    {
        if (token_count != 4)
        {
            return CLI_STATUS_INVALID_ARGUMENTS;
        }

        REQUIRE_SERVICE(services->rom_write_raw);

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
            REQUIRE_SERVICE(services->log_reset_stats);
            if (!services->log_reset_stats(services->context))
            {
                return CLI_STATUS_SERVICE_ERROR;
            }
            return CLI_STATUS_OK;
        }

        if (token_count == 3 && strcmp(tokens[2], "timing") == 0)
        {
            REQUIRE_SERVICE(services->log_get_stats);

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

        REQUIRE_SERVICE(services->log_get_stats);

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
        REQUIRE_SERVICE(services->log_set_stream);

        size_t idx = 2;
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
            REQUIRE_SERVICE(services->log_set_stream_batch);
            if (!services->log_set_stream_batch((uint8_t)batch, services->context))
            {
                return CLI_STATUS_SERVICE_ERROR;
            }
        }

        if (!services->log_set_stream(true, services->context))
        {
            return CLI_STATUS_SERVICE_ERROR;
        }

        if (!write_line(io, "log stream active (press q to stop)\n"))
        {
            return CLI_STATUS_SERVICE_ERROR;
        }

        if (result != NULL)
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
        REQUIRE_SERVICE(services->log_set_level);

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
        REQUIRE_SERVICE(services->log_set_enabled);

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

        REQUIRE_SERVICE(get_status);

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
        REQUIRE_SERVICE(set_mode);

        bool enabled = false;
        if (!cli_parse_bool01(tokens[2], &enabled) || !set_mode(enabled, context))
        {
            return CLI_STATUS_SERVICE_ERROR;
        }

        return CLI_STATUS_OK;
    }

    if (strcmp(tokens[1], "vector") == 0)
    {
        REQUIRE_SERVICE(set_vector);

        uint8_t vector = 0;
        if (!test_vector_id(tokens[2], &vector))
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
        REQUIRE_SERVICE(set_freq);

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
        REQUIRE_SERVICE(set_amp);

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

static CliStatus handle_audio(char **tokens,
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
        REQUIRE_SERVICE(services->audio_get_status);
        uint8_t source = 0;
        bool output_enabled = false;
        uint32_t out_dropped = 0;
        uint32_t in_dropped = 0;
        if (!services->audio_get_status(&source, &output_enabled,
                                        &out_dropped, &in_dropped,
                                        services->context))
        {
            return CLI_STATUS_SERVICE_ERROR;
        }
        char line[80];
        (void)snprintf(line, sizeof(line),
                       "audio input=%s output=%s drops_out=%lu drops_in=%lu\n",
                       source == 0U ? "adc" : "usb",
                       output_enabled ? "on" : "off",
                       (unsigned long)out_dropped,
                       (unsigned long)in_dropped);
        return write_line(io, line) ? CLI_STATUS_OK
                                    : CLI_STATUS_SERVICE_ERROR;
    }

    if (strcmp(tokens[1], "input") == 0)
    {
        if (token_count != 3)
        {
            return CLI_STATUS_INVALID_ARGUMENTS;
        }
        REQUIRE_SERVICE(services->audio_set_input);
        uint8_t source = 0xFFU;
        if (strcmp(tokens[2], "adc") == 0)
        {
            source = 0U;
        }
        else if (strcmp(tokens[2], "usb") == 0)
        {
            source = 1U;
        }
        else
        {
            return CLI_STATUS_PARSE_ERROR;
        }
        if (!services->audio_set_input(source, services->context))
        {
            return CLI_STATUS_SERVICE_ERROR;
        }
        return CLI_STATUS_OK;
    }

    if (strcmp(tokens[1], "output") == 0)
    {
        if (token_count != 3)
        {
            return CLI_STATUS_INVALID_ARGUMENTS;
        }
        REQUIRE_SERVICE(services->audio_set_output);
        bool enabled = false;
        if (!cli_parse_bool01(tokens[2], &enabled))
        {
            return CLI_STATUS_PARSE_ERROR;
        }
        if (!services->audio_set_output(enabled, services->context))
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
        REQUIRE_SERVICE(services->i2c_scan);

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

        return write_byte_list(io, "i2c scan", " 0x%02X", 6, found, count);
    }

    if (strcmp(tokens[1], "ping") == 0)
    {
        if (token_count != 3)
        {
            return CLI_STATUS_INVALID_ARGUMENTS;
        }
        REQUIRE_SERVICE(services->i2c_ping);

        uint32_t addr = 0;
        if (!parse_u32_max(tokens[2], 0x7F, &addr))
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
        REQUIRE_SERVICE(services->i2c_transfer);

        uint32_t addr = 0, reg = 0;
        if (!parse_u32_max(tokens[2], 0x7F, &addr) || !parse_u32_max(tokens[3], 0xFF, &reg))
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

        return write_byte_list(io, "i2c data", " %02X", 4, rx_buf, rx_len);
    }

    if (strcmp(tokens[1], "write") == 0)
    {
        if (token_count != 5)
        {
            return CLI_STATUS_INVALID_ARGUMENTS;
        }
        REQUIRE_SERVICE(services->i2c_transfer);

        uint32_t addr = 0, reg = 0;
        if (!parse_u32_max(tokens[2], 0x7F, &addr) || !parse_u32_max(tokens[3], 0xFF, &reg))
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
        REQUIRE_SERVICE(services->i2c_transfer);

        uint32_t addr = 0;
        if (!parse_u32_max(tokens[2], 0x7F, &addr))
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
        REQUIRE_SERVICE(services->i2c_transfer);

        uint32_t addr = 0, len = 0;
        if (!parse_u32_max(tokens[2], 0x7F, &addr) ||
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

        return write_byte_list(io, "i2c data", " %02X", 4, rx_buf, rx_len);
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

    REQUIRE_SERVICE(services->audio_get_info);

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

static CliStatus handle_sysinfo(char **tokens,
                                size_t token_count,
                                const CliServices *services,
                                const CliIo *io,
                                CliCommandResult *result)
{
    (void)tokens;
    (void)services;
    (void)result;

    if (token_count != 1)
    {
        return CLI_STATUS_INVALID_ARGUMENTS;
    }

    const SysinfoEntry *entries = sysinfo_get_entries();
    size_t count = sysinfo_get_entry_count();

    for (size_t i = 0; i < count; i++)
    {
        char line[64];
        (void)snprintf(line, sizeof(line), "%s=%s\n",
                       entries[i].key, entries[i].value);
        if (!write_line(io, line))
        {
            break;
        }
    }

    return CLI_STATUS_OK;
}

static const CliCommandDefinition command_table[] = {
    {"help", handle_help, "help [command]\n"},
    {"ping", handle_ping, "ping\n"},
    {"override", handle_override, "override pot|switch set|clear <index> [value] | clear-all\n"},
    {"config", handle_config, "config set|get <key> [value]\n"},
    {"slot", handle_slot, "slot [get] | set <pos> <effect> | clear <pos> | swap <a> <b> | enable <pos> <0|1> | active [<pos>]\n"},
    {"effects", handle_effects, "effects (list catalog + slot assignment)\n"},
    {"rom", handle_rom, "rom save-state|load-state|read <addr> <len>|write <addr> <hex>\n"},
    {"log", handle_log, "log enable <0|1> | level <0-255> | stream [0|1] [batch <N>] (q to stop) | stats [reset|timing]\n"},
    {"audio", handle_audio, "audio input adc|usb | output on|off | status\n"},
    {"test", handle_test, "test mode <0|1> | vector <sine|lut|sweep|wav|impulse|usb> | freq <hz> | amp <value> | status\n"},
    {"i2c", handle_i2c, "i2c scan [start] [end] | ping <addr> | read <addr> <reg> [len] | write <addr> <reg> <hex> | send <addr> <hex> | recv <addr> <len>\n"},
    {"crash", handle_crash, "crash null|udf|div0\n"},
    {"reboot", handle_reboot, "reboot\n"},
    {"info", handle_info, "info\n"},
    {"sysinfo", handle_sysinfo, "sysinfo\n"},
};

static const CliCommandDefinition *cli_commands(size_t *count)
{
    if (count != NULL)
    {
        *count = sizeof(command_table) / sizeof(command_table[0]);
    }
    return command_table;
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

    size_t line_length = cli_strnlen(line, CLI_MAX_LINE_LENGTH + 1);
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

    size_t command_count = 0;
    const CliCommandDefinition *command_table = cli_commands(&command_count);

    CliStatus status = CLI_STATUS_UNKNOWN_COMMAND;
    for (size_t i = 0; i < command_count; i++)
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