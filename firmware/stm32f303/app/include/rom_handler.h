#ifndef ROM_HANDLER_H
#define ROM_HANDLER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "effects_control_logic.h"
#include "effects_state_manager.h"

size_t rom_handler_read_payload_size(void);
size_t rom_handler_write_payload_size(void);

bool rom_handler_encode_address(uint16_t address, uint8_t *payload, size_t payloadSize);
bool rom_handler_encode_write_payload(uint16_t writeAddress, const EffectsState *state,
    const EffectsParams *params, uint8_t *payload, size_t payloadSize);
bool rom_handler_decode_read_payload(const uint8_t *payload, size_t payloadSize, EffectsState *state,
    EffectsParams *params);

#endif
