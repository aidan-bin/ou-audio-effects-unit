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
    uint16_t frequency_hz;
    uint16_t amplitude;
} CliTestModeStatus;

typedef struct
{
    bool enabled;
    bool stream_enabled;
    uint8_t level;
    uint32_t frame_count;
    uint32_t failure_count;
    uint32_t step_failure_count;
    uint32_t step_failure_streak;
    uint32_t frame_time_min_us;
    uint32_t frame_time_max_us;
    uint32_t frame_time_total_us;
    uint32_t measured_frame_count;
    uint32_t overrun_count;
    uint32_t stream_drop_count;
    uint8_t stream_batch_size;
    uint8_t stream_queue_count;
} CliLogStats;

typedef struct
{
    uint32_t sample_rate_hz;
    uint32_t sample_buf_len;
    uint32_t sampling_period_us;
    uint32_t delay_samples_len;
    uint32_t processing_slack_ms;
} CliAudioStatus;

typedef struct
{
    bool (*set_pot_override)(uint8_t pot_index, uint32_t value, void *context);
    bool (*clear_pot_override)(uint8_t pot_index, void *context);
    bool (*set_switch_override)(uint8_t switch_index, bool enabled, void *context);
    bool (*clear_switch_override)(uint8_t switch_index, void *context);
    bool (*clear_all_overrides)(void *context);

    bool (*config_set)(const char *key, int32_t value, void *context);
    bool (*config_get)(const char *key, int32_t *value_out, void *context);

    bool (*rom_save_state)(void *context);
    bool (*rom_load_state)(void *context);
    bool (*rom_read_raw)(uint16_t address, uint16_t length, uint8_t *payload_out,
                         size_t payload_capacity, size_t *payload_size_out, void *context);
    bool (*rom_write_raw)(uint16_t address, const uint8_t *payload, size_t payload_size,
                          void *context);

    bool (*log_set_level)(uint8_t level, void *context);
    bool (*log_set_enabled)(bool enabled, void *context);
    bool (*log_set_stream)(bool enabled, void *context);
    bool (*log_set_stream_batch)(uint8_t batch_size, void *context);
    bool (*log_read_line)(char *line_out,
                          size_t line_capacity,
                          bool *has_line_out,
                          void *context);
    bool (*log_get_stats)(CliLogStats *stats_out, void *context);
    bool (*log_reset_stats)(void *context);

    bool (*test_set_input_mode)(bool enabled, void *context);
    bool (*test_set_input_vector)(uint8_t vector, void *context);
    bool (*test_set_input_frequency_hz)(uint16_t frequency_hz, void *context);
    bool (*test_set_input_amplitude)(uint16_t amplitude, void *context);
    bool (*test_get_input_status)(CliTestModeStatus *status_out, void *context);

    bool (*test_set_output_mode)(bool enabled, void *context);
    bool (*test_set_output_vector)(uint8_t vector, void *context);
    bool (*test_set_output_frequency_hz)(uint16_t frequency_hz, void *context);
    bool (*test_set_output_amplitude)(uint16_t amplitude, void *context);
    bool (*test_get_output_status)(CliTestModeStatus *status_out, void *context);

    bool (*i2c_ping)(uint8_t address, void *context);
    bool (*i2c_transfer)(uint8_t address, const uint8_t *tx_data, size_t tx_len,
                         uint8_t *rx_data, size_t *rx_len, uint32_t timeout_ms,
                         void *context);
    bool (*i2c_scan)(uint8_t start_addr, uint8_t end_addr,
                     uint8_t *found_addrs, size_t *count,
                     size_t max_count, void *context);

    bool (*system_reboot)(void *context);

    bool (*audio_set_input)(uint8_t source, void *context);
    bool (*audio_set_output)(bool enabled, void *context);
    bool (*audio_get_status)(uint8_t *source_out, bool *output_enabled_out, void *context);

    bool (*audio_get_info)(CliAudioStatus *status_out, void *context);
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