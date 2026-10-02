#ifndef SPINNER_H
#define SPINNER_H

#include <stdbool.h>
#include "esp_err.h"
#include "lvgl.h"

/* Creates the spinner (starts not visible) */
esp_err_t spinner_create(lv_disp_t *disp);

/* Hides or pops up the spinner. */
void spinner_set_visible(bool visible);
#endif //SPINNER_H