#include "cli_service_adapter.h"

#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define TEST_FREQ_MIN_HZ 20U
#define TEST_DEFAULT_FREQ_HZ 1000
#define MAX_TEST_VECTOR_INDEX 5U
#define ROM_RAW_MIN_ADDRESS 0x0000
#define ROM_RAW_MAX_ADDRESS 0x01FF

static void lock_ctx(CliServiceAdapter *context)
{
    if (context != NULL && context->ops.lock != NULL)
    {
        context->ops.lock(context->ops.context);
    }
}

static void unlock_ctx(CliServiceAdapter *context)
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

// Maps a "group.field" config key to byte offsets of the current value and its
// min/max bounds within EffectsParams, so a single getter/setter can serve all
// keys. PARAM_ENTRY composes the EffectsParams group offset with the field
// offset inside the parameter struct.
typedef struct
{
    const char *key;
    size_t value_offset;
    size_t min_offset;
    size_t max_offset;
} ParamMapping;

#define PARAM_ENTRY(key, group, group_min, group_max, group_type, field)            \
    {                                                                               \
        (key), offsetof(EffectsParams, group) + offsetof(group_type, field),        \
            offsetof(EffectsParams, group_min) + offsetof(group_type, field),       \
            offsetof(EffectsParams, group_max) + offsetof(group_type, field)        \
    }

#define OVERDRIVE_ENTRY(key, field) \
    PARAM_ENTRY(key, overdrive, overdrive_min, overdrive_max, OverdriveParam, field)
#define ECHO_ENTRY(key, field) PARAM_ENTRY(key, echo, echo_min, echo_max, EchoParam, field)
#define COMPRESSION_ENTRY(key, field) \
    PARAM_ENTRY(key, compression, compression_min, compression_max, CompressionParam, field)

static const ParamMapping PARAM_MAP[] = {
    OVERDRIVE_ENTRY("overdrive.gain", gain),
    OVERDRIVE_ENTRY("overdrive.level", level),
    OVERDRIVE_ENTRY("overdrive.tone", tone),
    OVERDRIVE_ENTRY("overdrive.mix", mix),
    ECHO_ENTRY("echo.delay_samples", delay_samples),
    ECHO_ENTRY("echo.pre_delay", pre_delay),
    ECHO_ENTRY("echo.density", density),
    ECHO_ENTRY("echo.attack", attack),
    ECHO_ENTRY("echo.decay", decay),
    COMPRESSION_ENTRY("compression.threshold", threshold),
    COMPRESSION_ENTRY("compression.ratio", ratio),
};

#undef PARAM_ENTRY
#undef OVERDRIVE_ENTRY
#undef ECHO_ENTRY
#undef COMPRESSION_ENTRY

static size_t param_value_at(const EffectsParams *params, size_t offset)
{
    return *(const size_t *)((const char *)params + offset);
}

static const ParamMapping *find_param_mapping(const char *key)
{
    for (size_t i = 0; i < sizeof(PARAM_MAP) / sizeof(PARAM_MAP[0]); i++)
    {
        if (strcmp(PARAM_MAP[i].key, key) == 0)
        {
            return &PARAM_MAP[i];
        }
    }
    return NULL;
}

static bool set_param_value(EffectsParams *params, const char *key, int32_t value)
{
    if (params == NULL || key == NULL)
    {
        return false;
    }

    const ParamMapping *mapping = find_param_mapping(key);
    if (mapping == NULL)
    {
        return false;
    }

    return assign_bounded_size_t(value, param_value_at(params, mapping->min_offset),
                                 param_value_at(params, mapping->max_offset),
                                 (size_t *)((char *)params + mapping->value_offset));
}

static bool get_param_value(const EffectsParams *params, const char *key, int32_t *value_out)
{
    if (params == NULL || key == NULL || value_out == NULL)
    {
        return false;
    }

    const ParamMapping *mapping = find_param_mapping(key);
    if (mapping == NULL)
    {
        return false;
    }

    size_t value = param_value_at(params, mapping->value_offset);
    if (value > (size_t)INT32_MAX)
    {
        return false;
    }

    *value_out = (int32_t)value;
    return true;
}

