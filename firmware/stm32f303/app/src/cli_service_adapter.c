#include "cli_service_adapter.h"

#include "usb/audio_stream.h"

#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define TEST_FREQ_MIN_HZ 20U
#define TEST_DEFAULT_FREQ_HZ 1000
#define MAX_TEST_VECTOR_INDEX 6U
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

static size_t param_value_at(const EffectsParams *params, size_t offset)
{
    return *(const size_t *)((const char *)params + offset);
}

static bool set_param_value(EffectsParams *params, const char *key, int32_t value)
{
    if (params == NULL || key == NULL)
    {
        return false;
    }

    const EffectParamMeta *meta = effect_params_meta_find(key);
    if (meta == NULL)
    {
        return false;
    }

    return assign_bounded_size_t(value, meta->min, meta->max,
                                 (size_t *)((char *)params + meta->offset));
}

static bool get_param_value(const EffectsParams *params, const char *key, int32_t *value_out)
{
    if (params == NULL || key == NULL || value_out == NULL)
    {
        return false;
    }

    const EffectParamMeta *meta = effect_params_meta_find(key);
    if (meta == NULL)
    {
        return false;
    }

    size_t value = param_value_at(params, meta->offset);
    if (value > (size_t)INT32_MAX)
    {
        return false;
    }

    *value_out = (int32_t)value;
    return true;
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
    success = set_param_value(context->params, key, value) ||
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
    success = get_param_value(context->params, key, value_out) ||
              get_runtime_value(context, key, value_out);
    unlock_ctx(context);

    return success;
}

static bool service_slot_get(uint8_t *slots_out, uint8_t *enabled_out, size_t capacity,
                             size_t *count_out, void *ctx)
{
    CliServiceAdapter *context = (CliServiceAdapter *)ctx;

    if (context == NULL || context->state == NULL || slots_out == NULL || enabled_out == NULL ||
        count_out == NULL || capacity < NUM_SLOTS)
    {
        return false;
    }

    lock_ctx(context);
    for (size_t i = 0; i < NUM_SLOTS; i++)
    {
        slots_out[i] = context->state->slots[i];
        enabled_out[i] = context->state->slot_enabled[i] ? 1U : 0U;
    }
    *count_out = NUM_SLOTS;
    unlock_ctx(context);

    return true;
}

static bool service_slot_assign(uint8_t pos, uint8_t effect, void *ctx)
{
    CliServiceAdapter *context = (CliServiceAdapter *)ctx;
    if (context == NULL || context->state == NULL)
    {
        return false;
    }

    lock_ctx(context);
    bool success = effects_state_assign_slot(context->state, pos, (EffectType)effect);
    unlock_ctx(context);
    return success;
}

static bool service_slot_clear(uint8_t pos, void *ctx)
{
    CliServiceAdapter *context = (CliServiceAdapter *)ctx;
    if (context == NULL || context->state == NULL)
    {
        return false;
    }

    lock_ctx(context);
    bool success = effects_state_clear_slot(context->state, pos);
    unlock_ctx(context);
    return success;
}

static bool service_slot_swap(uint8_t a, uint8_t b, void *ctx)
{
    CliServiceAdapter *context = (CliServiceAdapter *)ctx;
    if (context == NULL || context->state == NULL)
    {
        return false;
    }

    lock_ctx(context);
    bool success = effects_state_swap_slots(context->state, a, b);
    unlock_ctx(context);
    return success;
}

static bool service_slot_set_enabled(uint8_t pos, bool enabled, void *ctx)
{
    CliServiceAdapter *context = (CliServiceAdapter *)ctx;
    if (context == NULL || context->state == NULL)
    {
        return false;
    }

    lock_ctx(context);
    bool success = effects_state_set_slot_enabled(context->state, pos, enabled);
    unlock_ctx(context);
    return success;
}

static bool service_slot_get_active(uint8_t *pos_out, void *ctx)
{
    CliServiceAdapter *context = (CliServiceAdapter *)ctx;
    if (context == NULL || context->state == NULL || pos_out == NULL)
    {
        return false;
    }

    lock_ctx(context);
    *pos_out = context->state->active_slot;
    unlock_ctx(context);
    return true;
}

