#ifndef CLI_CORE_H
#define CLI_CORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CLI_MAX_LINE_LENGTH 160
#define CLI_MAX_ROM_BYTES 64

typedef enum
{
    CLI_STATUS_OK = 0,
    CLI_STATUS_UNKNOWN_COMMAND = 1,
    CLI_STATUS_INVALID_ARGUMENTS = 2,
    CLI_STATUS_PARSE_ERROR = 3,
    CLI_STATUS_UNSUPPORTED = 4,
    CLI_STATUS_SERVICE_ERROR = 5,
} CliStatus;

typedef struct
{
    bool (*write)(const char *text, void *context);
    void *context;
} CliIo;

typedef struct
{
    bool (*set_pot_override)(uint8_t potIndex, uint32_t value, void *context);
    bool (*clear_pot_override)(uint8_t potIndex, void *context);
    bool (*set_switch_override)(uint8_t switchIndex, bool enabled, void *context);
    bool (*clear_switch_override)(uint8_t switchIndex, void *context);
    bool (*clear_all_overrides)(void *context);

    bool (*config_set)(const char *key, int32_t value, void *context);
    bool (*config_get)(const char *key, int32_t *valueOut, void *context);

    bool (*rom_save_state)(void *context);
    bool (*rom_load_state)(void *context);
    bool (*rom_read_raw)(uint16_t address, uint16_t length, uint8_t *payloadOut,
                         size_t payloadCapacity, size_t *payloadSizeOut, void *context);
    bool (*rom_write_raw)(uint16_t address, const uint8_t *payload, size_t payloadSize,
                          void *context);

    bool (*log_set_level)(uint8_t level, void *context);
    bool (*log_set_enabled)(bool enabled, void *context);
    void *context;
} CliServices;

const char *cli_core_error_text(CliStatus status);
CliStatus cli_core_process_line(const char *line, const CliServices *services, const CliIo *io);

#endif