// "state.enable.*" keys all toggle one entry of EffectsState::is_enabled, so a
// table keyed by effect collapses the otherwise-identical handlers.
static const struct
{
    const char *key;
    Effect effect;
} STATE_ENABLE_MAP[] = {
    {"state.enable.overdrive", OVERDRIVE},
    {"state.enable.echo", ECHO},
    {"state.enable.compression", COMPRESSION},
};

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
        state->active_effect_selection = (uint8_t)value;
        return true;
    }

    for (size_t i = 0; i < sizeof(STATE_ENABLE_MAP) / sizeof(STATE_ENABLE_MAP[0]); i++)
    {
        if (strcmp(key, STATE_ENABLE_MAP[i].key) == 0)
        {
            if (value < 0 || value > 1)
            {
                return false;
            }
            state->is_enabled[STATE_ENABLE_MAP[i].effect] = value == 1;
            return true;
        }
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
        *value_out = state->active_effect_selection;
        return true;
    }

    for (size_t i = 0; i < sizeof(STATE_ENABLE_MAP) / sizeof(STATE_ENABLE_MAP[0]); i++)
    {
        if (strcmp(key, STATE_ENABLE_MAP[i].key) == 0)
        {
            *value_out = state->is_enabled[STATE_ENABLE_MAP[i].effect] ? 1 : 0;
            return true;
        }
    }

    return false;
}

static bool set_runtime_value(CliServiceAdapter *context, const char *key, int32_t value)
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

    if (context->heartbeat.period_ms == NULL)
    {
        return false;
    }

    uint32_t period_ms = (uint32_t)value;
    if (period_ms < context->heartbeat.period_min_ms || period_ms > context->heartbeat.period_max_ms)
    {
        return false;
    }

    *context->heartbeat.period_ms = period_ms;
    return true;
}

static bool get_runtime_value(const CliServiceAdapter *context,
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

    if (context->heartbeat.period_ms == NULL ||
        *context->heartbeat.period_ms > (uint32_t)INT32_MAX)
    {
        return false;
    }

    *value_out = (int32_t)(*context->heartbeat.period_ms);
    return true;
}

static bool in_rom_window(const CliServiceAdapter *context, uint16_t address, uint16_t length)
{
    if (context == NULL || length == 0 || length > context->rom_window.max_length)
    {
        return false;
    }

    if (address < context->rom_window.min_address || address > context->rom_window.max_address)
    {
        return false;
    }

    uint32_t end = (uint32_t)address + (uint32_t)length - 1U;
    return end <= context->rom_window.max_address;
}

static void log_stream_queue_clear(CliServiceAdapter *context)
{
    if (context == NULL)
    {
        return;
    }

    context->log_stream.head = 0;
    context->log_stream.tail = 0;
    context->log_stream.count = 0;
}

static bool log_stream_queue_push(CliServiceAdapter *context, const char *line)
{
    if (context == NULL || line == NULL)
    {
        return false;
    }

    if (context->log_stream.count >= CLI_LOG_STREAM_QUEUE_DEPTH)
    {
        context->log_stream.drop_count++;
        context->log_stream.head = (uint8_t)((context->log_stream.head + 1U) % CLI_LOG_STREAM_QUEUE_DEPTH);
        context->log_stream.count--;
    }

    (void)strncpy(context->log_stream.lines[context->log_stream.tail], line,
                  CLI_LOG_STREAM_LINE_MAX - 1U);
    context->log_stream.lines[context->log_stream.tail][CLI_LOG_STREAM_LINE_MAX - 1U] = '\0';
    context->log_stream.tail = (uint8_t)((context->log_stream.tail + 1U) % CLI_LOG_STREAM_QUEUE_DEPTH);
    context->log_stream.count++;
    return true;
}

static bool log_stream_queue_pop(CliServiceAdapter *context,
                                 char *line_out,
                                 size_t line_capacity,
                                 bool *has_line_out)
{
    if (context == NULL || line_out == NULL || line_capacity == 0 || has_line_out == NULL)
    {
        return false;
    }

    if (context->log_stream.count == 0)
    {
        *has_line_out = false;
        line_out[0] = '\0';
        return true;
    }

    (void)strncpy(line_out, context->log_stream.lines[context->log_stream.head], line_capacity - 1U);
    line_out[line_capacity - 1U] = '\0';

    context->log_stream.head = (uint8_t)((context->log_stream.head + 1U) % CLI_LOG_STREAM_QUEUE_DEPTH);
    context->log_stream.count--;
    *has_line_out = true;
    return true;
}

static bool service_set_pot_override(uint8_t pot_index, uint32_t value, void *ctx)
{
    CliServiceAdapter *context = (CliServiceAdapter *)ctx;
    if (context == NULL || pot_index >= CLI_POT_COUNT)
    {
        return false;
    }

    lock_ctx(context);
    context->overrides.pot_enabled[pot_index] = true;
    context->overrides.pot_value[pot_index] = value;
    unlock_ctx(context);
    return true;
}

static bool service_clear_pot_override(uint8_t pot_index, void *ctx)
{
    CliServiceAdapter *context = (CliServiceAdapter *)ctx;
    if (context == NULL || pot_index >= CLI_POT_COUNT)
    {
        return false;
    }

    lock_ctx(context);
    context->overrides.pot_enabled[pot_index] = false;
    unlock_ctx(context);
    return true;
}

