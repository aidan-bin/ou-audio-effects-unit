#ifndef CLI_SERVICE_ADAPTER_H
#define CLI_SERVICE_ADAPTER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "cli_core.h"
#include "effects_model.h"

#define CLI_POT_COUNT 4
#define CLI_SWITCH_COUNT 3
#define CLI_LOG_STREAM_QUEUE_DEPTH 8
#define CLI_LOG_STREAM_LINE_MAX 96

typedef struct
{
    void (*lock)(void *context);
    void (*unlock)(void *context);

    bool (*rom_save_state)(void *context);
    bool (*rom_load_state)(void *context);
    bool (*rom_read_raw)(uint16_t address, uint16_t length, uint8_t *payload_out,
                         size_t payload_capacity, size_t *payload_size_out, void *context);
    bool (*rom_write_raw)(uint16_t address, const uint8_t *payload, size_t payload_size,
                          void *context);

    bool (*log_set_level)(uint8_t level, void *context);
    bool (*log_set_enabled)(bool enabled, void *context);
    bool (*log_set_stream)(bool enabled, void *context);
    bool (*test_set_input_mode)(bool enabled, void *context);
    bool (*test_set_input_vector)(uint8_t vector, void *context);
    bool (*test_set_input_frequency_hz)(uint16_t frequency_hz, void *context);
    bool (*test_set_input_amplitude)(uint16_t amplitude, void *context);

    bool (*test_set_output_mode)(bool enabled, void *context);
    bool (*test_set_output_vector)(uint8_t vector, void *context);
    bool (*test_set_output_frequency_hz)(uint16_t frequency_hz, void *context);
    bool (*test_set_output_amplitude)(uint16_t amplitude, void *context);

    void (*system_reboot)(void *context);
    void *context;
} CliServiceAdapterOps;

typedef struct
{
    uint16_t min_address;
    uint16_t max_address;
    uint16_t max_length;
} CliRomWindow;

typedef struct
{
    bool pot_enabled[CLI_POT_COUNT];
    uint32_t pot_value[CLI_POT_COUNT];
    bool switch_enabled[CLI_SWITCH_COUNT];
    bool switch_value[CLI_SWITCH_COUNT];
} CliOverrideState;

typedef struct
{
    bool enabled;
    bool stream_enabled;
    uint8_t level;
    uint32_t frame_count;
    uint32_t failure_count;
} CliLogConfig;

typedef struct
{
    char lines[CLI_LOG_STREAM_QUEUE_DEPTH][CLI_LOG_STREAM_LINE_MAX];
    uint8_t head;
    uint8_t tail;
    uint8_t count;
    uint32_t drop_count;
} CliLogStreamQueue;

typedef struct
{
    bool enabled;
    uint8_t vector;
    uint16_t frequency_hz;
    uint16_t amplitude;
} CliTestModeState;

typedef struct
{
    uint32_t *period_ms;
    uint32_t period_min_ms;
    uint32_t period_max_ms;
} CliHeartbeatBinding;

typedef struct
{
    uint32_t frame_time_min_us;
    uint32_t frame_time_max_us;
    uint32_t frame_time_total_us;
    uint32_t frame_count;
    uint32_t overrun_count;
} CliProfileAccum;

typedef struct
{
    EffectsState *state;
    EffectsParams *params;
    CliServiceAdapterOps ops;
    uint32_t adc_max;

    CliRomWindow rom_window;
    CliOverrideState overrides;
    CliLogConfig log;
    CliLogStreamQueue log_stream;
    CliTestModeState test_input;
    CliTestModeState test_output;
    CliHeartbeatBinding heartbeat;

    CliProfileAccum profile;
    CliProfileAccum profile_batch;
    uint32_t step_failure_count;
    uint32_t step_failure_streak;
    uint8_t stream_batch_size;
    uint8_t stream_batch_counter;

    uint32_t sample_rate_hz;
    uint32_t sample_buf_len;
    uint32_t sampling_period_us;
    uint32_t delay_samples_len;
    uint32_t processing_slack_ms;
} CliServiceAdapter;

void cli_service_adapter_init(CliServiceAdapter *context, EffectsState *state,
                              EffectsParams *params, const CliServiceAdapterOps *ops,
                              uint32_t adc_max);
void cli_service_adapter_bind_heartbeat_period(CliServiceAdapter *context,
                                               uint32_t *period_ms,
                                               uint32_t min_ms,
                                               uint32_t max_ms);
void cli_service_adapter_bind(CliServiceAdapter *context, CliServices *services_out);

bool cli_service_adapter_apply_pot_sample(CliServiceAdapter *context, Effect active_effect,
                                          uint8_t pot_index, uint32_t adc_value);
bool cli_service_adapter_apply_switches(CliServiceAdapter *context, bool switch_a_enabled,
                                        bool switch_b_enabled, bool switch_c_enabled);

bool cli_service_adapter_get_test_input_mode_status(CliServiceAdapter *context,
                                                    CliTestModeStatus *status_out);
bool cli_service_adapter_get_test_output_mode_status(CliServiceAdapter *context,
                                                     CliTestModeStatus *status_out);

bool cli_service_adapter_get_log_stats(CliServiceAdapter *context,
                                       CliLogStats *stats_out);
bool cli_service_adapter_get_log_stream_enabled(CliServiceAdapter *context,
                                                bool *enabled_out);
bool cli_service_adapter_get_log_enabled(CliServiceAdapter *context, bool *enabled_out);
bool cli_service_adapter_get_log_level(CliServiceAdapter *context, uint8_t *level_out);
bool cli_service_adapter_append_log_line(CliServiceAdapter *context, const char *line);

void cli_service_adapter_note_frame_processed(CliServiceAdapter *context);
void cli_service_adapter_note_processing_failure(CliServiceAdapter *context);
void cli_service_adapter_note_step_result(CliServiceAdapter *context, bool step_succeeded);

void cli_service_adapter_set_stream_batch_size(CliServiceAdapter *context, uint8_t batch_size);
uint8_t cli_service_adapter_get_stream_batch_size(CliServiceAdapter *context);
void cli_service_adapter_reset_profiling_stats(CliServiceAdapter *context);
void cli_service_adapter_note_frame_timing(CliServiceAdapter *context, uint32_t frame_time_us,
                                           bool overrun);
void cli_service_adapter_set_audio_info(CliServiceAdapter *context,
                                        uint32_t sample_rate_hz,
                                        uint32_t sample_buf_len,
                                        uint32_t sampling_period_us,
                                        uint32_t delay_samples_len,
                                        uint32_t processing_slack_ms);

#endif