#include <stdio.h>
#include <stdlib.h>
#include "string_check.h"

_Static_assert(MAX_TIME <= UINT32_MAX, "MAX_TIME debe entrar en uint32_t");

esp_err_t check_String(const char *argument, uint32_t *seconds,
                              char *response)
{
    if (argument == NULL) {
        sprintf(response, "arg null\n");
        return ESP_ERR_INVALID_ARG;
    }

    char *end;
    long long value = strtoll(argument, &end, 10);

    if (end == argument) {      // If it doesnt have anything or there's something else than numbers 
        sprintf(response, "Only numbers\n");
        return ESP_ERR_INVALID_ARG;
    }
    if (value < 0) {
        sprintf(response, "No negative\n");
        return ESP_ERR_INVALID_ARG; // If its negative
    }
    if (value > MAX_TIME) {// If it passes the max input
        sprintf(response, "Max passed\n");
        return ESP_ERR_INVALID_SIZE;
    }

    *seconds = (uint32_t)value;
    return ESP_OK;
}