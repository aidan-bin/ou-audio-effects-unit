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

typedef enum
{
    CLI_COMMAND_ACTION_NONE = 0,
    CLI_COMMAND_ACTION_ENTER_LOG_STREAM = 1,
} CliCommandAction;

typedef struct
{
    CliCommandAction action;
} CliCommandResult;

typedef struct
{
    bool (*write)(const char *text, void *context);
    void *context;
} CliIo;

typedef struct
{
    bool enabled;
    uint8_t vector;
    uint16_t frequencyHz;
    uint16_t amplitude;
} CliTestModeStatus;

typedef struct
{
    bool enabled;
    bool streamEnabled;
    uint8_t level;
    uint32_t frameCount;
    uint32_t failureCount;
    uint32_t stepFailureCount;
    uint32_t stepFailureStreak;
    uint32_t frameTimeMinUs;
    uint32_t frameTimeMaxUs;
    uint32_t frameTimeTotalUs;
    uint32_t measuredFrameCount;
    uint32_t overrunCount;
    uint32_t streamDropCount;
    uint8_t streamBatchSize;
    uint8_t streamQueueCount;
} CliLogStats;

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
    bool (*log_set_stream)(bool enabled, void *context);
    bool (*log_set_stream_batch)(uint8_t batchSize, void *context);
    bool (*log_read_line)(char *lineOut,
                          size_t lineCapacity,
                          bool *hasLineOut,
                          void *context);
    bool (*log_get_stats)(CliLogStats *statsOut, void *context);
    bool (*log_reset_stats)(void *context);

    bool (*test_set_input_mode)(bool enabled, void *context);
    bool (*test_set_input_vector)(uint8_t vector, void *context);
    bool (*test_set_input_frequency_hz)(uint16_t frequencyHz, void *context);
    bool (*test_set_input_amplitude)(uint16_t amplitude, void *context);
    bool (*test_get_input_status)(CliTestModeStatus *statusOut, void *context);

    bool (*test_set_output_mode)(bool enabled, void *context);
    bool (*test_set_output_vector)(uint8_t vector, void *context);
    bool (*test_set_output_frequency_hz)(uint16_t frequencyHz, void *context);
    bool (*test_set_output_amplitude)(uint16_t amplitude, void *context);
    bool (*test_get_output_status)(CliTestModeStatus *statusOut, void *context);
    void *context;
} CliServices;

const char *cli_core_error_text(CliStatus status);
bool cli_core_stop_log_stream(const CliServices *services);
CliStatus cli_core_process_line_ex(const char *line,
                                   const CliServices *services,
                                   const CliIo *io,
                                   CliCommandResult *result_out);
CliStatus cli_core_process_line(const char *line, const CliServices *services, const CliIo *io);

#endif