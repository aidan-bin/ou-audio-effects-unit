#include "cli_service_adapter.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

static void lock_ctx(CliServiceAdapterContext *context)
{
    if (context != NULL && context->ops.lock != NULL)
    {
        context->ops.lock(context->ops.context);
    }
}

static void unlock_ctx(CliServiceAdapterContext *context)
{
    if (context != NULL && context->ops.unlock != NULL)
    {
        context->ops.unlock(context->ops.context);
    }
}

static bool assign_bounded_size_t(int32_t value, size_t min, size_t max, size_t *field)
{
    if (field == NULL || value < 0)
    {
        return false;
    }

    size_t cast_value = (size_t)value;
    if (cast_value < min || cast_value > max)
    {
        return false;
    }

    *field = cast_value;
    return true;
}

static bool set_param_value(EffectsParams *params, const char *key, int32_t value)
{
    if (params == NULL || key == NULL)
    {
        return false;
    }

    if (strcmp(key, "overdrive.gain") == 0)
    {
        return assign_bounded_size_t(value, params->overdriveMin.gain, params->overdriveMax.gain,
                                     &params->overdrive.gain);
    }
    if (strcmp(key, "overdrive.level") == 0)
    {
        return assign_bounded_size_t(value, params->overdriveMin.level, params->overdriveMax.level,
                                     &params->overdrive.level);
    }
    if (strcmp(key, "overdrive.tone") == 0)
    {
        return assign_bounded_size_t(value, params->overdriveMin.tone, params->overdriveMax.tone,
                                     &params->overdrive.tone);
    }
    if (strcmp(key, "overdrive.mix") == 0)
    {
        return assign_bounded_size_t(value, params->overdriveMin.mix, params->overdriveMax.mix,
                                     &params->overdrive.mix);
    }

    if (strcmp(key, "echo.delay_samples") == 0)
    {
        return assign_bounded_size_t(value, params->echoMin.delay_samples,
                                     params->echoMax.delay_samples, &params->echo.delay_samples);
    }
    if (strcmp(key, "echo.pre_delay") == 0)
    {
        return assign_bounded_size_t(value, params->echoMin.pre_delay, params->echoMax.pre_delay,
                                     &params->echo.pre_delay);
    }
    if (strcmp(key, "echo.density") == 0)
    {
        return assign_bounded_size_t(value, params->echoMin.density, params->echoMax.density,
                                     &params->echo.density);
    }
    if (strcmp(key, "echo.attack") == 0)
    {
        return assign_bounded_size_t(value, params->echoMin.attack, params->echoMax.attack,
                                     &params->echo.attack);
    }
    if (strcmp(key, "echo.decay") == 0)
    {
        return assign_bounded_size_t(value, params->echoMin.decay, params->echoMax.decay,
                                     &params->echo.decay);
    }

    if (strcmp(key, "compression.threshold") == 0)
    {
        return assign_bounded_size_t(value, params->compressionMin.threshold,
                                     params->compressionMax.threshold,
                                     &params->compression.threshold);
    }
    if (strcmp(key, "compression.ratio") == 0)
    {
        return assign_bounded_size_t(value, params->compressionMin.ratio,
                                     params->compressionMax.ratio, &params->compression.ratio);
    }

    return false;
}

static bool get_param_value(const EffectsParams *params, const char *key, int32_t *value_out)
{
    size_t value = 0;

    if (params == NULL || key == NULL || value_out == NULL)
    {
        return false;
    }

    if (strcmp(key, "overdrive.gain") == 0)
    {
        value = params->overdrive.gain;
    }
    else if (strcmp(key, "overdrive.level") == 0)
    {
        value = params->overdrive.level;
    }
    else if (strcmp(key, "overdrive.tone") == 0)
    {
        value = params->overdrive.tone;
    }
    else if (strcmp(key, "overdrive.mix") == 0)
    {
        value = params->overdrive.mix;
    }
    else if (strcmp(key, "echo.delay_samples") == 0)
    {
        value = params->echo.delay_samples;
    }
    else if (strcmp(key, "echo.pre_delay") == 0)
    {
        value = params->echo.pre_delay;
    }
    else if (strcmp(key, "echo.density") == 0)
    {
        value = params->echo.density;
    }
    else if (strcmp(key, "echo.attack") == 0)
    {
        value = params->echo.attack;
    }
    else if (strcmp(key, "echo.decay") == 0)
    {
        value = params->echo.decay;
    }
    else if (strcmp(key, "compression.threshold") == 0)
    {
        value = params->compression.threshold;
    }
    else if (strcmp(key, "compression.ratio") == 0)
    {
        value = params->compression.ratio;
    }
    else
    {
        return false;
    }

    if (value > (size_t)INT32_MAX)
    {
        return false;
    }

    *value_out = (int32_t)value;
    return true;
}

