#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cli_service_adapter.h"
#include "harness/expect.h"

int failures = 0;

typedef struct
{
    uint32_t romReadCalls;
    uint32_t romWriteCalls;
} TestOpsContext;

static void set_default_effects_state(EffectsState *state)
{
    memset(state, 0, sizeof(*state));
    effects_state_set_default_order(state);
}

static void set_default_effects_params(EffectsParams *params)
{
    memset(params, 0, sizeof(*params));

    params->overdriveMin.gain = 10;
    params->overdriveMax.gain = 110;
    params->overdriveMin.level = 20;
    params->overdriveMax.level = 120;
    params->overdriveMin.tone = 30;
    params->overdriveMax.tone = 130;
    params->overdriveMin.mix = 40;
    params->overdriveMax.mix = 140;

    params->echoMin.delay_samples = 5;
    params->echoMax.delay_samples = 50;
    params->echoMin.pre_delay = 1;
    params->echoMax.pre_delay = 20;
    params->echoMin.density = 2;
    params->echoMax.density = 22;
    params->echoMin.attack = 3;
    params->echoMax.attack = 23;
    params->echoMin.decay = 4;
    params->echoMax.decay = 24;

    params->compressionMin.threshold = 100;
    params->compressionMax.threshold = 400;
    params->compressionMin.ratio = 1;
    params->compressionMax.ratio = 32;
}

static bool rom_read_raw(uint16_t address, uint16_t length, uint8_t *payloadOut,
                         size_t payloadCapacity, size_t *payloadSizeOut, void *context)
{
    (void)address;
    (void)length;
    TestOpsContext *ops = (TestOpsContext *)context;
    ops->romReadCalls++;

    if (payloadCapacity < 2)
    {
        return false;
    }

    payloadOut[0] = 0x12;
    payloadOut[1] = 0x34;
    *payloadSizeOut = 2;
    return true;
}

static bool rom_write_raw(uint16_t address, const uint8_t *payload, size_t payloadSize,
                          void *context)
{
    (void)address;
    (void)payload;
    (void)payloadSize;
    TestOpsContext *ops = (TestOpsContext *)context;
    ops->romWriteCalls++;
    return true;
}

static void test_pot_override_precedence(void)
{
    EffectsState state;
    EffectsParams params;
    set_default_effects_state(&state);
    set_default_effects_params(&params);

    CliServiceAdapterContext context;
    cli_service_adapter_init(&context, &state, &params, NULL, 255);

    CliServices services = {0};
    cli_service_adapter_bind(&context, &services);

    expect_true(cli_service_adapter_apply_pot_sample(&context, OVERDRIVE, 0, 0),
                "pot apply without override");
    expect_eq_size(10, params.overdrive.gain, "raw pot input applied");

    expect_true(services.set_pot_override(0, 255, services.context), "set pot override");
    expect_true(cli_service_adapter_apply_pot_sample(&context, OVERDRIVE, 0, 0),
                "pot apply with override");
    expect_eq_size(110, params.overdrive.gain, "override value takes precedence");

    expect_true(services.clear_pot_override(0, services.context), "clear pot override");
    expect_true(cli_service_adapter_apply_pot_sample(&context, OVERDRIVE, 0, 0),
                "pot apply after clear");
    expect_eq_size(10, params.overdrive.gain, "hardware value restored after clear");
}

static void test_switch_override_precedence(void)
{
    EffectsState state;
    EffectsParams params;
    set_default_effects_state(&state);
    set_default_effects_params(&params);

    CliServiceAdapterContext context;
    cli_service_adapter_init(&context, &state, &params, NULL, 255);

    CliServices services = {0};
    cli_service_adapter_bind(&context, &services);

    expect_true(services.set_switch_override(1, true, services.context), "set switch override");
    expect_true(cli_service_adapter_apply_switches(&context, false, false, false),
                "apply switches with override");
    expect_true(state.isEnabled[ECHO], "switch override took precedence");

    expect_true(services.clear_switch_override(1, services.context), "clear switch override");
    expect_true(cli_service_adapter_apply_switches(&context, false, false, false),
                "apply switches after clear");
    expect_false(state.isEnabled[ECHO], "hardware switch restored after clear");
}

