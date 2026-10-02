#ifndef METHOD_EVENT_GROUP_H
#define METHOD_EVENT_GROUP_H

#include "esp_err.h"

/* Method 3: esp_timer + event group + task that waits the bit of the event group to change */
esp_err_t method_event_group_start(void);
#endif