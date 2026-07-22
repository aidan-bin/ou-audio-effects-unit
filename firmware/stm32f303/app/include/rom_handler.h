/*
 * rom_handler — ROM driver and encoder/decoder for effects configuration.
 */

#ifndef ROM_HANDLER_H
#define ROM_HANDLER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "effects_model.h"

#define ROM_HANDLER_ADDRESS_BYTES 2
#define ROM_STATE_MAGIC_0 'O'
#define ROM_STATE_MAGIC_1 'U'
#define ROM_STATE_VERSION 1
#define ROM_STATE_HEADER_BYTES 5 /* magic(2) + version(1) + length(2) */

size_t rom_handler_read_payload_size(void);
size_t rom_handler_write_payload_size(void);

bool rom_handler_encode_address(uint16_t address, uint8_t *payload, size_t payload_size);
bool rom_handler_encode_write_payload(uint16_t write_address, const EffectsState *state,
                                      const EffectsParams *params, uint8_t *payload,
                                      size_t payload_size);
bool rom_handler_decode_read_payload(const uint8_t *payload, size_t payload_size,
                                     EffectsState *state, EffectsParams *params);

#endif