static bool service_slot_set_active(uint8_t pos, void *ctx)
{
    CliServiceAdapter *context = (CliServiceAdapter *)ctx;
    if (context == NULL || context->state == NULL)
    {
        return false;
    }

    lock_ctx(context);
    bool success = effects_state_set_active_slot(context->state, pos);
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
    return cli_service_adapter_get_log_stats((CliServiceAdapter *)ctx, stats_out);
}

static bool test_set_mode(CliServiceAdapter *context, CliTestModeState *slot, bool enabled,
                          bool (*op)(bool, void *))
{
    lock_ctx(context);
    slot->enabled = enabled;
    unlock_ctx(context);
    return op == NULL ? true : op(enabled, context->ops.context);
}

static bool test_set_vector(CliServiceAdapter *context, CliTestModeState *slot, uint8_t vector,
                            bool (*op)(uint8_t, void *))
{
    if (vector > MAX_TEST_VECTOR_INDEX)
    {
        return false;
    }

    lock_ctx(context);
    slot->vector = vector;
    unlock_ctx(context);
    return op == NULL ? true : op(vector, context->ops.context);
}

static bool test_set_frequency(CliServiceAdapter *context, CliTestModeState *slot,
                               uint16_t frequency_hz, bool (*op)(uint16_t, void *))
{
    if (frequency_hz < TEST_FREQ_MIN_HZ)
    {
        return false;
    }

    lock_ctx(context);
    slot->frequency_hz = frequency_hz;
    unlock_ctx(context);
    return op == NULL ? true : op(frequency_hz, context->ops.context);
}

static bool test_set_amplitude(CliServiceAdapter *context, CliTestModeState *slot,
                               uint16_t amplitude, bool (*op)(uint16_t, void *))
{
    if (amplitude > X_AXIS)
    {
        return false;
    }

    lock_ctx(context);
    slot->amplitude = amplitude;
    unlock_ctx(context);
    return op == NULL ? true : op(amplitude, context->ops.context);
}

static bool test_get_status(CliServiceAdapter *context, bool is_output,
                            CliTestModeStatus *status_out)
{
    if (context == NULL || status_out == NULL)
    {
        return false;
    }

    const CliTestModeState *slot = is_output ? &context->test_output : &context->test_input;
    lock_ctx(context);
    status_out->enabled = slot->enabled;
    status_out->vector = slot->vector;
    status_out->frequency_hz = slot->frequency_hz;
    status_out->amplitude = slot->amplitude;
    unlock_ctx(context);

    return true;
}

static bool service_test_set_input_mode(bool enabled, void *ctx)
{
    CliServiceAdapter *context = (CliServiceAdapter *)ctx;
    return context != NULL &&
           test_set_mode(context, &context->test_input, enabled, context->ops.test_set_input_mode);
}

static bool service_test_set_input_vector(uint8_t vector, void *ctx)
{
    CliServiceAdapter *context = (CliServiceAdapter *)ctx;
    return context != NULL && test_set_vector(context, &context->test_input, vector,
                                              context->ops.test_set_input_vector);
}

static bool service_test_set_input_frequency_hz(uint16_t frequency_hz, void *ctx)
{
    CliServiceAdapter *context = (CliServiceAdapter *)ctx;
    return context != NULL && test_set_frequency(context, &context->test_input, frequency_hz,
                                                 context->ops.test_set_input_frequency_hz);
}

static bool service_test_set_input_amplitude(uint16_t amplitude, void *ctx)
{
    CliServiceAdapter *context = (CliServiceAdapter *)ctx;
    return context != NULL && test_set_amplitude(context, &context->test_input, amplitude,
                                                 context->ops.test_set_input_amplitude);
}

static bool service_test_get_input_status(CliTestModeStatus *status_out, void *ctx)
{
    return test_get_status((CliServiceAdapter *)ctx, false, status_out);
}