static bool service_set_switch_override(uint8_t switch_index, bool enabled, void *ctx)
{
    CliServiceAdapter *context = (CliServiceAdapter *)ctx;
    if (context == NULL || switch_index >= CLI_SWITCH_COUNT)
    {
        return false;
    }

    lock_ctx(context);
    context->overrides.switch_enabled[switch_index] = true;
    context->overrides.switch_value[switch_index] = enabled;
    unlock_ctx(context);
    return true;
}

static bool service_clear_switch_override(uint8_t switch_index, void *ctx)
{
    CliServiceAdapter *context = (CliServiceAdapter *)ctx;
    if (context == NULL || switch_index >= CLI_SWITCH_COUNT)
    {
        return false;
    }

    lock_ctx(context);
    context->overrides.switch_enabled[switch_index] = false;
    unlock_ctx(context);
    return true;
}

static bool service_clear_all_overrides(void *ctx)
{
    CliServiceAdapter *context = (CliServiceAdapter *)ctx;
    if (context == NULL)
    {
        return false;
    }

    lock_ctx(context);
    memset(context->overrides.pot_enabled, 0, sizeof(context->overrides.pot_enabled));
    memset(context->overrides.switch_enabled, 0, sizeof(context->overrides.switch_enabled));
    unlock_ctx(context);
    return true;
}

static bool service_config_set(const char *key, int32_t value, void *ctx)
{
    CliServiceAdapter *context = (CliServiceAdapter *)ctx;
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
    CliServiceAdapter *context = (CliServiceAdapter *)ctx;
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
    CliServiceAdapter *context = (CliServiceAdapter *)ctx;
    return context != NULL && context->ops.rom_save_state != NULL &&
           context->ops.rom_save_state(context->ops.context);
}

static bool service_rom_load_state(void *ctx)
{
    CliServiceAdapter *context = (CliServiceAdapter *)ctx;
    return context != NULL && context->ops.rom_load_state != NULL &&
           context->ops.rom_load_state(context->ops.context);
}

