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
        if( argument != NULL && strcmp(argument, "0") == 0){
        uint64_t time_us;    
        period_load(&time_us, 0); // Going to nvs to search for the value
        sprintf(response, "Sleeping\n");    
        write(sockfd, response, strlen(response));
        vTaskDelay(pdMS_TO_TICKS(500));
        esp_sleep_enable_timer_wakeup(time_us);// Configure the time to sleep
        esp_deep_sleep_start();// start sleeping
        return;
        } 
        if (check_String(argument, &seconds, response) != ESP_OK) {
        write(sockfd, response, strlen(response));
        return;
    }
    sprintf(response, "Sleeping\n");
    write(sockfd, response, strlen(response));
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_sleep_enable_timer_wakeup(seconds * 1000000ULL);  // s -> us
    esp_deep_sleep_start(); 
}