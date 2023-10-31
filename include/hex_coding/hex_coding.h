#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief encode bytes in hex
 * 
 * @param in input buffer
 * @param out output buffer
 * @param in_bytes number of input bytes
 */
void hex_encode(const uint8_t *in, char *out, size_t in_bytes);

/**
 * @brief decode hex into bytes
 * 
 * @param in input hex string
 * @param out output buffer
 * @param in_bytes number of input characters
 */
void hex_decode(const char *in, uint8_t *out, size_t in_bytes);