static bool service_rom_read_raw(uint16_t address, uint16_t length, uint8_t *payload_out,
                                 size_t payload_capacity, size_t *payload_size_out, void *ctx)
{
    CliServiceAdapter *context = (CliServiceAdapter *)ctx;

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
    CliServiceAdapter *context = (CliServiceAdapter *)ctx;

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

static bool service_i2c_ping(uint8_t address, void *ctx)
{
    CliServiceAdapter *context = (CliServiceAdapter *)ctx;
    return context != NULL && context->ops.i2c_ping != NULL &&
           context->ops.i2c_ping(address, context->ops.context);
}

static bool service_i2c_transfer(uint8_t address, const uint8_t *tx_data, size_t tx_len,
                                 uint8_t *rx_data, size_t *rx_len, uint32_t timeout_ms,
                                 void *ctx)
{
    CliServiceAdapter *context = (CliServiceAdapter *)ctx;
    return context != NULL && context->ops.i2c_transfer != NULL &&
           context->ops.i2c_transfer(address, tx_data, tx_len, rx_data, rx_len, timeout_ms,
                                     context->ops.context);
}

static bool service_i2c_scan(uint8_t start_addr, uint8_t end_addr,
                             uint8_t *found_addrs, size_t *count,
                             size_t max_count, void *ctx)
{
    CliServiceAdapter *context = (CliServiceAdapter *)ctx;
    return context != NULL && context->ops.i2c_scan != NULL &&
           context->ops.i2c_scan(start_addr, end_addr, found_addrs, count, max_count,
                                 context->ops.context);
}

static bool service_log_set_level(uint8_t level, void *ctx)
{
    CliServiceAdapter *context = (CliServiceAdapter *)ctx;
    if (context == NULL)
    {
        return false;
    }

    lock_ctx(context);
    context->log.level = level;
    unlock_ctx(context);

    if (context->ops.log_set_level != NULL)
    {
        return context->ops.log_set_level(level, context->ops.context);
    }

    return true;
}

static bool service_log_set_enabled(bool enabled, void *ctx)
{
    CliServiceAdapter *context = (CliServiceAdapter *)ctx;
    if (context == NULL)
    {
        return false;
    }

    lock_ctx(context);
    context->log.enabled = enabled;
    unlock_ctx(context);

    if (context->ops.log_set_enabled != NULL)
    {
        return context->ops.log_set_enabled(enabled, context->ops.context);
    }

    return true;
}

static bool service_log_set_stream(bool enabled, void *ctx)
{
    CliServiceAdapter *context = (CliServiceAdapter *)ctx;
    if (context == NULL)
    {
        return false;
    }

    lock_ctx(context);
    context->log.enabled = enabled;
    context->log.stream_enabled = enabled;
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
    CliServiceAdapter *context = (CliServiceAdapter *)ctx;
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
    CliServiceAdapter *context = (CliServiceAdapter *)ctx;
    if (context == NULL || batch_size == 0)
    {
        return false;
    }

    lock_ctx(context);
    context->stream_batch_size = batch_size;
    unlock_ctx(context);
    return true;
}

static bool service_log_reset_stats(void *ctx)
{
    CliServiceAdapter *context = (CliServiceAdapter *)ctx;
    if (context == NULL)
    {
        return false;
    }

    cli_service_adapter_reset_profiling_stats(context);
    return true;
}

static bool service_log_get_stats(CliLogStats *stats_out, void *ctx)
{
    CliServiceAdapter *context = (CliServiceAdapter *)ctx;
    if (context == NULL || stats_out == NULL)
    {
        return false;
    }

    lock_ctx(context);
    stats_out->enabled = context->log.enabled;
    stats_out->stream_enabled = context->log.stream_enabled;
    stats_out->level = context->log.level;
    stats_out->frame_count = context->log.frame_count;
    stats_out->failure_count = context->log.failure_count;
    stats_out->step_failure_count = context->step_failure_count;
    stats_out->step_failure_streak = context->step_failure_streak;
    stats_out->frame_time_min_us = context->profile.frame_time_min_us;
    stats_out->frame_time_max_us = context->profile.frame_time_max_us;
    stats_out->frame_time_total_us = context->profile.frame_time_total_us;
    stats_out->measured_frame_count = context->profile.frame_count;
    stats_out->overrun_count = context->profile.overrun_count;
    stats_out->stream_drop_count = context->log_stream.drop_count;
    stats_out->stream_batch_size = context->stream_batch_size;
    stats_out->stream_queue_count = context->log_stream.count;
    unlock_ctx(context);

    return true;
}

static bool service_test_set_input_mode(bool enabled, void *ctx)
{
    CliServiceAdapter *context = (CliServiceAdapter *)ctx;
    if (context == NULL)
    {
        return false;
    }

    lock_ctx(context);
    context->test_input.enabled = enabled;
    unlock_ctx(context);

    if (context->ops.test_set_input_mode != NULL)
    {
        return context->ops.test_set_input_mode(enabled, context->ops.context);
    }

    return true;
}

static bool service_test_set_input_vector(uint8_t vector, void *ctx)
{
    CliServiceAdapter *context = (CliServiceAdapter *)ctx;
    if (context == NULL || vector > MAX_TEST_VECTOR_INDEX)
    {
        return false;
    }

    lock_ctx(context);
    context->test_input.vector = vector;
    unlock_ctx(context);

    if (context->ops.test_set_input_vector != NULL)
    {
        return context->ops.test_set_input_vector(vector, context->ops.context);
    }

    return true;
}

static bool service_test_set_input_frequency_hz(uint16_t frequency_hz, void *ctx)
{
    CliServiceAdapter *context = (CliServiceAdapter *)ctx;
    if (context == NULL || frequency_hz < TEST_FREQ_MIN_HZ)
    {
        return false;
    }

    lock_ctx(context);
    context->test_input.frequency_hz = frequency_hz;
    unlock_ctx(context);

    if (context->ops.test_set_input_frequency_hz != NULL)
    {
        return context->ops.test_set_input_frequency_hz(frequency_hz, context->ops.context);
    }

    return true;
}

static bool service_test_set_input_amplitude(uint16_t amplitude, void *ctx)
{
    CliServiceAdapter *context = (CliServiceAdapter *)ctx;
    if (context == NULL || amplitude > X_AXIS)
    {
        return false;
    }

    lock_ctx(context);
    context->test_input.amplitude = amplitude;
    unlock_ctx(context);

    if (context->ops.test_set_input_amplitude != NULL)
    {
        return context->ops.test_set_input_amplitude(amplitude, context->ops.context);
    }

    return true;
}

static bool service_test_get_input_status(CliTestModeStatus *status_out, void *ctx)
{
    CliServiceAdapter *context = (CliServiceAdapter *)ctx;
    if (context == NULL || status_out == NULL)
    {
        return false;
    }

    lock_ctx(context);
    status_out->enabled = context->test_input.enabled;
    status_out->vector = context->test_input.vector;
    status_out->frequency_hz = context->test_input.frequency_hz;
    status_out->amplitude = context->test_input.amplitude;
    unlock_ctx(context);

    return true;
}

static bool service_test_set_output_mode(bool enabled, void *ctx)
{
    CliServiceAdapter *context = (CliServiceAdapter *)ctx;
    if (context == NULL)
    {
        return false;
    }

    lock_ctx(context);
    context->test_output.enabled = enabled;
    unlock_ctx(context);

    if (context->ops.test_set_output_mode != NULL)
    {
        return context->ops.test_set_output_mode(enabled, context->ops.context);
    }

    return true;
}

static bool service_test_set_output_vector(uint8_t vector, void *ctx)
{
    CliServiceAdapter *context = (CliServiceAdapter *)ctx;
    if (context == NULL || vector > MAX_TEST_VECTOR_INDEX)
    {
        return false;
    }

    lock_ctx(context);
    context->test_output.vector = vector;
    unlock_ctx(context);

    if (context->ops.test_set_output_vector != NULL)
    {
        return context->ops.test_set_output_vector(vector, context->ops.context);
    }

    return true;
}

static bool service_test_set_output_frequency_hz(uint16_t frequency_hz, void *ctx)
{
    CliServiceAdapter *context = (CliServiceAdapter *)ctx;
    if (context == NULL || frequency_hz < TEST_FREQ_MIN_HZ)
    {
        return false;
    }

    lock_ctx(context);
    context->test_output.frequency_hz = frequency_hz;
    unlock_ctx(context);

    if (context->ops.test_set_output_frequency_hz != NULL)
    {
        return context->ops.test_set_output_frequency_hz(frequency_hz, context->ops.context);
    }

    return true;
}

static bool service_test_set_output_amplitude(uint16_t amplitude, void *ctx)
{
    CliServiceAdapter *context = (CliServiceAdapter *)ctx;
    if (context == NULL || amplitude > X_AXIS)
    {
        return false;
    }

    lock_ctx(context);
    context->test_output.amplitude = amplitude;
    unlock_ctx(context);

    if (context->ops.test_set_output_amplitude != NULL)
    {
        return context->ops.test_set_output_amplitude(amplitude, context->ops.context);
    }

    return true;
}

static bool service_test_get_output_status(CliTestModeStatus *status_out, void *ctx)
{
    CliServiceAdapter *context = (CliServiceAdapter *)ctx;
    if (context == NULL || status_out == NULL)
    {
        return false;
    }

    lock_ctx(context);
    status_out->enabled = context->test_output.enabled;
    status_out->vector = context->test_output.vector;
    status_out->frequency_hz = context->test_output.frequency_hz;
    status_out->amplitude = context->test_output.amplitude;
    unlock_ctx(context);

    return true;
}

static bool service_system_reboot(void *ctx)
{
    CliServiceAdapter *context = (CliServiceAdapter *)ctx;
    if (context == NULL || context->ops.system_reboot == NULL)
    {
        return false;
    }

    context->ops.system_reboot(context->ops.context);
    return true;
}

static bool service_audio_get_info(CliAudioStatus *status_out, void *ctx)
{
    CliServiceAdapter *context = (CliServiceAdapter *)ctx;
    if (context == NULL || status_out == NULL)
    {
        return false;
    }

    lock_ctx(context);
    status_out->sample_rate_hz = context->sample_rate_hz;
    status_out->sample_buf_len = context->sample_buf_len;
    status_out->sampling_period_us = context->sampling_period_us;
    status_out->delay_samples_len = context->delay_samples_len;
    status_out->processing_slack_ms = context->processing_slack_ms;
    unlock_ctx(context);

    return true;
}

void cli_service_adapter_init(CliServiceAdapter *context, EffectsState *state,
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
    context->adc_max = adc_max;
    context->rom_window.min_address = ROM_RAW_MIN_ADDRESS;
    context->rom_window.max_address = ROM_RAW_MAX_ADDRESS;
    context->rom_window.max_length = CLI_MAX_ROM_BYTES;
    context->log.enabled = false;
    context->log.stream_enabled = false;
    context->log.level = 0;
    context->log.frame_count = 0;
    context->log.failure_count = 0;
    context->test_input.enabled = false;
    context->test_input.vector = 0;
    context->test_input.frequency_hz = TEST_DEFAULT_FREQ_HZ;
    context->test_input.amplitude = (uint16_t)(X_AXIS / 2U);

    context->test_output.enabled = false;
    context->test_output.vector = 0;
    context->test_output.frequency_hz = TEST_DEFAULT_FREQ_HZ;
    context->test_output.amplitude = (uint16_t)(X_AXIS / 2U);

    context->profile.frame_time_min_us = 0;
    context->profile.frame_time_max_us = 0;
    context->profile.frame_time_total_us = 0;
    context->profile.frame_count = 0;
    context->profile.overrun_count = 0;
    context->log_stream.drop_count = 0;
    context->stream_batch_size = 1;
    context->step_failure_count = 0;
    context->step_failure_streak = 0;
    context->stream_batch_counter = 0;
    context->profile_batch.frame_time_min_us = 0;
    context->profile_batch.frame_time_max_us = 0;
    context->profile_batch.frame_time_total_us = 0;
    context->profile_batch.frame_count = 0;
    context->profile_batch.overrun_count = 0;

    if (ops != NULL)
    {
        context->ops = *ops;
    }
}

void cli_service_adapter_bind_heartbeat_period(CliServiceAdapter *context,
                                               uint32_t *period_ms,
                                               uint32_t min_ms,
                                               uint32_t max_ms)
{
    if (context == NULL || period_ms == NULL || min_ms > max_ms)
    {
        return;
    }

    context->heartbeat.period_ms = period_ms;
    context->heartbeat.period_min_ms = min_ms;
    context->heartbeat.period_max_ms = max_ms;
}

static bool service_audio_set_input(uint8_t source, void *ctx);
static bool service_audio_set_output(bool enabled, void *ctx);
static bool service_audio_get_status(uint8_t *source_out,
                                     bool *output_enabled_out, void *ctx);

void cli_service_adapter_bind(CliServiceAdapter *context, CliServices *services_out)
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
    services_out->i2c_ping = service_i2c_ping;
    services_out->i2c_transfer = service_i2c_transfer;
    services_out->i2c_scan = service_i2c_scan;
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
    services_out->system_reboot = service_system_reboot;
    services_out->audio_set_input = service_audio_set_input;
    services_out->audio_set_output = service_audio_set_output;
    services_out->audio_get_status = service_audio_get_status;
    services_out->audio_get_info = service_audio_get_info;
    services_out->context = context;
}