static bool set_state_value(EffectsState *state, const char *key, int32_t value)
{
    if (state == NULL || key == NULL)
    {
        return false;
    }

    if (strcmp(key, "state.active_effect") == 0)
    {
        if (value < 0 || value >= NUM_EFFECTS)
        {
            return false;
        }
        state->activeEffectSelection = (uint8_t)value;
        return true;
    }

    if (strcmp(key, "state.enable.overdrive") == 0)
    {
        if (value < 0 || value > 1)
        {
            return false;
        }
        state->isEnabled[OVERDRIVE] = value == 1;
        return true;
    }
    if (strcmp(key, "state.enable.echo") == 0)
    {
        if (value < 0 || value > 1)
        {
            return false;
        }
        state->isEnabled[ECHO] = value == 1;
        return true;
    }
    if (strcmp(key, "state.enable.compression") == 0)
    {
        if (value < 0 || value > 1)
        {
            return false;
        }
        state->isEnabled[COMPRESSION] = value == 1;
        return true;
    }

    return false;
}

static bool get_state_value(const EffectsState *state, const char *key, int32_t *value_out)
{
    if (state == NULL || key == NULL || value_out == NULL)
    {
        return false;
    }

    if (strcmp(key, "state.active_effect") == 0)
    {
        *value_out = state->activeEffectSelection;
        return true;
    }

    if (strcmp(key, "state.enable.overdrive") == 0)
    {
        *value_out = state->isEnabled[OVERDRIVE] ? 1 : 0;
        return true;
    }
    if (strcmp(key, "state.enable.echo") == 0)
    {
        *value_out = state->isEnabled[ECHO] ? 1 : 0;
        return true;
    }
    if (strcmp(key, "state.enable.compression") == 0)
    {
        *value_out = state->isEnabled[COMPRESSION] ? 1 : 0;
        return true;
    }

    return false;
}

static bool set_runtime_value(CliServiceAdapterContext *context, const char *key, int32_t value)
{
    if (context == NULL || key == NULL || value < 0)
    {
        return false;
    }

    if (strcmp(key, "heartbeat.period_ms") != 0 &&
        strcmp(key, "system.heartbeat_ms") != 0)
    {
        return false;
    }

    if (context->heartbeatPeriodMs == NULL)
    {
        return false;
    }

    uint32_t period_ms = (uint32_t)value;
    if (period_ms < context->heartbeatPeriodMinMs || period_ms > context->heartbeatPeriodMaxMs)
    {
        return false;
    }

    *context->heartbeatPeriodMs = period_ms;
    return true;
}

static bool get_runtime_value(const CliServiceAdapterContext *context,
                              const char *key,
                              int32_t *value_out)
{
    if (context == NULL || key == NULL || value_out == NULL)
    {
        return false;
    }

    if (strcmp(key, "heartbeat.period_ms") != 0 &&
        strcmp(key, "system.heartbeat_ms") != 0)
    {
        return false;
    }

    if (context->heartbeatPeriodMs == NULL ||
        *context->heartbeatPeriodMs > (uint32_t)INT32_MAX)
    {
        return false;
    }

    *value_out = (int32_t)(*context->heartbeatPeriodMs);
    return true;
}

static bool in_rom_window(const CliServiceAdapterContext *context, uint16_t address, uint16_t length)
{
    if (context == NULL || length == 0 || length > context->romRawMaxLength)
    {
        return false;
    }

    if (address < context->romRawMinAddress || address > context->romRawMaxAddress)
    {
        return false;
    }

    uint32_t end = (uint32_t)address + (uint32_t)length - 1U;
    return end <= context->romRawMaxAddress;
}

static void log_stream_queue_clear(CliServiceAdapterContext *context)
{
    if (context == NULL)
    {
        return;
    }

    context->logStreamHead = 0;
    context->logStreamTail = 0;
    context->logStreamCount = 0;
}

static bool log_stream_queue_push(CliServiceAdapterContext *context, const char *line)
{
    if (context == NULL || line == NULL)
    {
        return false;
    }

    if (context->logStreamCount >= CLI_LOG_STREAM_QUEUE_DEPTH)
    {
        context->streamDropCount++;
        context->logStreamHead = (uint8_t)((context->logStreamHead + 1U) % CLI_LOG_STREAM_QUEUE_DEPTH);
        context->logStreamCount--;
    }

    (void)strncpy(context->logStreamQueue[context->logStreamTail], line,
                  CLI_LOG_STREAM_LINE_MAX - 1U);
    context->logStreamQueue[context->logStreamTail][CLI_LOG_STREAM_LINE_MAX - 1U] = '\0';
    context->logStreamTail = (uint8_t)((context->logStreamTail + 1U) % CLI_LOG_STREAM_QUEUE_DEPTH);
    context->logStreamCount++;
    return true;
}

