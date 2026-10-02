#include "spinner.h"
#include "esp_lvgl_port.h"

static lv_obj_t *spinner_obj;

esp_err_t spinner_create(lv_disp_t *disp) 
{
    if (spinner_obj != NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!lvgl_port_lock(0)) {
        return ESP_ERR_TIMEOUT;
    }

    lv_obj_t *scr = lv_disp_get_scr_act(disp);
      
        //Force the background color to black for safety
        lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

        spinner_obj = lv_spinner_create(scr, 1500, 180); // Create a spinner with: lv_spinner_create(parent, spin_time_ms, arc_angle)
        lv_obj_set_size(spinner_obj, 35, 35); // Set the size of the spinner to 50x50 pixels

        lv_obj_center(spinner_obj); // Align the spinner to the center of the screen
        lv_obj_set_style_arc_color(spinner_obj, lv_color_black(), LV_PART_MAIN); // LV_PART_MAIN is the background ring 
        lv_obj_set_style_arc_color(spinner_obj, lv_color_white(), LV_PART_INDICATOR);  // LV_PART_INDICATOR is the spinning portion
        
       
        lv_obj_set_style_arc_width(spinner_obj, 2, LV_PART_MAIN);
        lv_obj_set_style_arc_width(spinner_obj, 2, LV_PART_INDICATOR);
        lv_obj_add_flag(spinner_obj, LV_OBJ_FLAG_HIDDEN);
        // Release the mutex
        lvgl_port_unlock();
        return ESP_OK;
    

    // The task already created the UI and finished its job, it must delete itself.
    vTaskDelete(NULL);

}

void spinner_set_visible(bool visible)
{
    if (spinner_obj == NULL) {
        return;
    }
    if (lvgl_port_lock(0)) {
        if (visible) {
            lv_obj_clear_flag(spinner_obj, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(spinner_obj, LV_OBJ_FLAG_HIDDEN);
        }
        lvgl_port_unlock();
    }
}