bool cli_service_adapter_apply_pot_sample(CliServiceAdapter *context, Effect active_effect,
                                          uint8_t pot_index, uint32_t adc_value)
{
    if (context == NULL || context->params == NULL || pot_index >= CLI_POT_COUNT)
    {
        return false;
    }

    lock_ctx(context);
    uint32_t effective_adc = adc_value;
    if (context->overrides.pot_enabled[pot_index])
    {
        effective_adc = context->overrides.pot_value[pot_index];
    }

    bool success = effects_params_apply_pot_sample(context->params, active_effect, pot_index,
                                                   effective_adc, context->adc_max);
    unlock_ctx(context);
    return success;
}

bool cli_service_adapter_apply_switches(CliServiceAdapter *context, bool switch_a_enabled,
                                        bool switch_b_enabled, bool switch_c_enabled)
{
    if (context == NULL || context->state == NULL)
    {
        return false;
    }

    lock_ctx(context);
    const CliOverrideState *ov = &context->overrides;
    bool effective_a = ov->switch_enabled[0] ? ov->switch_value[0] : switch_a_enabled;
    bool effective_b = ov->switch_enabled[1] ? ov->switch_value[1] : switch_b_enabled;
    bool effective_c = ov->switch_enabled[2] ? ov->switch_value[2] : switch_c_enabled;

    effects_state_apply_switches(context->state, effective_a, effective_b, effective_c);
    unlock_ctx(context);
    return true;
}

