#ifndef METHOD_TASK_H
#define METHOD_TASK_H

#include "esp_err.h"

/* Method 1: task that changes between the ball and the spinner with vTaskDelay.*/
esp_err_t method_task_start(void);
#endif