static bool service_test_set_output_mode(bool enabled, void *ctx)
{
    CliServiceAdapter *context = (CliServiceAdapter *)ctx;
    return context != NULL && test_set_mode(context, &context->test_output, enabled,
                                            context->ops.test_set_output_mode);
}

static bool service_test_set_output_vector(uint8_t vector, void *ctx)
{
    CliServiceAdapter *context = (CliServiceAdapter *)ctx;
    return context != NULL && test_set_vector(context, &context->test_output, vector,
                                              context->ops.test_set_output_vector);
}

static bool service_test_set_output_frequency_hz(uint16_t frequency_hz, void *ctx)
{
    CliServiceAdapter *context = (CliServiceAdapter *)ctx;
    return context != NULL && test_set_frequency(context, &context->test_output, frequency_hz,
                                                 context->ops.test_set_output_frequency_hz);
}

static bool service_test_set_output_amplitude(uint16_t amplitude, void *ctx)
{
    CliServiceAdapter *context = (CliServiceAdapter *)ctx;
    return context != NULL && test_set_amplitude(context, &context->test_output, amplitude,
                                                 context->ops.test_set_output_amplitude);
}

static bool service_test_get_output_status(CliTestModeStatus *status_out, void *ctx)
{
    return test_get_status((CliServiceAdapter *)ctx, true, status_out);
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
    context->test_input.frequency_hz = TEST_DEFAULT_FREQ_HZ;
    context->test_input.amplitude = (uint16_t)(X_AXIS / 2U);
    context->test_output.frequency_hz = TEST_DEFAULT_FREQ_HZ;
    context->test_output.amplitude = (uint16_t)(X_AXIS / 2U);
    context->stream_batch_size = 1;

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
                                     bool *output_enabled_out,
                                     uint32_t *out_dropped_out,
                                     uint32_t *in_dropped_out, void *ctx);
static bool service_audio_get_diag(CliAudioDiag *diag_out, void *ctx);
static bool service_audio_reset_diag(void *ctx);

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
    services_out->slot_get = service_slot_get;
    services_out->slot_assign = service_slot_assign;
    services_out->slot_clear = service_slot_clear;
    services_out->slot_swap = service_slot_swap;
    services_out->slot_set_enabled = service_slot_set_enabled;
    services_out->slot_get_active = service_slot_get_active;
    services_out->slot_set_active = service_slot_set_active;
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
    services_out->audio_get_diag = service_audio_get_diag;
    services_out->audio_reset_diag = service_audio_reset_diag;
    services_out->context = context;
}

bool cli_service_adapter_apply_pot_sample(CliServiceAdapter *context, EffectType active_effect,
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
    return test_get_status(context, false, status_out);
}