static bool log_stream_queue_pop(CliServiceAdapterContext *context,
                                 char *line_out,
                                 size_t line_capacity,
                                 bool *has_line_out)
{
    if (context == NULL || line_out == NULL || line_capacity == 0 || has_line_out == NULL)
    {
        return false;
    }

    if (context->logStreamCount == 0)
    {
        *has_line_out = false;
        line_out[0] = '\0';
        return true;
    }

    (void)strncpy(line_out, context->logStreamQueue[context->logStreamHead], line_capacity - 1U);
    line_out[line_capacity - 1U] = '\0';

    context->logStreamHead = (uint8_t)((context->logStreamHead + 1U) % CLI_LOG_STREAM_QUEUE_DEPTH);
    context->logStreamCount--;
    *has_line_out = true;
    return true;
}

static bool service_set_pot_override(uint8_t pot_index, uint32_t value, void *ctx)
{
    CliServiceAdapterContext *context = (CliServiceAdapterContext *)ctx;
    if (context == NULL || pot_index >= CLI_POT_COUNT)
    {
        return false;
    }

    lock_ctx(context);
    context->potOverrideEnabled[pot_index] = true;
    context->potOverrideValue[pot_index] = value;
    unlock_ctx(context);
    return true;
}

static bool service_clear_pot_override(uint8_t pot_index, void *ctx)
{
    CliServiceAdapterContext *context = (CliServiceAdapterContext *)ctx;
    if (context == NULL || pot_index >= CLI_POT_COUNT)
    {
        return false;
    }

    lock_ctx(context);
    context->potOverrideEnabled[pot_index] = false;
    unlock_ctx(context);
    return true;
}

static bool service_set_switch_override(uint8_t switch_index, bool enabled, void *ctx)
{
    CliServiceAdapterContext *context = (CliServiceAdapterContext *)ctx;
    if (context == NULL || switch_index >= CLI_SWITCH_COUNT)
    {
        return false;
    }

    lock_ctx(context);
    context->switchOverrideEnabled[switch_index] = true;
    context->switchOverrideValue[switch_index] = enabled;
    unlock_ctx(context);
    return true;
}

static bool service_clear_switch_override(uint8_t switch_index, void *ctx)
{
    CliServiceAdapterContext *context = (CliServiceAdapterContext *)ctx;
    if (context == NULL || switch_index >= CLI_SWITCH_COUNT)
    {
        return false;
    }

    lock_ctx(context);
    context->switchOverrideEnabled[switch_index] = false;
    unlock_ctx(context);
    return true;
}

static bool service_clear_all_overrides(void *ctx)
{
    CliServiceAdapterContext *context = (CliServiceAdapterContext *)ctx;
    if (context == NULL)
    {
        return false;
    }

    lock_ctx(context);
    memset(context->potOverrideEnabled, 0, sizeof(context->potOverrideEnabled));
    memset(context->switchOverrideEnabled, 0, sizeof(context->switchOverrideEnabled));
    unlock_ctx(context);
    return true;
}

static bool service_config_set(const char *key, int32_t value, void *ctx)
{
    CliServiceAdapterContext *context = (CliServiceAdapterContext *)ctx;
    bool success = false;

    if (context == NULL || context->state == NULL || context->params == NULL)
    {
        return false;
    }

    lock_ctx(context);
    success = set_state_value(context->state, key, value) ||
              set_param_value(context->params, key, value) ||
              set_runtime_value(context, key, value);
    if (success)
    {
        effects_state_normalize(context->state);
    }
    unlock_ctx(context);

    return success;
}

static bool service_config_get(const char *key, int32_t *value_out, void *ctx)
{
    CliServiceAdapterContext *context = (CliServiceAdapterContext *)ctx;
    bool success = false;

    if (context == NULL || context->state == NULL || context->params == NULL)
    {
        return false;
    }

    lock_ctx(context);
    success = get_state_value(context->state, key, value_out) ||
              get_param_value(context->params, key, value_out) ||
              get_runtime_value(context, key, value_out);
    unlock_ctx(context);

    return success;
}

static bool service_rom_save_state(void *ctx)
{
    CliServiceAdapterContext *context = (CliServiceAdapterContext *)ctx;
    return context != NULL && context->ops.rom_save_state != NULL &&
           context->ops.rom_save_state(context->ops.context);
}

static bool service_rom_load_state(void *ctx)
{
    CliServiceAdapterContext *context = (CliServiceAdapterContext *)ctx;
    return context != NULL && context->ops.rom_load_state != NULL &&
           context->ops.rom_load_state(context->ops.context);
}

static bool service_rom_read_raw(uint16_t address, uint16_t length, uint8_t *payload_out,
                                 size_t payload_capacity, size_t *payload_size_out, void *ctx)
{
    CliServiceAdapterContext *context = (CliServiceAdapterContext *)ctx;

    if (context == NULL || context->ops.rom_read_raw == NULL || !in_rom_window(context, address, length))
    {
        return false;
    }

    return context->ops.rom_read_raw(address, length, payload_out, payload_capacity,
                                     payload_size_out, context->ops.context);
}

