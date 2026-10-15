#ifndef STRING_CHECK_H
#define STRING_CHECK_H

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

#define MAX_TIME 2529200

// Parsea un tiempo en segundos. Si hay error, deja el mensaje en response.
esp_err_t check_String(const char *argument, uint32_t *seconds,
                        char *response);
#endif 