static void test_config_bounds_and_state_updates(void)
{
    EffectsState state;
    EffectsParams params;
    set_default_effects_state(&state);
    set_default_effects_params(&params);

    CliServiceAdapterContext context;
    cli_service_adapter_init(&context, &state, &params, NULL, 255);
    uint32_t heartbeat_period_ms = 500;
    cli_service_adapter_bind_heartbeat_period(&context, &heartbeat_period_ms, 50, 5000);

    CliServices services = {0};
    cli_service_adapter_bind(&context, &services);

    expect_false(services.config_set("overdrive.gain", 9, services.context),
                 "reject out-of-range config write");
    expect_true(services.config_set("overdrive.gain", 50, services.context),
                "accept in-range config write");

    int32_t value = -1;
    expect_true(services.config_get("overdrive.gain", &value, services.context),
                "read config value");
    expect_eq_u32(50, (uint32_t)value, "config write persisted");

    expect_true(services.config_set("state.active_effect", 2, services.context),
                "set active effect");
    expect_eq_u8(2, state.activeEffectSelection, "active effect updated");
    expect_false(services.config_set("state.active_effect", 9, services.context),
                 "reject invalid active effect");

    expect_true(services.config_set("heartbeat.period_ms", 250, services.context),
                "set heartbeat period");
    expect_eq_u32(250, heartbeat_period_ms, "heartbeat period updated");

    value = -1;
    expect_true(services.config_get("heartbeat.period_ms", &value, services.context),
                "read heartbeat period");
    expect_eq_u32(250, (uint32_t)value, "heartbeat period read back");

    expect_false(services.config_set("heartbeat.period_ms", 10, services.context),
                 "reject too-small heartbeat period");
}

static void test_rom_raw_guardrails(void)
{
    EffectsState state;
    EffectsParams params;
    set_default_effects_state(&state);
    set_default_effects_params(&params);

    TestOpsContext ops_context = {0};
    CliServiceAdapterOps ops = {
        .rom_read_raw = rom_read_raw,
        .rom_write_raw = rom_write_raw,
        .context = &ops_context,
    };

    CliServiceAdapterContext context;
    cli_service_adapter_init(&context, &state, &params, &ops, 255);
    context.romRawMinAddress = 0x0040;
    context.romRawMaxAddress = 0x00FF;
    context.romRawMaxLength = 8;

    CliServices services = {0};
    cli_service_adapter_bind(&context, &services);

    uint8_t out[8] = {0};
    size_t out_size = 0;
    expect_true(services.rom_read_raw(0x0040, 2, out, sizeof(out), &out_size, services.context),
                "rom read in range");
    expect_eq_u32(1, ops_context.romReadCalls, "rom read delegated");

    expect_false(services.rom_read_raw(0x0001, 2, out, sizeof(out), &out_size, services.context),
                 "rom read out of range rejected");
    expect_eq_u32(1, ops_context.romReadCalls, "rom read not delegated when rejected");

    uint8_t payload[2] = {0xAA, 0xBB};
    expect_true(services.rom_write_raw(0x0041, payload, 2, services.context),
                "rom write in range");
    expect_eq_u32(1, ops_context.romWriteCalls, "rom write delegated");

    expect_false(services.rom_write_raw(0x00FE, payload, 4, services.context),
                 "rom write overflow rejected");
    expect_eq_u32(1, ops_context.romWriteCalls, "rom write not delegated when rejected");
}

static void test_test_mode_status_and_validation(void)
{
    EffectsState state;
    EffectsParams params;
    set_default_effects_state(&state);
    set_default_effects_params(&params);

    CliServiceAdapterContext context;
    cli_service_adapter_init(&context, &state, &params, NULL, 255);

    CliServices services = {0};
    cli_service_adapter_bind(&context, &services);

    CliTestModeStatus status = {0};
    expect_true(services.test_get_status(&status, services.context), "read default test status");
    expect_false(status.enabled, "test mode disabled by default");

    expect_true(services.test_set_mode(true, services.context), "enable test mode");
    expect_true(services.test_set_vector(1, services.context), "set test vector");
    expect_true(services.test_set_frequency_hz(2000, services.context), "set test frequency");
    expect_true(services.test_set_amplitude((uint16_t)(X_AXIS / 2U), services.context),
                "set test amplitude");

    expect_false(services.test_set_vector(3, services.context), "reject invalid test vector");
    expect_false(services.test_set_frequency_hz(10, services.context),
                 "reject too-low frequency");
    expect_false(services.test_set_amplitude((uint16_t)(X_AXIS + 1U), services.context),
                 "reject out-of-range amplitude");

    expect_true(cli_service_adapter_get_test_mode_status(&context, &status),
                "read test status through adapter helper");
    expect_true(status.enabled, "test mode remains enabled");
    expect_eq_u8(1, status.vector, "test vector persisted");
    expect_eq_u16(2000, status.frequencyHz, "test frequency persisted");
}