static bool service_rom_write_raw(uint16_t address, const uint8_t *payload, size_t payload_size,
                                  void *ctx)
{
    CliServiceAdapterContext *context = (CliServiceAdapterContext *)ctx;

    if (context == NULL || context->ops.rom_write_raw == NULL || payload_size > UINT16_MAX)
    {
        return false;
    }

    if (!in_rom_window(context, address, (uint16_t)payload_size))
    {
        return false;
    }

    return context->ops.rom_write_raw(address, payload, payload_size, context->ops.context);
}

static bool service_log_set_level(uint8_t level, void *ctx)
{
    CliServiceAdapterContext *context = (CliServiceAdapterContext *)ctx;
    if (context == NULL)
    {
        return false;
    }

    lock_ctx(context);
    context->logLevel = level;
    unlock_ctx(context);

    if (context->ops.log_set_level != NULL)
    {
        return context->ops.log_set_level(level, context->ops.context);
    }

    return true;
}

static bool service_log_set_enabled(bool enabled, void *ctx)
{
    CliServiceAdapterContext *context = (CliServiceAdapterContext *)ctx;
    if (context == NULL)
    {
        return false;
    }

    lock_ctx(context);
    context->logEnabled = enabled;
    unlock_ctx(context);

    if (context->ops.log_set_enabled != NULL)
    {
        return context->ops.log_set_enabled(enabled, context->ops.context);
    }

    return true;
}

static bool service_log_set_stream(bool enabled, void *ctx)
{
    CliServiceAdapterContext *context = (CliServiceAdapterContext *)ctx;
    if (context == NULL)
    {
        return false;
    }

    lock_ctx(context);
    context->logEnabled = enabled;
    context->logStreamEnabled = enabled;
    if (!enabled)
    {
        log_stream_queue_clear(context);
    }
    unlock_ctx(context);

    if (context->ops.log_set_stream != NULL)
    {
        return context->ops.log_set_stream(enabled, context->ops.context);
    }

    return true;
}

static bool service_log_read_line(char *line_out,
                                  size_t line_capacity,
                                  bool *has_line_out,
                                  void *ctx)
{
    CliServiceAdapterContext *context = (CliServiceAdapterContext *)ctx;
    if (context == NULL || line_out == NULL || line_capacity == 0 || has_line_out == NULL)
    {
        return false;
    }

    lock_ctx(context);
    const bool success = log_stream_queue_pop(context, line_out, line_capacity, has_line_out);
    unlock_ctx(context);

    return success;
}

static bool service_log_set_stream_batch(uint8_t batch_size, void *ctx)
{
    CliServiceAdapterContext *context = (CliServiceAdapterContext *)ctx;
    if (context == NULL || batch_size == 0)
    {
        return false;
    }

    lock_ctx(context);
    context->streamBatchSize = batch_size;
    unlock_ctx(context);
    return true;
}

static bool service_log_reset_stats(void *ctx)
{
    CliServiceAdapterContext *context = (CliServiceAdapterContext *)ctx;
    if (context == NULL)
    {
        return false;
    }

    cli_service_adapter_reset_profiling_stats(context);
    return true;
}

static bool service_log_get_stats(CliLogStats *stats_out, void *ctx)
{
    CliServiceAdapterContext *context = (CliServiceAdapterContext *)ctx;
    if (context == NULL || stats_out == NULL)
    {
        return false;
    }

    lock_ctx(context);
    stats_out->enabled = context->logEnabled;
    stats_out->streamEnabled = context->logStreamEnabled;
    stats_out->level = context->logLevel;
    stats_out->frameCount = context->logFrameCount;
    stats_out->failureCount = context->logFailureCount;
    stats_out->stepFailureCount = context->stepFailureCount;
    stats_out->stepFailureStreak = context->stepFailureStreak;
    stats_out->frameTimeMinUs = context->frameTimeMinUs;
    stats_out->frameTimeMaxUs = context->frameTimeMaxUs;
    stats_out->frameTimeTotalUs = context->frameTimeTotalUs;
    stats_out->measuredFrameCount = context->measuredFrameCount;
    stats_out->overrunCount = context->overrunCount;
    stats_out->streamDropCount = context->streamDropCount;
    stats_out->streamBatchSize = context->streamBatchSize;
    stats_out->streamQueueCount = context->logStreamCount;
    unlock_ctx(context);

    return true;
}

