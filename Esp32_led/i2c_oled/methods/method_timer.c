#include "method_timer.h"
#include "esp_timer.h"
#include "esp_lvgl_port.h"
#include "ball.h"
#include "spinner.h"

static bool ball_state = true;   // true = ball visible, false = spinner visible

static void lvgl_timer_handler(void *arg)
{
    if (lvgl_port_lock(0)) {
        if (ball_state) {   // If the ball is visible, hide it and show the spinner
            ball_set_visible(false);
            spinner_set_visible(true);
            ball_pause();
        } else {            // If the spinner is visible, hide it and show the ball
            spinner_set_visible(false);
            ball_set_visible(true);
            ball_resume();
        }
        lvgl_port_unlock();
        ball_state = !ball_state;
    }
}

esp_err_t method_timer_start(void)
{
    const esp_timer_create_args_t timer_args = {
        .callback = &lvgl_timer_handler, // when it reaches the amount of time it goes to that function
        .name = "timer_animacion",
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK
    };

    esp_timer_handle_t lvgl_timer; //timer handler
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &lvgl_timer)); //creates the timer
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_timer, 10 * 500000)); // 5000ms
    return ESP_OK;
}