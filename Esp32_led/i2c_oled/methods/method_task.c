#include "method_task.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_lvgl_port.h"
#include "ball.h"
#include "spinner.h"

#define STACK_SIZE (2048 * 2)

static void animation_controller_task(void *pvParameters)
{
    // Give some time for both the animations to start and display their initial state
    vTaskDelay(pdMS_TO_TICKS(500));

    while (1) {
        ball_pause();   // Freeze the ball in place

        if (lvgl_port_lock(100)) {
            // Hide the ball and show the spinner
            ball_set_visible(false);
            spinner_set_visible(true);
            lvgl_port_unlock();
        }

        vTaskDelay(pdMS_TO_TICKS(5000)); // wait 5 seconds

        if (lvgl_port_lock(100)) {
            // Hide the spinner and show the ball
            spinner_set_visible(false);
            ball_set_visible(true);
            lvgl_port_unlock();
        }

        ball_resume();   // Resume the ball movement
        vTaskDelay(pdMS_TO_TICKS(5000)); // wait 5 seconds
    }
}

esp_err_t method_task_start(void)
{
    BaseType_t ok = xTaskCreate(animation_controller_task,
                                "Handler",
                                STACK_SIZE,
                                NULL,
                                10,
                                NULL);
    return (ok == pdPASS) ? ESP_OK : ESP_FAIL;
}