static bool service_test_set_input_mode(bool enabled, void *ctx)
{
    CliServiceAdapterContext *context = (CliServiceAdapterContext *)ctx;
    if (context == NULL)
    {
        return false;
    }

    lock_ctx(context);
    context->testInputModeEnabled = enabled;
    unlock_ctx(context);

    if (context->ops.test_set_input_mode != NULL)
    {
        return context->ops.test_set_input_mode(enabled, context->ops.context);
    }

    return true;
}

static bool service_test_set_input_vector(uint8_t vector, void *ctx)
{
    CliServiceAdapterContext *context = (CliServiceAdapterContext *)ctx;
    if (context == NULL || vector > 2U)
    {
        return false;
    }

    lock_ctx(context);
    context->testInputVector = vector;
    unlock_ctx(context);

    if (context->ops.test_set_input_vector != NULL)
    {
        return context->ops.test_set_input_vector(vector, context->ops.context);
    }

    return true;
}

static bool service_test_set_input_frequency_hz(uint16_t frequency_hz, void *ctx)
{
    CliServiceAdapterContext *context = (CliServiceAdapterContext *)ctx;
    if (context == NULL || frequency_hz < 20U || frequency_hz > 8000U)
    {
        return false;
    }

    lock_ctx(context);
    context->testInputFrequencyHz = frequency_hz;
    unlock_ctx(context);

    if (context->ops.test_set_input_frequency_hz != NULL)
    {
        return context->ops.test_set_input_frequency_hz(frequency_hz, context->ops.context);
    }

    return true;
}

static bool service_test_set_input_amplitude(uint16_t amplitude, void *ctx)
{
    CliServiceAdapterContext *context = (CliServiceAdapterContext *)ctx;
    if (context == NULL || amplitude > X_AXIS)
    {
        return false;
    }

    lock_ctx(context);
    context->testInputAmplitude = amplitude;
    unlock_ctx(context);

    if (context->ops.test_set_input_amplitude != NULL)
    {
        return context->ops.test_set_input_amplitude(amplitude, context->ops.context);
    }

    return true;
}

static bool service_test_get_input_status(CliTestModeStatus *status_out, void *ctx)
{
    CliServiceAdapterContext *context = (CliServiceAdapterContext *)ctx;
    if (context == NULL || status_out == NULL)
    {
        return false;
    }

    lock_ctx(context);
    status_out->enabled = context->testInputModeEnabled;
    status_out->vector = context->testInputVector;
    status_out->frequencyHz = context->testInputFrequencyHz;
    status_out->amplitude = context->testInputAmplitude;
    unlock_ctx(context);

    return true;
}

static bool service_test_set_output_mode(bool enabled, void *ctx)
{
    CliServiceAdapterContext *context = (CliServiceAdapterContext *)ctx;
    if (context == NULL)
    {
        return false;
    }

    lock_ctx(context);
    context->testOutputModeEnabled = enabled;
    unlock_ctx(context);

    if (context->ops.test_set_output_mode != NULL)
    {
        return context->ops.test_set_output_mode(enabled, context->ops.context);
    }

    return true;
}

static bool service_test_set_output_vector(uint8_t vector, void *ctx)
{
    CliServiceAdapterContext *context = (CliServiceAdapterContext *)ctx;
    if (context == NULL || vector > 2U)
    {
        return false;
    }

    lock_ctx(context);
    context->testOutputVector = vector;
    unlock_ctx(context);

    if (context->ops.test_set_output_vector != NULL)
    {
        return context->ops.test_set_output_vector(vector, context->ops.context);
    }

    return true;
}

static bool service_test_set_output_frequency_hz(uint16_t frequency_hz, void *ctx)
{
    CliServiceAdapterContext *context = (CliServiceAdapterContext *)ctx;
    if (context == NULL || frequency_hz < 20U || frequency_hz > 8000U)
    {
        return false;
    }

    lock_ctx(context);
    context->testOutputFrequencyHz = frequency_hz;
    unlock_ctx(context);

    if (context->ops.test_set_output_frequency_hz != NULL)
    {
        return context->ops.test_set_output_frequency_hz(frequency_hz, context->ops.context);
    }

    return true;
}

static bool service_test_set_output_amplitude(uint16_t amplitude, void *ctx)
{
    CliServiceAdapterContext *context = (CliServiceAdapterContext *)ctx;
    if (context == NULL || amplitude > X_AXIS)
    {
        return false;
    }

    lock_ctx(context);
    context->testOutputAmplitude = amplitude;
    unlock_ctx(context);

    if (context->ops.test_set_output_amplitude != NULL)
    {
        return context->ops.test_set_output_amplitude(amplitude, context->ops.context);
    }

    return true;
}

static bool service_test_get_output_status(CliTestModeStatus *status_out, void *ctx)
{
    CliServiceAdapterContext *context = (CliServiceAdapterContext *)ctx;
    if (context == NULL || status_out == NULL)
    {
        return false;
    }

    lock_ctx(context);
    status_out->enabled = context->testOutputModeEnabled;
    status_out->vector = context->testOutputVector;
    status_out->frequencyHz = context->testOutputFrequencyHz;
    status_out->amplitude = context->testOutputAmplitude;
    unlock_ctx(context);

    return true;
}