static void test_log_stats_counters(void)
{
    EffectsState state;
    EffectsParams params;
    set_default_effects_state(&state);
    set_default_effects_params(&params);

    CliServiceAdapterContext context;
    cli_service_adapter_init(&context, &state, &params, NULL, 255);

    CliServices services = {0};
    cli_service_adapter_bind(&context, &services);

    expect_true(services.log_set_enabled(true, services.context), "enable logging");
    expect_true(services.log_set_level(4, services.context), "set log level");
    expect_true(services.log_set_stream(true, services.context), "enable stream");

    uint8_t level = 0;
    expect_true(cli_service_adapter_get_log_level(&context, &level), "read log level helper");
    expect_eq_u8(4, level, "log level helper tracks value");

    cli_service_adapter_note_frame_processed(&context);
    cli_service_adapter_note_frame_processed(&context);
    cli_service_adapter_note_processing_failure(&context);

    CliLogStats stats = {0};
    expect_true(services.log_get_stats(&stats, services.context), "read log stats");
    expect_true(stats.enabled, "logging enabled tracked");
    expect_true(stats.streamEnabled, "stream enabled tracked");
    expect_eq_u8(4, stats.level, "log level tracked");
    expect_eq_u32(2, stats.frameCount, "frame count tracked");
    expect_eq_u32(1, stats.failureCount, "failure count tracked");
    expect_eq_u32(0, stats.stepFailureCount, "step failure count tracked");
    expect_eq_u32(0, stats.stepFailureStreak, "step failure streak tracked");
    expect_eq_u8(1, stats.streamQueueCount, "stream queue count tracked");

    bool stream_enabled = false;
    expect_true(cli_service_adapter_get_log_stream_enabled(&context, &stream_enabled),
                "read log stream state");
    expect_true(stream_enabled, "log stream enabled tracked");

    expect_true(services.log_set_stream(false, services.context), "disable stream");
    bool log_enabled = false;
    expect_true(cli_service_adapter_get_log_enabled(&context, &log_enabled),
                "read log enabled state");
    expect_false(log_enabled, "stream disable clears logging state");
}

static void test_profiling_accumulation_and_reset(void)
{
    EffectsState state;
    EffectsParams params;
    set_default_effects_state(&state);
    set_default_effects_params(&params);

    CliServiceAdapterContext context;
    cli_service_adapter_init(&context, &state, &params, NULL, 255);
    context.streamBatchSize = 5;
    context.logStreamEnabled = true;

    cli_service_adapter_note_frame_timing(&context, 1000, false);
    expect_eq_u32(1000, context.frameTimeMinUs, "min tracks first frame");
    expect_eq_u32(1000, context.frameTimeMaxUs, "max tracks first frame");
    expect_eq_u32(1, context.measuredFrameCount, "frame count incremented");

    cli_service_adapter_note_frame_timing(&context, 2000, false);
    expect_eq_u32(1000, context.frameTimeMinUs, "min unchanged");
    expect_eq_u32(2000, context.frameTimeMaxUs, "max updated");
    expect_eq_u32(2, context.measuredFrameCount, "frame count 2");

    cli_service_adapter_note_frame_timing(&context, 500, true);
    expect_eq_u32(500, context.frameTimeMinUs, "min updated");
    expect_eq_u32(2000, context.frameTimeMaxUs, "max unchanged");
    expect_eq_u32(3, context.measuredFrameCount, "frame count 3");
    expect_eq_u32(1, context.overrunCount, "overrun counted");

    cli_service_adapter_note_step_result(&context, false);
    cli_service_adapter_note_step_result(&context, false);
    expect_eq_u32(2, context.stepFailureCount, "step failures counted");
    expect_eq_u32(2, context.stepFailureStreak, "step failure streak counted");

    cli_service_adapter_note_step_result(&context, true);
    expect_eq_u32(2, context.stepFailureCount, "step failure count retained after success");
    expect_eq_u32(0, context.stepFailureStreak, "step failure streak reset after success");

    cli_service_adapter_reset_profiling_stats(&context);
    expect_eq_u32(0, context.measuredFrameCount, "frame count reset");
    expect_eq_u32(0, context.overrunCount, "overrun reset");
    expect_eq_u32(0, context.frameTimeMinUs, "min reset");
    expect_eq_u32(0, context.frameTimeMaxUs, "max reset");
    expect_eq_u32(0, context.frameTimeTotalUs, "total reset");
    expect_eq_u32(0, context.stepFailureCount, "step failure count reset");
    expect_eq_u32(0, context.stepFailureStreak, "step failure streak reset");
}

