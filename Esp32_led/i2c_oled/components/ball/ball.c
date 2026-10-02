#include "ball.h"
#include "esp_lvgl_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define BALL_SIZE        12
#define BALL_PERIOD_MS   30
#define BALL_TASK_STACK  (2048 * 2)
#define BALL_TASK_PRIO   5




//Ball parameters 
static lv_obj_t *ball_obj;
static int16_t ball_x = 10;
static int16_t ball_y = 10;
static int16_t ball_dx = 2;
static int16_t ball_dy = 2;
static int16_t ball_max_x, ball_max_y;
static volatile bool s_paused;

static void ball_task(void *arg)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(BALL_PERIOD_MS));
        if (s_paused) {
            continue;
        }
          // Request UI access again with a 100ms timeout
        if (lvgl_port_lock(100)) {
            
            // FAILSAFE: Only calculate and move if the object actually exists
            if (ball_obj != NULL) {
                
                // Update coordinates based on current velocity
                ball_x += ball_dx; 
                ball_y += ball_dy; 

                // X-axis collision detection (Left and Right borders)
                if (ball_x <= 0) {
                    ball_x = 0;
                    ball_dx *= -1; // Reverse horizontal direction
                } else if (ball_x >= ball_max_x) {
                    ball_x = ball_max_x;
                    ball_dx *= -1;
                }

                // Y-axis collision detection (Top and Bottom borders)
                if (ball_y <= 0) {
                    ball_y = 0;
                    ball_dy *= -1; // Reverse vertical direction
                } else if (ball_y >= ball_max_y) {
                    ball_y = ball_max_y;
                    ball_dy *= -1;
                }

                // Apply the new calculated coordinates to the LVGL object
                lv_obj_set_pos(ball_obj, ball_x, ball_y); 
            }
            
            // Release the mutex so the background task can render the frame
            lvgl_port_unlock();
        }


    }
}


esp_err_t ball_start(lv_disp_t *disp)
{
    if (ball_obj != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    /* Los límites salen de la pantalla, ya no dependen de EXAMPLE_LCD_*_RES */
    ball_max_x = lv_disp_get_hor_res(disp) - BALL_SIZE;
    ball_max_y = lv_disp_get_ver_res(disp) - BALL_SIZE;

    if (!lvgl_port_lock(0)) {
        return ESP_ERR_TIMEOUT;
    }
    lv_obj_t *scr = lv_disp_get_scr_act(disp);
    ball_obj = lv_obj_create(scr);
    lv_obj_set_size(ball_obj, BALL_SIZE, BALL_SIZE);
    lv_obj_set_style_radius(ball_obj, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(ball_obj, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(ball_obj, lv_color_black(), 0);
    lv_obj_set_style_border_width(ball_obj, 0, 0);
    lv_obj_set_pos(ball_obj, ball_x, ball_y);
    lvgl_port_unlock();

    BaseType_t ok = xTaskCreate(ball_task, "ball", BALL_TASK_STACK,
                                NULL, BALL_TASK_PRIO, NULL);
    return (ok == pdPASS) ? ESP_OK : ESP_FAIL;
}

void ball_set_visible(bool visible)
{
    if (ball_obj == NULL) {
        return;
    }
    if (lvgl_port_lock(0)) {
        if (visible) {
            lv_obj_clear_flag(ball_obj, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(ball_obj, LV_OBJ_FLAG_HIDDEN);
        }
        lvgl_port_unlock();
    }
}

void ball_pause(void)  { s_paused = true;  }
void ball_resume(void) { s_paused = false; }