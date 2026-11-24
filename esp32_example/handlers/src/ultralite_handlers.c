#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "esp_log.h"
#include "ultralite_handlers.h"
#include "ultralite_frame.h"
#include "hex_utils.h"

static const char *TAG = "ultralite";

static void reply(int sockfd, const char *msg)
{
    write(sockfd, msg, strlen(msg));
}

/* Coordinates only: hex -> bytes, check, text, vector, show, free.
 * Nothing is executed yet: each command is just written to the socket and the log. */
void setterUltralite(int sockfd, char *argument)
{
    uint8_t frame[ULTRALITE_FRAME_LEN];

    /* Transport: PoMA is text based, so the frame arrives as 40 hex chars. */
    if (hex_to_bytes(argument, frame, sizeof(frame)) != ESP_OK) {
        ESP_LOGW(TAG, "Rejected: not 40 hex chars");
        reply(sockfd, "error\n");
        return;
    }
    ESP_LOG_BUFFER_HEX(TAG, frame, sizeof(frame));

    /* Stage 1: a broken frame produces nothing. */
    ultralite_frame_err_t err = ultralite_check_frame(frame);
    if (err != ULTRALITE_FRAME_OK) {
        ESP_LOGW(TAG, "Frame rejected: %s", ultralite_frame_err_str(err));
        reply(sockfd, "error\n");
        return;
    }

    /* Stage 2: frame -> "=Period 1;?Period;" */
    char *text = ultralite_to_text(frame);
    if (text == NULL) {
        ESP_LOGE(TAG, "Out of memory (text)");
        reply(sockfd, "error\n");
        return;
    }
    ESP_LOGI(TAG, "Text: %s", text);

    /* Stage 3: text -> vector. NULL with n > 0 means malloc failed. */
    size_t n   = 0;
    char **vec = ultralite_split(text, &n);
    if (vec == NULL && n > 0) {
        ESP_LOGE(TAG, "Out of memory (vector)");
        free(text);
        reply(sockfd, "error\n");
        return;
    }

    /* Show the vector: one command per line on the socket and in the log. */
    reply(sockfd, "done\n");
    for (size_t i = 0; i < n; i++) {
        ESP_LOGI(TAG, "vec[%u] = %s", (unsigned)i, vec[i]);
        reply(sockfd, vec[i]);
        reply(sockfd, "\n");
    }

    /* Vector first: its slots point into text. */
    free(vec);
    free(text);
}