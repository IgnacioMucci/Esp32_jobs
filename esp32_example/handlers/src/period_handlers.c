#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>            
#include "period_handlers.h"
#include "string_check.h"
#include "nvs_period.h"

uint64_t time_deepsleep = 0;

void setterPeriod(int sockfd, char *argument)
{   
    char response[25];
    uint32_t seconds;

   if (check_String(argument, &seconds, response) == ESP_OK) {
     time_deepsleep = seconds * 1000000ULL; // changes from secons to us
     period_save(time_deepsleep);   
    }    
    write(sockfd, response, strlen(response));
}

void getterPeriod(int sockfd, char *argument)
{
    char response[25];
    uint64_t period_seconds = time_deepsleep / 1000000ULL; // Chaging from us to s
    sprintf(response, "%llu\n", (unsigned long long)period_seconds);
    write(sockfd, response, strlen(response));
}