void cli_service_adapter_init(CliServiceAdapterContext *context, EffectsState *state,
                              EffectsParams *params, const CliServiceAdapterOps *ops,
                              uint32_t adc_max)
{
    if (context == NULL)
    {
        return;
    }

    memset(context, 0, sizeof(*context));
    context->state = state;
    context->params = params;
    context->adcMax = adc_max;
    context->romRawMinAddress = 0x0000;
    context->romRawMaxAddress = 0x01FF;
    context->romRawMaxLength = CLI_MAX_ROM_BYTES;
    context->logEnabled = false;
    context->logStreamEnabled = false;
    context->logLevel = 0;
    context->logFrameCount = 0;
    context->logFailureCount = 0;
    context->testInputModeEnabled = false;
    context->testInputVector = 0;
    context->testInputFrequencyHz = 1000;
    context->testInputAmplitude = (uint16_t)(X_AXIS / 2U);

    context->testOutputModeEnabled = false;
    context->testOutputVector = 0;
    context->testOutputFrequencyHz = 1000;
    context->testOutputAmplitude = (uint16_t)(X_AXIS / 2U);

    context->frameTimeMinUs = 0;
    context->frameTimeMaxUs = 0;
    context->frameTimeTotalUs = 0;
    context->measuredFrameCount = 0;
    context->overrunCount = 0;
    context->streamDropCount = 0;
    context->streamBatchSize = 1;
    context->stepFailureCount = 0;
    context->stepFailureStreak = 0;
    context->streamBatchCounter = 0;
    context->batchFrameTimeMinUs = 0;
    context->batchFrameTimeMaxUs = 0;
    context->batchFrameTimeTotalUs = 0;
    context->batchFrameCount = 0;
    context->batchOverrunCount = 0;

    if (ops != NULL)
    {
        context->ops = *ops;
    }
}

void cli_service_adapter_bind_heartbeat_period(CliServiceAdapterContext *context,
                                               uint32_t *period_ms,
                                               uint32_t min_ms,
                                               uint32_t max_ms)
{
    if (context == NULL || period_ms == NULL || min_ms > max_ms)
    {
        return;
    }

    context->heartbeatPeriodMs = period_ms;
    context->heartbeatPeriodMinMs = min_ms;
    context->heartbeatPeriodMaxMs = max_ms;
}

void cli_service_adapter_bind(CliServiceAdapterContext *context, CliServices *services_out)
{
    if (context == NULL || services_out == NULL)
    {
        return;
    }

    services_out->set_pot_override = service_set_pot_override;
    services_out->clear_pot_override = service_clear_pot_override;
    services_out->set_switch_override = service_set_switch_override;
    services_out->clear_switch_override = service_clear_switch_override;
    services_out->clear_all_overrides = service_clear_all_overrides;
    services_out->config_set = service_config_set;
    services_out->config_get = service_config_get;
    services_out->rom_save_state = service_rom_save_state;
    services_out->rom_load_state = service_rom_load_state;
    services_out->rom_read_raw = service_rom_read_raw;
    services_out->rom_write_raw = service_rom_write_raw;
    services_out->log_set_level = service_log_set_level;
    services_out->log_set_enabled = service_log_set_enabled;
    services_out->log_set_stream = service_log_set_stream;
    services_out->log_set_stream_batch = service_log_set_stream_batch;
    services_out->log_read_line = service_log_read_line;
    services_out->log_get_stats = service_log_get_stats;
    services_out->log_reset_stats = service_log_reset_stats;
    services_out->test_set_input_mode = service_test_set_input_mode;
    services_out->test_set_input_vector = service_test_set_input_vector;
    services_out->test_set_input_frequency_hz = service_test_set_input_frequency_hz;
    services_out->test_set_input_amplitude = service_test_set_input_amplitude;
    services_out->test_get_input_status = service_test_get_input_status;

    services_out->test_set_output_mode = service_test_set_output_mode;
    services_out->test_set_output_vector = service_test_set_output_vector;
    services_out->test_set_output_frequency_hz = service_test_set_output_frequency_hz;
    services_out->test_set_output_amplitude = service_test_set_output_amplitude;
    services_out->test_get_output_status = service_test_get_output_status;
    services_out->context = context;
}

bool cli_service_adapter_apply_pot_sample(CliServiceAdapterContext *context, Effect active_effect,
                                          uint8_t pot_index, uint32_t adc_value)
{
    if (context == NULL || context->params == NULL || pot_index >= CLI_POT_COUNT)
    {
        return false;
    }

    lock_ctx(context);
    uint32_t effective_adc = adc_value;
    if (context->potOverrideEnabled[pot_index])
    {
        effective_adc = context->potOverrideValue[pot_index];
    }

    bool success = effects_params_apply_pot_sample(context->params, active_effect, pot_index,
                                                   effective_adc, context->adcMax);
    unlock_ctx(context);
    return success;
}

