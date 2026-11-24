#ifndef HEX_UTILS_H
#define HEX_UTILS_H
 
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"
 
/*
 * Converts a hexadecimal text string into raw bytes.
 *
 * hex     : NUL-terminated string, must be EXACTLY out_len * 2 characters long
 *           and contain only 0-9, a-f, A-F (e.g. "83020E10" -> 83 02 0E 10).
 * out     : buffer where the bytes are written (must hold out_len bytes).
 * out_len : number of bytes expected.
 *
 * Returns ESP_OK on success, or:
 *   ESP_ERR_INVALID_ARG  - NULL pointer or a non-hex character
 *   ESP_ERR_INVALID_SIZE - wrong string length
 * On error the content of "out" is undefined and must not be used.
 */
esp_err_t hex_to_bytes(const char *hex, uint8_t *out, size_t out_len);
 
#endif
 
