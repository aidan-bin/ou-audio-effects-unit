#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "effects_model.h"
#include "rom_handler.h"

#include "harness/expect.h"

int failures = 0;

static void test_encode_address_big_endian(void)
{
    uint8_t payload[2] = {0};

    expect_true(rom_handler_encode_address(0x1234, payload, sizeof(payload)),
                "encode address success");
    expect_eq_u8(0x12, payload[0], "address upper byte");
    expect_eq_u8(0x34, payload[1], "address lower byte");
}

static void test_encode_write_and_decode_round_trip(void)
{
    EffectsState state = {
        .ordered = {ECHO, COMPRESSION, OVERDRIVE},
        .is_enabled = {true, false, true},
        .active_effect_selection = 2,
    };

    EffectsParams params;
    memset(&params, 0, sizeof(params));
    params.overdrive.level = 111;
    params.echo.attack = 222;
    params.compression.threshold = 333;

    uint8_t payload[sizeof(uint16_t) + sizeof(EffectsState) + sizeof(EffectsParams)] = {0};

    expect_true(rom_handler_encode_write_payload(0x0040, &state, &params, payload, sizeof(payload)),
                "encode write payload success");

    EffectsState decoded_state;
    EffectsParams decoded_params;
    memset(&decoded_state, 0, sizeof(decoded_state));
    memset(&decoded_params, 0, sizeof(decoded_params));

    expect_true(rom_handler_decode_read_payload(&payload[2], sizeof(payload) - 2, &decoded_state,
                                                &decoded_params),
                "decode read payload success");

    expect_true(decoded_state.ordered[0] == ECHO, "decode ordered[0]");
    expect_true(decoded_state.ordered[1] == COMPRESSION, "decode ordered[1]");
    expect_true(decoded_state.ordered[2] == OVERDRIVE, "decode ordered[2]");
    expect_true(decoded_state.active_effect_selection == 2, "decode selection");

    expect_true(decoded_params.overdrive.level == 111, "decode overdrive level");
    expect_true(decoded_params.echo.attack == 222, "decode echo attack");
    expect_true(decoded_params.compression.threshold == 333, "decode compression threshold");
}

static void test_encode_fails_for_small_buffer(void)
{
    uint8_t small_address_buf[1] = {0};
    expect_true(!rom_handler_encode_address(0x1111, small_address_buf, sizeof(small_address_buf)),
                "encode address rejects small buffer");

    EffectsState state = {0};
    EffectsParams params = {0};
    uint8_t small_write_buf[4] = {0};

    expect_true(
        !rom_handler_encode_write_payload(0, &state, &params, small_write_buf, sizeof(small_write_buf)),
        "encode write rejects small buffer");
}

int main(void)
{
    test_encode_address_big_endian();
    test_encode_write_and_decode_round_trip();
    test_encode_fails_for_small_buffer();

    if (failures != 0)
    {
        fprintf(stderr, "Firmware rom handler tests failed: %d\n", failures);
        return 1;
    }

    puts("Firmware rom handler tests passed");
    return 0;
}
