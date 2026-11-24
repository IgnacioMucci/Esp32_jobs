#include <string.h>
#include "hex_utils.h"

/* Converts ONE hex character into its numeric value (0..15).
 * Returns -1 if the character is not a hex digit.
 * We do it by hand (no strtol/errno) so the behaviour is fully predictable. */
static int hex_nibble(char c)
{
    if (c >= '0' && c <= '9') return c - '0';        /* '0'..'9' -> 0..9   */
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;   /* 'A'..'F' -> 10..15 */
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;   /* 'a'..'f' -> 10..15 */
    return -1;
}

esp_err_t hex_to_bytes(const char *hex, uint8_t *out, size_t out_len)
{
    if (hex == NULL || out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Two characters per byte, and nothing extra: an exact length check also
     * rejects truncated frames and frames with trailing garbage. */
    if (strlen(hex) != out_len * 2) {
        return ESP_ERR_INVALID_SIZE;
    }

    for (size_t i = 0; i < out_len; i++) {
        int high = hex_nibble(hex[2 * i]);        /* first char  = upper 4 bits */
        int low  = hex_nibble(hex[2 * i + 1]);    /* second char = lower 4 bits */
        if (high < 0 || low < 0) {
            return ESP_ERR_INVALID_ARG;
        }
        out[i] = (uint8_t)((high << 4) | low);    /* e.g. 'E','1' -> 0xE1 */
    }
    return ESP_OK;
}
