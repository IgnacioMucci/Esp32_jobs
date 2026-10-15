#include <stdint.h>
#include "esp_err.h"
#include "nvs.h"

#define NVS_NAMESPACE "poma"
#define NVS_KEY_PERIOD "period"


// Reads the value stored;if it doesnt exits uses the default
esp_err_t period_load(uint64_t *out, uint64_t default_val)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &h);
    if (err != ESP_OK) {  //namespace still doesnt exists
        *out = default_val;
        return err;
    }
    err = nvs_get_u64(h, NVS_KEY_PERIOD, out);
    if (err != ESP_OK) {    // Didnt find the value
        *out = default_val;
    }
    nvs_close(h);
    return err;
}

// Changes the value if the value is diferent
esp_err_t period_save(uint64_t value)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;

    uint64_t stored;
    if (nvs_get_u64(h, NVS_KEY_PERIOD, &stored) == ESP_OK && stored == value) { // IF the value that im trying to store is the same that was stored
        nvs_close(h);
        return ESP_OK;                // I dont change it
    }
    err = nvs_set_u64(h, NVS_KEY_PERIOD, value); // changes the value
    if (err == ESP_OK) err = nvs_commit(h); // Puts it in the flash
    nvs_close(h);
    return err;
}