bool cli_service_adapter_get_test_input_mode_status(CliServiceAdapter *context,
                                                    CliTestModeStatus *status_out)
{
    if (context == NULL || status_out == NULL)
    {
        return false;
    }

    lock_ctx(context);
    status_out->enabled = context->test_input.enabled;
    status_out->vector = context->test_input.vector;
    status_out->frequency_hz = context->test_input.frequency_hz;
    status_out->amplitude = context->test_input.amplitude;
    unlock_ctx(context);
    return true;
}

bool cli_service_adapter_get_test_output_mode_status(CliServiceAdapter *context,
                                                     CliTestModeStatus *status_out)
{
    if (context == NULL || status_out == NULL)
    {
        return false;
    }

    lock_ctx(context);
    status_out->enabled = context->test_output.enabled;
    status_out->vector = context->test_output.vector;
    status_out->frequency_hz = context->test_output.frequency_hz;
    status_out->amplitude = context->test_output.amplitude;
    unlock_ctx(context);
    return true;
}

bool cli_service_adapter_get_log_stats(CliServiceAdapter *context,
                                       CliLogStats *stats_out)
{
    if (context == NULL || stats_out == NULL)
    {
        return false;
    }

    lock_ctx(context);
    stats_out->enabled = context->log.enabled;
    stats_out->stream_enabled = context->log.stream_enabled;
    stats_out->level = context->log.level;
    stats_out->frame_count = context->log.frame_count;
    stats_out->failure_count = context->log.failure_count;
    stats_out->step_failure_count = context->step_failure_count;
    stats_out->step_failure_streak = context->step_failure_streak;
    stats_out->frame_time_min_us = context->profile.frame_time_min_us;
    stats_out->frame_time_max_us = context->profile.frame_time_max_us;
    stats_out->frame_time_total_us = context->profile.frame_time_total_us;
    stats_out->measured_frame_count = context->profile.frame_count;
    stats_out->overrun_count = context->profile.overrun_count;
    stats_out->stream_drop_count = context->log_stream.drop_count;
    stats_out->stream_batch_size = context->stream_batch_size;
    stats_out->stream_queue_count = context->log_stream.count;
    unlock_ctx(context);
    return true;
}

