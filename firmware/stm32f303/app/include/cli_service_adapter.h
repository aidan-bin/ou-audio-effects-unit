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
    bool (*rom_read_raw)(uint16_t address, uint16_t length, uint8_t *payloadOut,
                         size_t payloadCapacity, size_t *payloadSizeOut, void *context);
    bool (*rom_write_raw)(uint16_t address, const uint8_t *payload, size_t payloadSize,
                          void *context);

    bool (*log_set_level)(uint8_t level, void *context);
    bool (*log_set_enabled)(bool enabled, void *context);
    bool (*log_set_stream)(bool enabled, void *context);
    bool (*test_set_input_mode)(bool enabled, void *context);
    bool (*test_set_input_vector)(uint8_t vector, void *context);
    bool (*test_set_input_frequency_hz)(uint16_t frequencyHz, void *context);
    bool (*test_set_input_amplitude)(uint16_t amplitude, void *context);

    bool (*test_set_output_mode)(bool enabled, void *context);
    bool (*test_set_output_vector)(uint8_t vector, void *context);
    bool (*test_set_output_frequency_hz)(uint16_t frequencyHz, void *context);
    bool (*test_set_output_amplitude)(uint16_t amplitude, void *context);

    void (*system_reboot)(void *context);
    void *context;
} CliServiceAdapterOps;

typedef struct
{
    uint16_t minAddress;
    uint16_t maxAddress;
    uint16_t maxLength;
} CliRomWindow;

typedef struct
{
    bool potEnabled[CLI_POT_COUNT];
    uint32_t potValue[CLI_POT_COUNT];
    bool switchEnabled[CLI_SWITCH_COUNT];
    bool switchValue[CLI_SWITCH_COUNT];
} CliOverrideState;

typedef struct
{
    bool enabled;
    bool streamEnabled;
    uint8_t level;
    uint32_t frameCount;
    uint32_t failureCount;
} CliLogConfig;

typedef struct
{
    char lines[CLI_LOG_STREAM_QUEUE_DEPTH][CLI_LOG_STREAM_LINE_MAX];
    uint8_t head;
    uint8_t tail;
    uint8_t count;
    uint32_t dropCount;
} CliLogStreamQueue;

typedef struct
{
    bool enabled;
    uint8_t vector;
    uint16_t frequencyHz;
    uint16_t amplitude;
} CliTestModeState;

typedef struct
{
    uint32_t *periodMs;
    uint32_t periodMinMs;
    uint32_t periodMaxMs;
} CliHeartbeatBinding;

typedef struct
{
    uint32_t frameTimeMinUs;
    uint32_t frameTimeMaxUs;
    uint32_t frameTimeTotalUs;
    uint32_t frameCount;
    uint32_t overrunCount;
} CliProfileAccum;

typedef struct
{
    EffectsState *state;
    EffectsParams *params;
    CliServiceAdapterOps ops;
    uint32_t adcMax;

    CliRomWindow romWindow;
    CliOverrideState overrides;
    CliLogConfig log;
    CliLogStreamQueue logStream;
    CliTestModeState testInput;
    CliTestModeState testOutput;
    CliHeartbeatBinding heartbeat;

    CliProfileAccum profile;
    CliProfileAccum profileBatch;
    uint32_t stepFailureCount;
    uint32_t stepFailureStreak;
    uint8_t streamBatchSize;
    uint8_t streamBatchCounter;

    uint32_t sampleRateHz;
    uint32_t sampleBufLen;
    uint32_t samplingPeriodUs;
    uint32_t delaySamplesLen;
    uint32_t processingSlackMs;
} CliServiceAdapter;

void cli_service_adapter_init(CliServiceAdapter *context, EffectsState *state,
                              EffectsParams *params, const CliServiceAdapterOps *ops,
                              uint32_t adcMax);
void cli_service_adapter_bind_heartbeat_period(CliServiceAdapter *context,
                                               uint32_t *periodMs,
                                               uint32_t minMs,
                                               uint32_t maxMs);
void cli_service_adapter_bind(CliServiceAdapter *context, CliServices *servicesOut);

bool cli_service_adapter_apply_pot_sample(CliServiceAdapter *context, Effect activeEffect,
                                          uint8_t potIndex, uint32_t adcValue);
bool cli_service_adapter_apply_switches(CliServiceAdapter *context, bool switchAEnabled,
                                        bool switchBEnabled, bool switchCEnabled);

bool cli_service_adapter_get_test_input_mode_status(CliServiceAdapter *context,
                                                    CliTestModeStatus *statusOut);
bool cli_service_adapter_get_test_output_mode_status(CliServiceAdapter *context,
                                                     CliTestModeStatus *statusOut);

bool cli_service_adapter_get_log_stats(CliServiceAdapter *context,
                                       CliLogStats *statsOut);
bool cli_service_adapter_get_log_stream_enabled(CliServiceAdapter *context,
                                                bool *enabledOut);
bool cli_service_adapter_get_log_enabled(CliServiceAdapter *context, bool *enabledOut);
bool cli_service_adapter_get_log_level(CliServiceAdapter *context, uint8_t *levelOut);
bool cli_service_adapter_append_log_line(CliServiceAdapter *context, const char *line);

void cli_service_adapter_note_frame_processed(CliServiceAdapter *context);
void cli_service_adapter_note_processing_failure(CliServiceAdapter *context);
void cli_service_adapter_note_step_result(CliServiceAdapter *context, bool stepSucceeded);

void cli_service_adapter_set_stream_batch_size(CliServiceAdapter *context, uint8_t batchSize);
uint8_t cli_service_adapter_get_stream_batch_size(CliServiceAdapter *context);
void cli_service_adapter_reset_profiling_stats(CliServiceAdapter *context);
void cli_service_adapter_note_frame_timing(CliServiceAdapter *context, uint32_t frameTimeUs,
                                           bool overrun);
void cli_service_adapter_set_audio_info(CliServiceAdapter *context,
                                        uint32_t sampleRateHz,
                                        uint32_t sampleBufLen,
                                        uint32_t samplingPeriodUs,
                                        uint32_t delaySamplesLen,
                                        uint32_t processingSlackMs);

#endif