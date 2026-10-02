#include "method_event_group.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "ball.h"
#include "spinner.h"

#define STACK_SIZE (2048 * 2)
#define BIT_UI_SWITCH (1 << 0)   // Bit to switch between states on the event group

static const char *TAG = "method_event_group";

static EventGroupHandle_t ui_event_group;
static bool ball_state = true;   // true = ball visible, false = spinner visible

static void ui_event_group_init(void)
{
    ui_event_group = xEventGroupCreate();
    configASSERT(ui_event_group != NULL);
}

// Every time the timer reaches 5 sec this function sets the switch bit to 1
static void switch_timer_group(void *arg)
{
    ESP_LOGI(TAG, "timer disparado, seteando bit");
    xEventGroupSetBits(ui_event_group, BIT_UI_SWITCH);
}

static void timer_init_group(void)
{
    const esp_timer_create_args_t timer_args = {
        .callback = &switch_timer_group,
        .name = "switch_timer",
        .dispatch_method = ESP_TIMER_TASK,
    };
    esp_timer_handle_t switch_timer;
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &switch_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(switch_timer, 5 * 1000000)); // 5s
}

static void group_state_task(void *pvParameters)
{
    while (1) {
        // pdTRUE = after it notices the bit in 1, it clears it so the next iteration waits again
        xEventGroupWaitBits(ui_event_group, BIT_UI_SWITCH, pdTRUE, pdFALSE, portMAX_DELAY);

        if (lvgl_port_lock(portMAX_DELAY)) {
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
        }
        ball_state = !ball_state;
    }
}

esp_err_t method_event_group_start(void)
{
    ui_event_group_init();   // The event group must exist before the timer fires

    BaseType_t ok = xTaskCreate(group_state_task, 
                                "Group_switch", 
                                STACK_SIZE, 
                                NULL,
                                 6,
                                NULL);
                                
    if (ok != pdPASS) {
        return ESP_FAIL;
    }

    timer_init_group();      // Start the timer once the task is already waiting
    return ESP_OK;
}