bool cli_service_adapter_get_log_stream_enabled(CliServiceAdapter *context,
                                                bool *enabled_out)
{
    if (context == NULL || enabled_out == NULL)
    {
        return false;
    }

    lock_ctx(context);
    *enabled_out = context->log.stream_enabled;
    unlock_ctx(context);
    return true;
}

bool cli_service_adapter_get_log_enabled(CliServiceAdapter *context, bool *enabled_out)
{
    if (context == NULL || enabled_out == NULL)
    {
        return false;
    }

    lock_ctx(context);
    *enabled_out = context->log.enabled;
    unlock_ctx(context);
    return true;
}

bool cli_service_adapter_get_log_level(CliServiceAdapter *context, uint8_t *level_out)
{
    if (context == NULL || level_out == NULL)
    {
        return false;
    }

    lock_ctx(context);
    *level_out = context->log.level;
    unlock_ctx(context);
    return true;
}

bool cli_service_adapter_append_log_line(CliServiceAdapter *context, const char *line)
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

void cli_service_adapter_note_frame_processed(CliServiceAdapter *context)
{
    if (context == NULL)
    {
        return;
    }

    lock_ctx(context);
    context->log.frame_count++;
    unlock_ctx(context);
}

void cli_service_adapter_note_processing_failure(CliServiceAdapter *context)
{
    if (context == NULL)
    {
        return;
    }

    lock_ctx(context);
    context->log.failure_count++;
    char line[CLI_LOG_STREAM_LINE_MAX] = {0};
    (void)snprintf(line, sizeof(line), "log failure count=%lu",
                   (unsigned long)context->log.failure_count);
    (void)log_stream_queue_push(context, line);
    unlock_ctx(context);
}

void cli_service_adapter_note_step_result(CliServiceAdapter *context, bool step_succeeded)
{
    if (context == NULL)
    {
        return;
    }

    lock_ctx(context);
    if (step_succeeded)
    {
        context->step_failure_streak = 0;
    }
    else
    {
        context->step_failure_count++;
        context->step_failure_streak++;
    }
    unlock_ctx(context);
}

void cli_service_adapter_set_stream_batch_size(CliServiceAdapter *context, uint8_t batch_size)
{
    if (context == NULL || batch_size == 0)
    {
        return;
    }

    lock_ctx(context);
    context->stream_batch_size = batch_size;
    unlock_ctx(context);
}

uint8_t cli_service_adapter_get_stream_batch_size(CliServiceAdapter *context)
{
    if (context == NULL)
    {
        return 0;
    }

    lock_ctx(context);
    uint8_t size = context->stream_batch_size;
    unlock_ctx(context);
    return size;
}

void cli_service_adapter_set_audio_info(CliServiceAdapter *context,
                                        uint32_t sample_rate_hz,
                                        uint32_t sample_buf_len,
                                        uint32_t sampling_period_us,
                                        uint32_t delay_samples_len,
                                        uint32_t processing_slack_ms)
{
    if (context == NULL)
    {
        return;
    }

    lock_ctx(context);
    context->sample_rate_hz = sample_rate_hz;
    context->sample_buf_len = sample_buf_len;
    context->sampling_period_us = sampling_period_us;
    context->delay_samples_len = delay_samples_len;
    context->processing_slack_ms = processing_slack_ms;
    unlock_ctx(context);
}

void cli_service_adapter_reset_profiling_stats(CliServiceAdapter *context)
{
    if (context == NULL)
    {
        return;
    }

    lock_ctx(context);
    context->profile.frame_time_min_us = 0;
    context->profile.frame_time_max_us = 0;
    context->profile.frame_time_total_us = 0;
    context->profile.frame_count = 0;
    context->profile.overrun_count = 0;
    context->step_failure_count = 0;
    context->step_failure_streak = 0;
    context->log_stream.drop_count = 0;
    context->stream_batch_counter = 0;
    context->profile_batch.frame_time_min_us = 0;
    context->profile_batch.frame_time_max_us = 0;
    context->profile_batch.frame_time_total_us = 0;
    context->profile_batch.frame_count = 0;
    context->profile_batch.overrun_count = 0;
    unlock_ctx(context);
}

