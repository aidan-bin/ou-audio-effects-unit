#ifndef CLI_SERVICE_ADAPTER_H
#define CLI_SERVICE_ADAPTER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "cli_core.h"
#include "effects_model.h"

#define CLI_POT_COUNT 4
#define CLI_SWITCH_COUNT 3

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
} CliServiceAdapterContext;

void cli_service_adapter_init(CliServiceAdapterContext *context, EffectsState *state,
                              EffectsParams *params, const CliServiceAdapterOps *ops,
                              uint32_t adcMax);
void cli_service_adapter_bind(CliServiceAdapterContext *context, CliServices *servicesOut);

bool cli_service_adapter_apply_pot_sample(CliServiceAdapterContext *context, Effect activeEffect,
                                          uint8_t potIndex, uint32_t adcValue);
bool cli_service_adapter_apply_switches(CliServiceAdapterContext *context, bool switchAEnabled,
                                        bool switchBEnabled, bool switchCEnabled);

#endif