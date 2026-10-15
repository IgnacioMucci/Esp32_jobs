#ifndef NVS_PERIOD_H
#define NVS_PERIOD_H

// Reads the value stored;if it doesnt exits uses the default
esp_err_t period_load(uint64_t *out, uint64_t default_val);
// Changes the value if the value is diferent
esp_err_t period_save(uint64_t value);

#endif