void cli_service_adapter_note_frame_timing(CliServiceAdapter *context, uint32_t frame_time_us,
                                           bool overrun)
{
    if (context == NULL)
    {
        return;
    }

    lock_ctx(context);
    if (context->profile.frame_count == 0)
    {
        context->profile.frame_time_min_us = frame_time_us;
        context->profile.frame_time_max_us = frame_time_us;
    }
    else
    {
        if (frame_time_us < context->profile.frame_time_min_us)
        {
            context->profile.frame_time_min_us = frame_time_us;
        }
        if (frame_time_us > context->profile.frame_time_max_us)
        {
            context->profile.frame_time_max_us = frame_time_us;
        }
    }
    context->profile.frame_time_total_us += frame_time_us;
    context->profile.frame_count++;
    if (overrun)
    {
        context->profile.overrun_count++;
    }

    if (context->profile_batch.frame_count == 0)
    {
        context->profile_batch.frame_time_min_us = frame_time_us;
        context->profile_batch.frame_time_max_us = frame_time_us;
    }
    else
    {
        if (frame_time_us < context->profile_batch.frame_time_min_us)
        {
            context->profile_batch.frame_time_min_us = frame_time_us;
        }
        if (frame_time_us > context->profile_batch.frame_time_max_us)
        {
            context->profile_batch.frame_time_max_us = frame_time_us;
        }
    }
    context->profile_batch.frame_time_total_us += frame_time_us;
    context->profile_batch.frame_count++;
    if (overrun)
    {
        context->profile_batch.overrun_count++;
    }
    context->stream_batch_counter++;

    if (context->stream_batch_counter >= context->stream_batch_size && context->log.stream_enabled)
    {
        uint32_t avg_us = context->profile_batch.frame_time_total_us / context->profile_batch.frame_count;
        char line[CLI_LOG_STREAM_LINE_MAX];
        (void)snprintf(line, sizeof(line),
                       "prof f=%lu t=%lu mn=%lu mx=%lu ov=%lu dr=%lu",
                       (unsigned long)context->profile_batch.frame_count, (unsigned long)avg_us,
                       (unsigned long)context->profile_batch.frame_time_min_us,
                       (unsigned long)context->profile_batch.frame_time_max_us,
                       (unsigned long)context->profile_batch.overrun_count,
                       (unsigned long)context->log_stream.drop_count);
        (void)log_stream_queue_push(context, line);

        context->stream_batch_counter = 0;
        context->profile_batch.frame_time_min_us = 0;
        context->profile_batch.frame_time_max_us = 0;
        context->profile_batch.frame_time_total_us = 0;
        context->profile_batch.frame_count = 0;
        context->profile_batch.overrun_count = 0;
    }

    unlock_ctx(context);
}

void cli_service_adapter_set_audio_stream(CliServiceAdapter *context,
                                          UsbAudioStream *stream)
{
    if (context != NULL)
    {
        lock_ctx(context);
        context->audio_stream = stream;
        unlock_ctx(context);
    }
}

static bool service_audio_set_input(uint8_t source, void *ctx)
{
    CliServiceAdapter *c = (CliServiceAdapter *)ctx;
    if (c == NULL)
    {
        return false;
    }

    lock_ctx(c);
    if (source == 0U)
    {
        c->test_input.enabled = false;
    }
    else
    {
        c->test_input.enabled = true;
        c->test_input.vector = 5U;
    }
    unlock_ctx(c);
    return true;
}

static bool service_audio_set_output(bool enabled, void *ctx)
{
    CliServiceAdapter *c = (CliServiceAdapter *)ctx;
    if (c == NULL)
    {
        return false;
    }

    lock_ctx(c);
    if (c->audio_stream != NULL)
    {
        c->audio_stream->output_active = enabled;
    }
    unlock_ctx(c);
    return true;
}

static bool service_audio_get_status(uint8_t *source_out,
                                     bool *output_enabled_out,
                                     void *ctx)
{
    CliServiceAdapter *c = (CliServiceAdapter *)ctx;
    if (c == NULL || source_out == NULL || output_enabled_out == NULL)
    {
        return false;
    }

    lock_ctx(c);
    *source_out =
        (c->test_input.enabled && c->test_input.vector == 5U) ? 1U : 0U;
    *output_enabled_out =
        (c->audio_stream != NULL) ? c->audio_stream->output_active : false;
    unlock_ctx(c);
    return true;
}