bool cli_service_adapter_apply_switches(CliServiceAdapterContext *context, bool switch_a_enabled,
                                        bool switch_b_enabled, bool switch_c_enabled)
{
    if (context == NULL || context->state == NULL)
    {
        return false;
    }

    lock_ctx(context);
    bool effective_a = context->switchOverrideEnabled[0] ? context->switchOverrideValue[0]
                                                         : switch_a_enabled;
    bool effective_b = context->switchOverrideEnabled[1] ? context->switchOverrideValue[1]
                                                         : switch_b_enabled;
    bool effective_c = context->switchOverrideEnabled[2] ? context->switchOverrideValue[2]
                                                         : switch_c_enabled;

    effects_state_apply_switches(context->state, effective_a, effective_b, effective_c);
    unlock_ctx(context);
    return true;
}

bool cli_service_adapter_get_test_input_mode_status(CliServiceAdapterContext *context,
                                                    CliTestModeStatus *status_out)
{
    if (context == NULL || status_out == NULL)
    {
        return false;
    }

    lock_ctx(context);
    status_out->enabled = context->testInputModeEnabled;
    status_out->vector = context->testInputVector;
    status_out->frequencyHz = context->testInputFrequencyHz;
    status_out->amplitude = context->testInputAmplitude;
    unlock_ctx(context);
    return true;
}

bool cli_service_adapter_get_test_output_mode_status(CliServiceAdapterContext *context,
                                                     CliTestModeStatus *status_out)
{
    if (context == NULL || status_out == NULL)
    {
        return false;
    }

    lock_ctx(context);
    status_out->enabled = context->testOutputModeEnabled;
    status_out->vector = context->testOutputVector;
    status_out->frequencyHz = context->testOutputFrequencyHz;
    status_out->amplitude = context->testOutputAmplitude;
    unlock_ctx(context);
    return true;
}

bool cli_service_adapter_get_log_stats(CliServiceAdapterContext *context,
                                       CliLogStats *stats_out)
{
    if (context == NULL || stats_out == NULL)
    {
        return false;
    }

    lock_ctx(context);
    stats_out->enabled = context->logEnabled;
    stats_out->streamEnabled = context->logStreamEnabled;
    stats_out->level = context->logLevel;
    stats_out->frameCount = context->logFrameCount;
    stats_out->failureCount = context->logFailureCount;
    stats_out->stepFailureCount = context->stepFailureCount;
    stats_out->stepFailureStreak = context->stepFailureStreak;
    stats_out->frameTimeMinUs = context->frameTimeMinUs;
    stats_out->frameTimeMaxUs = context->frameTimeMaxUs;
    stats_out->frameTimeTotalUs = context->frameTimeTotalUs;
    stats_out->measuredFrameCount = context->measuredFrameCount;
    stats_out->overrunCount = context->overrunCount;
    stats_out->streamDropCount = context->streamDropCount;
    stats_out->streamBatchSize = context->streamBatchSize;
    stats_out->streamQueueCount = context->logStreamCount;
    unlock_ctx(context);
    return true;
}

bool cli_service_adapter_get_log_stream_enabled(CliServiceAdapterContext *context,
                                                bool *enabled_out)
{
    if (context == NULL || enabled_out == NULL)
    {
        return false;
    }

    lock_ctx(context);
    *enabled_out = context->logStreamEnabled;
    unlock_ctx(context);
    return true;
}

bool cli_service_adapter_get_log_enabled(CliServiceAdapterContext *context, bool *enabled_out)
{
    if (context == NULL || enabled_out == NULL)
    {
        return false;
    }

    lock_ctx(context);
    *enabled_out = context->logEnabled;
    unlock_ctx(context);
    return true;
}

bool cli_service_adapter_get_log_level(CliServiceAdapterContext *context, uint8_t *level_out)
{
    if (context == NULL || level_out == NULL)
    {
        return false;
    }

    lock_ctx(context);
    *level_out = context->logLevel;
    unlock_ctx(context);
    return true;
}

bool cli_service_adapter_append_log_line(CliServiceAdapterContext *context, const char *line)
{
    if (context == NULL || line == NULL)
    {
        return false;
    }

    lock_ctx(context);
    const bool success = log_stream_queue_push(context, line);
    unlock_ctx(context);
    return success;
}

void cli_service_adapter_note_frame_processed(CliServiceAdapterContext *context)
{
    if (context == NULL)
    {
        return;
    }

    lock_ctx(context);
    context->logFrameCount++;
    unlock_ctx(context);
}

