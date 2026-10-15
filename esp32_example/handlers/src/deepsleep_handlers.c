#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"     
#include "esp_sleep.h"
#include "deepsleep_handlers.h"
#include "string_check.h"
#include "nvs_period.h"

void setterDeepSleep(int sockfd, char *argument)
{   
    char response[25];
    uint32_t seconds; 

    if (check_String(argument, &seconds, response) != ESP_OK) {
        write(sockfd, response, strlen(response));
        return;
    }
    uint64_t time_us = seconds * 1000000ULL;

     if (seconds == 0) { 
        period_load(&time_us, 0); // Going to nvs to search for the value
        if (time_us == 0) {                    // nunca se seteó (o se seteó en 0)
            sprintf(response, "Value 0\n");
            write(sockfd, response, strlen(response));
            return;}
        sprintf(response, "Sleeping\n");    
        write(sockfd, response, strlen(response));
        vTaskDelay(pdMS_TO_TICKS(500));
        esp_sleep_enable_timer_wakeup(time_us);// Configure the time to sleep
        esp_deep_sleep_start();// start sleeping
        return;
     }
    sprintf(response, "Sleeping\n");
    write(sockfd, response, strlen(response));
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_sleep_enable_timer_wakeup(seconds * 1000000ULL);  // s -> us
    esp_deep_sleep_start(); 
}