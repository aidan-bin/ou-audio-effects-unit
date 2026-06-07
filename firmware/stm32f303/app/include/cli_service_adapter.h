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
    bool (*test_set_mode)(bool enabled, void *context);
    bool (*test_set_vector)(uint8_t vector, void *context);
    bool (*test_set_frequency_hz)(uint16_t frequencyHz, void *context);
    bool (*test_set_amplitude)(uint16_t amplitude, void *context);
    void *context;
} CliServiceAdapterOps;

typedef struct
{
    EffectsState *state;
    EffectsParams *params;
    CliServiceAdapterOps ops;

    bool potOverrideEnabled[CLI_POT_COUNT];
    uint32_t potOverrideValue[CLI_POT_COUNT];
    bool switchOverrideEnabled[CLI_SWITCH_COUNT];
    bool switchOverrideValue[CLI_SWITCH_COUNT];

    uint32_t adcMax;
    uint16_t romRawMinAddress;
    uint16_t romRawMaxAddress;
    uint16_t romRawMaxLength;

    bool logEnabled;
    bool logStreamEnabled;
    uint8_t logLevel;
    uint32_t logFrameCount;
    uint32_t logFailureCount;
    char logStreamQueue[CLI_LOG_STREAM_QUEUE_DEPTH][CLI_LOG_STREAM_LINE_MAX];
    uint8_t logStreamHead;
    uint8_t logStreamTail;
    uint8_t logStreamCount;

    bool testModeEnabled;
    uint8_t testVector;
    uint16_t testFrequencyHz;
    uint16_t testAmplitude;

    uint32_t *heartbeatPeriodMs;
    uint32_t heartbeatPeriodMinMs;
    uint32_t heartbeatPeriodMaxMs;
} CliServiceAdapterContext;

void cli_service_adapter_init(CliServiceAdapterContext *context, EffectsState *state,
                              EffectsParams *params, const CliServiceAdapterOps *ops,
                              uint32_t adcMax);
void cli_service_adapter_bind_heartbeat_period(CliServiceAdapterContext *context,
                                               uint32_t *periodMs,
                                               uint32_t minMs,
                                               uint32_t maxMs);
void cli_service_adapter_bind(CliServiceAdapterContext *context, CliServices *servicesOut);

bool cli_service_adapter_apply_pot_sample(CliServiceAdapterContext *context, Effect activeEffect,
                                          uint8_t potIndex, uint32_t adcValue);
bool cli_service_adapter_apply_switches(CliServiceAdapterContext *context, bool switchAEnabled,
                                        bool switchBEnabled, bool switchCEnabled);

bool cli_service_adapter_get_test_mode_status(CliServiceAdapterContext *context,
                                              CliTestModeStatus *statusOut);

bool cli_service_adapter_get_log_stats(CliServiceAdapterContext *context,
                                       CliLogStats *statsOut);
bool cli_service_adapter_get_log_stream_enabled(CliServiceAdapterContext *context,
                                                bool *enabledOut);
bool cli_service_adapter_get_log_enabled(CliServiceAdapterContext *context, bool *enabledOut);
bool cli_service_adapter_get_log_level(CliServiceAdapterContext *context, uint8_t *levelOut);
bool cli_service_adapter_append_log_line(CliServiceAdapterContext *context, const char *line);

void cli_service_adapter_note_frame_processed(CliServiceAdapterContext *context);
void cli_service_adapter_note_processing_failure(CliServiceAdapterContext *context);

#endif