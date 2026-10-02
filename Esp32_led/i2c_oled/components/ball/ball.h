#ifndef BALL_H
#define BALL_H

#include <stdbool.h>
#include "esp_err.h"
#include "lvgl.h"

/* Creates the ball (visible) and the task that moves it */
esp_err_t ball_start(lv_disp_t *disp);

/* Hides or shows the ball. */
void ball_set_visible(bool visible);

/* Pauses or resumes the ball movement */
void ball_pause(void);
void ball_resume(void);

#endif /* BALL_H */