idf_component_register(
    SRCS
        "method_task.c"
        "method_timer.c"
        "method_event_group.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES ball spinner esp_lvgl_port lvgl esp_timer freertos
)