static void test_profiling_batch_emission(void)
{
    EffectsState state;
    EffectsParams params;
    set_default_effects_state(&state);
    set_default_effects_params(&params);

    CliServiceAdapterContext context;
    cli_service_adapter_init(&context, &state, &params, NULL, 255);
    context.streamBatchSize = 3;
    context.logStreamEnabled = true;

    CliServices services = {0};
    cli_service_adapter_bind(&context, &services);

    cli_service_adapter_note_frame_timing(&context, 1000, false);
    expect_eq_u32(1, context.streamBatchCounter, "batch counter 1");

    cli_service_adapter_note_frame_timing(&context, 2000, false);
    expect_eq_u32(2, context.streamBatchCounter, "batch counter 2");

    cli_service_adapter_note_frame_timing(&context, 1500, false);
    expect_eq_u32(0, context.streamBatchCounter, "batch counter reset after emit");

    char line[64] = {0};
    bool has_line = false;
    expect_true(services.log_read_line(line, sizeof(line), &has_line, services.context),
                "read batch line");
    expect_true(has_line, "batch line available");
    expect_true(strstr(line, "prof") != NULL, "batch line starts with prof");
    expect_true(strstr(line, "f=3") != NULL, "batch line has frame count 3");
    expect_true(strstr(line, "t=1500") != NULL, "batch line has avg 1500");

    // Second batch
    cli_service_adapter_note_frame_timing(&context, 3000, false);
    cli_service_adapter_note_frame_timing(&context, 3000, false);
    cli_service_adapter_note_frame_timing(&context, 3000, false);
    expect_true(services.log_read_line(line, sizeof(line), &has_line, services.context),
                "read second batch line");
    expect_true(has_line, "second batch line available");
    expect_true(strstr(line, "f=3") != NULL, "second batch line has frame count 3");
}

static void test_profiling_batch_size_one(void)
{
    EffectsState state;
    EffectsParams params;
    set_default_effects_state(&state);
    set_default_effects_params(&params);

    CliServiceAdapterContext context;
    cli_service_adapter_init(&context, &state, &params, NULL, 255);
    context.streamBatchSize = 1;
    context.logStreamEnabled = true;

    CliServices services = {0};
    cli_service_adapter_bind(&context, &services);

    cli_service_adapter_note_frame_timing(&context, 1234, false);
    expect_eq_u32(0, context.streamBatchCounter, "per-frame batch counter resets");

    char line[64] = {0};
    bool has_line = false;
    expect_true(services.log_read_line(line, sizeof(line), &has_line, services.context),
                "read per-frame line");
    expect_true(has_line, "per-frame line available");
    expect_true(strstr(line, "f=1") != NULL, "per-frame line has f=1");
    expect_true(strstr(line, "t=1234") != NULL, "per-frame line has t=1234");
}

static void test_queue_overflow_drop_tracking(void)
{
    EffectsState state;
    EffectsParams params;
    set_default_effects_state(&state);
    set_default_effects_params(&params);

    CliServiceAdapterContext context;
    cli_service_adapter_init(&context, &state, &params, NULL, 255);
    context.logStreamEnabled = true;
    context.streamBatchSize = 1;

    // Fill queue with profiling frames until it overflows
    for (int i = 0; i < CLI_LOG_STREAM_QUEUE_DEPTH + 5; i++)
    {
        cli_service_adapter_note_frame_timing(&context, 1000, false);
    }

    expect_eq_u32(5, context.streamDropCount, "5 stream lines dropped");
}

static void test_log_stream_line_queue(void)
{
    EffectsState state;
    EffectsParams params;
    set_default_effects_state(&state);
    set_default_effects_params(&params);

    CliServiceAdapterContext context;
    cli_service_adapter_init(&context, &state, &params, NULL, 255);

    CliServices services = {0};
    cli_service_adapter_bind(&context, &services);

    expect_true(services.log_set_stream(true, services.context), "enable stream");
    expect_true(cli_service_adapter_append_log_line(&context, "log event alpha"),
                "append stream log line");

    char line[64] = {0};
    bool has_line = false;
    expect_true(services.log_read_line(line, sizeof(line), &has_line, services.context),
                "read stream line");
    expect_true(has_line, "stream queue reports available line");
    expect_true(strcmp(line, "log event alpha") == 0, "stream line content preserved");

    has_line = true;
    expect_true(services.log_read_line(line, sizeof(line), &has_line, services.context),
                "read empty stream queue");
    expect_false(has_line, "empty stream queue reports no line");
}

int main(void)
{
    test_pot_override_precedence();
    test_switch_override_precedence();
    test_config_bounds_and_state_updates();
    test_rom_raw_guardrails();
    test_test_mode_status_and_validation();
    test_log_stats_counters();
    test_profiling_accumulation_and_reset();
    test_profiling_batch_emission();
    test_profiling_batch_size_one();
    test_queue_overflow_drop_tracking();
    test_log_stream_line_queue();

    if (failures != 0)
    {
        fprintf(stderr, "Firmware cli service adapter tests failed: %d\n", failures);
        return 1;
    }

    puts("Firmware cli service adapter tests passed");
    return 0;
}