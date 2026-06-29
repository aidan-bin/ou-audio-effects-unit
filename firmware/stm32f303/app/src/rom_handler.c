#include "rom_handler.h"

#include <string.h>

#define ADDRESS_HIGH_BYTE_MASK 0xFF00U
#define ADDRESS_LOW_BYTE_MASK 0x00FFU
#define ADDRESS_HIGH_BYTE_SHIFT 8U

static void write_be16(uint8_t *dst, uint16_t value)
{
    dst[0] = (uint8_t)((value & ADDRESS_HIGH_BYTE_MASK) >> ADDRESS_HIGH_BYTE_SHIFT);
    dst[1] = (uint8_t)(value & ADDRESS_LOW_BYTE_MASK);
}

static uint16_t read_be16(const uint8_t *src)
{
    return (uint16_t)(((uint16_t)src[0] << ADDRESS_HIGH_BYTE_SHIFT) | src[1]);
}

static void encode_state_frame(uint8_t *frame, const EffectsState *state,
                               const EffectsParams *params)
{
    frame[0] = (uint8_t)ROM_STATE_MAGIC_0;
    frame[1] = (uint8_t)ROM_STATE_MAGIC_1;
    frame[2] = (uint8_t)ROM_STATE_VERSION;
    write_be16(&frame[3], (uint16_t)(sizeof(*state) + sizeof(*params)));

    uint8_t *body = &frame[ROM_STATE_HEADER_BYTES];
    memcpy(body, state, sizeof(*state));
    memcpy(&body[sizeof(*state)], params, sizeof(*params));
}

size_t rom_handler_read_payload_size(void)
{
    return ROM_STATE_HEADER_BYTES + sizeof(EffectsState) + sizeof(EffectsParams);
}

size_t rom_handler_write_payload_size(void)
{
    return ROM_HANDLER_ADDRESS_BYTES + rom_handler_read_payload_size();
}

bool rom_handler_encode_address(uint16_t address, uint8_t *payload, size_t payload_size)
{
    if (payload == NULL || payload_size < ROM_HANDLER_ADDRESS_BYTES)
    {
        return false;
    }

    write_be16(payload, address);
    return true;
}

bool rom_handler_encode_write_payload(uint16_t write_address, const EffectsState *state,
                                      const EffectsParams *params, uint8_t *payload,
                                      size_t payload_size)
{
    if (state == NULL || params == NULL || payload == NULL)
    {
        return false;
    }

    if (payload_size < rom_handler_write_payload_size())
    {
        return false;
    }

    if (!rom_handler_encode_address(write_address, payload, payload_size))
    {
        return false;
    }

    encode_state_frame(&payload[ROM_HANDLER_ADDRESS_BYTES], state, params);
    return true;
}

bool rom_handler_decode_read_payload(const uint8_t *payload, size_t payload_size,
                                     EffectsState *state, EffectsParams *params)
{
    if (payload == NULL || state == NULL || params == NULL)
    {
        return false;
    }

    if (payload_size < rom_handler_read_payload_size())
    {
        return false;
    }

    uint16_t expected_len = (uint16_t)(sizeof(*state) + sizeof(*params));
    uint16_t stored_len = read_be16(&payload[3]);

    if (payload[0] != (uint8_t)ROM_STATE_MAGIC_0 || payload[1] != (uint8_t)ROM_STATE_MAGIC_1 ||
        payload[2] != (uint8_t)ROM_STATE_VERSION || stored_len != expected_len)
    {
        return false;
    }

    const uint8_t *body = &payload[ROM_STATE_HEADER_BYTES];
    memcpy(state, body, sizeof(*state));
    memcpy(params, &body[sizeof(*state)], sizeof(*params));
    return true;
}
