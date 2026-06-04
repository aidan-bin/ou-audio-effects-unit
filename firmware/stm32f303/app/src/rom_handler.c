#include "rom_handler.h"

#include <string.h>

#define ROM_HANDLER_ADDRESS_BYTES 2

size_t rom_handler_read_payload_size(void)
{
    return sizeof(EffectsState) + sizeof(EffectsParams);
}

size_t rom_handler_write_payload_size(void)
{
    return ROM_HANDLER_ADDRESS_BYTES + rom_handler_read_payload_size();
}

bool rom_handler_encode_address(uint16_t address, uint8_t *payload, size_t payloadSize)
{
    if (payload == NULL || payloadSize < ROM_HANDLER_ADDRESS_BYTES)
    {
        return false;
    }

    payload[0] = (uint8_t)((address & 0xFF00U) >> 8U);
    payload[1] = (uint8_t)(address & 0x00FFU);
    return true;
}

bool rom_handler_encode_write_payload(uint16_t writeAddress, const EffectsState *state,
                                      const EffectsParams *params, uint8_t *payload,
                                      size_t payloadSize)
{
    if (state == NULL || params == NULL || payload == NULL)
    {
        return false;
    }

    if (payloadSize < rom_handler_write_payload_size())
    {
        return false;
    }

    if (!rom_handler_encode_address(writeAddress, payload, payloadSize))
    {
        return false;
    }

    memcpy(&payload[ROM_HANDLER_ADDRESS_BYTES], state, sizeof(*state));
    memcpy(&payload[ROM_HANDLER_ADDRESS_BYTES + sizeof(*state)], params, sizeof(*params));
    return true;
}

bool rom_handler_decode_read_payload(const uint8_t *payload, size_t payloadSize,
                                     EffectsState *state, EffectsParams *params)
{
    if (payload == NULL || state == NULL || params == NULL)
    {
        return false;
    }

    if (payloadSize < rom_handler_read_payload_size())
    {
        return false;
    }

    memcpy(state, payload, sizeof(*state));
    memcpy(params, &payload[sizeof(*state)], sizeof(*params));
    return true;
}
