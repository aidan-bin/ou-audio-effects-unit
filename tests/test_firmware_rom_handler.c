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
        .isEnabled = {true, false, true},
        .activeEffectSelection = 2,
    };

    EffectsParams params;
    memset(&params, 0, sizeof(params));
    params.overdrive.level = 111;
    params.echo.attack = 222;
    params.compression.threshold = 333;

    uint8_t payload[sizeof(uint16_t) + sizeof(EffectsState) + sizeof(EffectsParams)] = {0};

    expect_true(rom_handler_encode_write_payload(0x0040, &state, &params, payload, sizeof(payload)),
                "encode write payload success");

    EffectsState decodedState;
    EffectsParams decodedParams;
    memset(&decodedState, 0, sizeof(decodedState));
    memset(&decodedParams, 0, sizeof(decodedParams));

    expect_true(rom_handler_decode_read_payload(&payload[2], sizeof(payload) - 2, &decodedState,
                                                &decodedParams),
                "decode read payload success");

    expect_true(decodedState.ordered[0] == ECHO, "decode ordered[0]");
    expect_true(decodedState.ordered[1] == COMPRESSION, "decode ordered[1]");
    expect_true(decodedState.ordered[2] == OVERDRIVE, "decode ordered[2]");
    expect_true(decodedState.activeEffectSelection == 2, "decode selection");

    expect_true(decodedParams.overdrive.level == 111, "decode overdrive level");
    expect_true(decodedParams.echo.attack == 222, "decode echo attack");
    expect_true(decodedParams.compression.threshold == 333, "decode compression threshold");
}

static void test_encode_fails_for_small_buffer(void)
{
    uint8_t smallAddressBuf[1] = {0};
    expect_true(!rom_handler_encode_address(0x1111, smallAddressBuf, sizeof(smallAddressBuf)),
                "encode address rejects small buffer");

    EffectsState state = {0};
    EffectsParams params = {0};
    uint8_t smallWriteBuf[4] = {0};

    expect_true(
        !rom_handler_encode_write_payload(0, &state, &params, smallWriteBuf, sizeof(smallWriteBuf)),
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