bool cli_service_adapter_get_test_output_mode_status(CliServiceAdapter *context,
                                                     CliTestModeStatus *status_out)
{
    return test_get_status(context, true, status_out);
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

void cli_service_adapter_bind_uac_diag(CliServiceAdapter *context,
                                       void (*get_diag)(UsbdUacDiag *),
                                       void (*reset_diag)(void))
{
    if (context != NULL)
    {
        lock_ctx(context);
        context->uac_get_diag = get_diag;
        context->uac_reset_diag = reset_diag;
        unlock_ctx(context);
    }
}

void cli_service_adapter_bind_hw_events(CliServiceAdapter *context,
                                        void (*get_events)(uint32_t *, uint32_t *),
                                        void (*reset_events)(void))
{
    if (context != NULL)
    {
        lock_ctx(context);
        context->hw_events_get = get_events;
        context->hw_events_reset = reset_events;
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
        c->test_output.enabled = false;
        if (c->audio_stream != NULL)
        {
            usb_audio_stream_reset(c->audio_stream);
        }
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
        usb_audio_stream_reset_output(c->audio_stream);
        c->audio_stream->output_active = enabled;
    }
    unlock_ctx(c);
    return true;
}

static bool service_audio_get_status(uint8_t *source_out,
                                     bool *output_enabled_out,
                                     uint32_t *out_dropped_out,
                                     uint32_t *in_dropped_out,
                                     void *ctx)
{
    CliServiceAdapter *c = (CliServiceAdapter *)ctx;
    if (c == NULL || source_out == NULL || output_enabled_out == NULL ||
        out_dropped_out == NULL || in_dropped_out == NULL)
    {
        return false;
    }

    lock_ctx(c);
    *source_out =
        (c->test_input.enabled && c->test_input.vector == 5U) ? 1U : 0U;
    *output_enabled_out =
        (c->audio_stream != NULL) ? c->audio_stream->output_active : false;
    *out_dropped_out = usb_audio_stream_out_dropped(c->audio_stream);
    *in_dropped_out = usb_audio_stream_in_dropped(c->audio_stream);
    unlock_ctx(c);
    return true;
}

static bool service_audio_get_diag(CliAudioDiag *diag_out, void *ctx)
{
    CliServiceAdapter *c = (CliServiceAdapter *)ctx;
    if (c == NULL || diag_out == NULL || c->audio_stream == NULL)
    {
        return false;
    }

    lock_ctx(c);
    diag_out->as_out_packets = usb_audio_stream_as_out_packets(c->audio_stream);
    diag_out->in_ring_fill_peak = usb_audio_stream_in_ring_fill_peak(c->audio_stream);
    diag_out->effects_zoh_holds = usb_audio_stream_effects_zoh_holds(c->audio_stream);
    diag_out->effects_frames = usb_audio_stream_effects_frames(c->audio_stream);
    diag_out->in_ring_fill_current = (uint32_t)usb_audio_stream_in_available(c->audio_stream);
    unlock_ctx(c);

    if (c->uac_get_diag != NULL)
    {
        UsbdUacDiag uac = {0};
        c->uac_get_diag(&uac);
        diag_out->uac_isr_total = uac.data_out_isr_total;
        diag_out->uac_pushed = uac.data_out_pushed;
        diag_out->uac_stream_bad = uac.data_out_stream_bad;
        diag_out->uac_size_zero = uac.data_out_size_zero;
        diag_out->uac_size_oversz = uac.data_out_size_oversz;
        diag_out->uac_sof_count = uac.sof_count;
        diag_out->uac_last_received = uac.last_received;
        diag_out->uac_stream_repair_count = uac.stream_repair_count;
        diag_out->uac_as_in_tx = uac.as_in_tx;
        diag_out->uac_as_in_tx_err = uac.as_in_tx_err;
        diag_out->uac_as_in_datain = uac.as_in_datain;
        diag_out->uac_as_in_sof_active = uac.as_in_sof_active;
        diag_out->uac_as_in_tx_long = uac.as_in_tx_long;
        diag_out->uac_as_in_underrun = uac.as_in_underrun;
        diag_out->uac_setalt_out_1 = uac.setalt_out_1;
        diag_out->uac_setalt_out_0 = uac.setalt_out_0;
        diag_out->uac_setalt_in_1 = uac.setalt_in_1;
        diag_out->uac_setalt_in_0 = uac.setalt_in_0;
        diag_out->uac_as_out_active = uac.as_out_active;
        diag_out->uac_as_in_active = uac.as_in_active;
    }
    if (c->hw_events_get != NULL)
    {
        uint32_t adc_events = 0;
        uint32_t dac_events = 0;
        c->hw_events_get(&adc_events, &dac_events);
        diag_out->hw_adc_events = adc_events;
        diag_out->hw_dac_events = dac_events;
    }
    return true;
}

static bool service_audio_reset_diag(void *ctx)
{
    CliServiceAdapter *c = (CliServiceAdapter *)ctx;
    if (c == NULL || c->audio_stream == NULL)
    {
        return false;
    }

    lock_ctx(c);
    usb_audio_stream_reset_diag(c->audio_stream);
    unlock_ctx(c);

    if (c->uac_reset_diag != NULL)
    {
        c->uac_reset_diag();
    }
    if (c->hw_events_reset != NULL)
    {
        c->hw_events_reset();
    }
    return true;
}