void cli_service_adapter_note_processing_failure(CliServiceAdapterContext *context)
{
    if (context == NULL)
    {
        return;
    }

    lock_ctx(context);
    context->logFailureCount++;
    char line[CLI_LOG_STREAM_LINE_MAX] = {0};
    (void)snprintf(line, sizeof(line), "log failure count=%lu",
                   (unsigned long)context->logFailureCount);
    (void)log_stream_queue_push(context, line);
    unlock_ctx(context);
}

void cli_service_adapter_note_step_result(CliServiceAdapterContext *context, bool step_succeeded)
{
    if (context == NULL)
    {
        return;
    }

    lock_ctx(context);
    if (step_succeeded)
    {
        context->stepFailureStreak = 0;
    }
    else
    {
        context->stepFailureCount++;
        context->stepFailureStreak++;
    }
    unlock_ctx(context);
}

void cli_service_adapter_set_stream_batch_size(CliServiceAdapterContext *context, uint8_t batch_size)
{
    if (context == NULL || batch_size == 0)
    {
        return;
    }

    lock_ctx(context);
    context->streamBatchSize = batch_size;
    unlock_ctx(context);
}

uint8_t cli_service_adapter_get_stream_batch_size(CliServiceAdapterContext *context)
{
    if (context == NULL)
    {
        return 0;
    }

    lock_ctx(context);
    uint8_t size = context->streamBatchSize;
    unlock_ctx(context);
    return size;
}

void cli_service_adapter_reset_profiling_stats(CliServiceAdapterContext *context)
{
    if (context == NULL)
    {
        return;
    }

    lock_ctx(context);
    context->frameTimeMinUs = 0;
    context->frameTimeMaxUs = 0;
    context->frameTimeTotalUs = 0;
    context->measuredFrameCount = 0;
    context->overrunCount = 0;
    context->stepFailureCount = 0;
    context->stepFailureStreak = 0;
    context->streamDropCount = 0;
    context->streamBatchCounter = 0;
    context->batchFrameTimeMinUs = 0;
    context->batchFrameTimeMaxUs = 0;
    context->batchFrameTimeTotalUs = 0;
    context->batchFrameCount = 0;
    context->batchOverrunCount = 0;
    unlock_ctx(context);
}

void cli_service_adapter_note_frame_timing(CliServiceAdapterContext *context, uint32_t frame_time_us,
                                           bool overrun)
{
    if (context == NULL)
    {
        return;
    }

    lock_ctx(context);
    if (context->measuredFrameCount == 0)
    {
        context->frameTimeMinUs = frame_time_us;
        context->frameTimeMaxUs = frame_time_us;
    }
    else
    {
        if (frame_time_us < context->frameTimeMinUs)
        {
            context->frameTimeMinUs = frame_time_us;
        }
        if (frame_time_us > context->frameTimeMaxUs)
        {
            context->frameTimeMaxUs = frame_time_us;
        }
    }
    context->frameTimeTotalUs += frame_time_us;
    context->measuredFrameCount++;
    if (overrun)
    {
        context->overrunCount++;
    }

    if (context->batchFrameCount == 0)
    {
        context->batchFrameTimeMinUs = frame_time_us;
        context->batchFrameTimeMaxUs = frame_time_us;
    }
    else
    {
        if (frame_time_us < context->batchFrameTimeMinUs)
        {
            context->batchFrameTimeMinUs = frame_time_us;
        }
        if (frame_time_us > context->batchFrameTimeMaxUs)
        {
            context->batchFrameTimeMaxUs = frame_time_us;
        }
    }
    context->batchFrameTimeTotalUs += frame_time_us;
    context->batchFrameCount++;
    if (overrun)
    {
        context->batchOverrunCount++;
    }
    context->streamBatchCounter++;

    if (context->streamBatchCounter >= context->streamBatchSize && context->logStreamEnabled)
    {
        uint32_t avg_us = context->batchFrameTimeTotalUs / context->batchFrameCount;
        char line[CLI_LOG_STREAM_LINE_MAX];
        (void)snprintf(line, sizeof(line),
                       "prof f=%lu t=%lu mn=%lu mx=%lu ov=%lu dr=%lu",
                       (unsigned long)context->batchFrameCount, (unsigned long)avg_us,
                       (unsigned long)context->batchFrameTimeMinUs,
                       (unsigned long)context->batchFrameTimeMaxUs,
                       (unsigned long)context->batchOverrunCount,
                       (unsigned long)context->streamDropCount);
        (void)log_stream_queue_push(context, line);

        context->streamBatchCounter = 0;
        context->batchFrameTimeMinUs = 0;
        context->batchFrameTimeMaxUs = 0;
        context->batchFrameTimeTotalUs = 0;
        context->batchFrameCount = 0;
        context->batchOverrunCount = 0;
    }

    unlock_ctx(context);
}