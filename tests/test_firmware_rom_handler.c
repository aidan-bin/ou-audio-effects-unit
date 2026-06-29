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
        .slots = {ECHO, COMPRESSION, OVERDRIVE, EFFECT_ID_NONE},
        .slot_enabled = {true, false, true, false},
        .active_slot = 2,
    };

    EffectsParams params;
    memset(&params, 0, sizeof(params));
    params.overdrive.level = 111;
    params.echo.attack = 222;
    params.compression.threshold = 333;

    uint8_t payload[ROM_HANDLER_ADDRESS_BYTES + ROM_STATE_HEADER_BYTES + sizeof(EffectsState) +
                    sizeof(EffectsParams)] = {0};

    expect_true(rom_handler_encode_write_payload(0x0040, &state, &params, payload, sizeof(payload)),
                "encode write payload success");

    EffectsState decoded_state;
    EffectsParams decoded_params;
    memset(&decoded_state, 0, sizeof(decoded_state));
    memset(&decoded_params, 0, sizeof(decoded_params));

    expect_true(rom_handler_decode_read_payload(&payload[ROM_HANDLER_ADDRESS_BYTES],
                                                sizeof(payload) - ROM_HANDLER_ADDRESS_BYTES,
                                                &decoded_state, &decoded_params),
                "decode read payload success");

    expect_true(decoded_state.slots[0] == ECHO, "decode slots[0]");
    expect_true(decoded_state.slots[1] == COMPRESSION, "decode slots[1]");
    expect_true(decoded_state.slots[2] == OVERDRIVE, "decode slots[2]");
    expect_true(decoded_state.slots[3] == EFFECT_ID_NONE, "decode slots[3] empty");
    expect_true(decoded_state.active_slot == 2, "decode active slot");

    expect_true(decoded_params.overdrive.level == 111, "decode overdrive level");
    expect_true(decoded_params.echo.attack == 222, "decode echo attack");
    expect_true(decoded_params.compression.threshold == 333, "decode compression threshold");
}

static void test_decode_rejects_bad_header(void)
{
    EffectsState state = {
        .slots = {OVERDRIVE, ECHO, COMPRESSION, EFFECT_ID_NONE},
        .slot_enabled = {true, false, true, false},
        .active_slot = 0,
    };
    EffectsParams params;
    memset(&params, 0, sizeof(params));

    uint8_t payload[ROM_HANDLER_ADDRESS_BYTES + ROM_STATE_HEADER_BYTES + sizeof(EffectsState) +
                    sizeof(EffectsParams)] = {0};
    expect_true(rom_handler_encode_write_payload(0x0040, &state, &params, payload, sizeof(payload)),
                "encode for rejection test");

    uint8_t *body = &payload[ROM_HANDLER_ADDRESS_BYTES];
    size_t body_size = sizeof(payload) - ROM_HANDLER_ADDRESS_BYTES;

    EffectsState out_state;
    EffectsParams out_params;

    uint8_t good[sizeof(payload)];
    memcpy(good, body, body_size);
    expect_true(rom_handler_decode_read_payload(good, body_size, &out_state, &out_params),
                "valid header decodes");

    uint8_t bad_magic[sizeof(payload)];
    memcpy(bad_magic, body, body_size);
    bad_magic[0] ^= 0xFF;
    expect_false(rom_handler_decode_read_payload(bad_magic, body_size, &out_state, &out_params),
                 "bad magic rejected");

    uint8_t bad_version[sizeof(payload)];
    memcpy(bad_version, body, body_size);
    bad_version[2] = (uint8_t)(ROM_STATE_VERSION + 1);
    expect_false(rom_handler_decode_read_payload(bad_version, body_size, &out_state, &out_params),
                 "wrong version rejected");

    uint8_t bad_len[sizeof(payload)];
    memcpy(bad_len, body, body_size);
    bad_len[4] ^= 0xFF;
    expect_false(rom_handler_decode_read_payload(bad_len, body_size, &out_state, &out_params),
                 "wrong length rejected");

    expect_false(rom_handler_decode_read_payload(body, body_size - 1, &out_state, &out_params),
                 "short payload rejected");
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
    test_decode_rejects_bad_header();
    test_encode_fails_for_small_buffer();

    if (failures != 0)
    {
        fprintf(stderr, "Firmware rom handler tests failed: %d\n", failures);
        return 1;
    }

    puts("Firmware rom handler tests passed");
    return 0;
}
