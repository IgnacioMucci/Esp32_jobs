#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "esp_log.h"
#include "ultralite_handlers.h"
#include "ultralite_frame.h"
#include "hex_utils.h"

/* Inner handlers must not answer through the client socket: the client expects
 * ONE line per command, and that line is ultralite's "done".
 * -1 is an invalid descriptor, so their write() calls fail silently. */
#define ULTRALITE_NO_REPLY_FD (-1)

static const char *TAG = "ultralite";



/* Writes a NUL-terminated message to the client socket. */
static void reply(int sockfd, const char *msg)
{
    write(sockfd, msg, strlen(msg));
}

/* Replies "error: <reason>" and logs it. Used when the WHOLE frame is rejected. */
static void reject_frame(int sockfd, ultralite_frame_err_t err)
{
    char response[48];
    ESP_LOGW(TAG, "Frame rejected, nothing executed: %s", ultralite_frame_err_str(err));
    snprintf(response, sizeof(response), "done\n");
    reply(sockfd, response);
}

/* Topic setter. It only COORDINATES: it never parses bytes itself.
 *
 *   step 0 : transport    -> hex text (PoMA is text based) to 20 raw bytes
 *   stage 1: check_frame  -> FIRST PARSE: is the frame well formed and does the count byte match?
 *                            If not, the whole frame is rejected and nothing is executed.
 *   stage 2: decode       -> SECOND PARSE: split into a list of commands, each one checked on its own
 *   stage 3: execute      -> run the good commands in order; bad commands are skipped */
void setterUltralite(int sockfd, char *argument)
{
    uint8_t         frame[ULTRALITE_FRAME_LEN];
    ultralite_cmd_t cmds[ULTRALITE_MAX_CMDS];
    size_t          count = 0;
    char            response[64];

    /* Step 0: "83020E10..." (40 chars) -> 20 bytes. A NULL argument, a wrong
     * length or a non-hex character is rejected here. */
    if (hex_to_bytes(argument, frame, sizeof(frame)) != ESP_OK) {
        ESP_LOGW(TAG, "Rejected: argument is not 40 hex characters");
        reply(sockfd, "done\n");
        return;
    }
    ESP_LOG_BUFFER_HEX(TAG, frame, sizeof(frame));   /* print the 20 bytes in the log */

    /* Stage 1: first parse (structure + command count). */
    ultralite_frame_err_t ferr = ultralite_check_frame(frame);
    if (ferr != ULTRALITE_FRAME_OK) {
        reject_frame(sockfd, ferr);
        return;
    }

    /* Stage 2: second parse (list the commands). */
    ferr = ultralite_decode(frame, cmds, &count);
    if (ferr != ULTRALITE_FRAME_OK) {
        reject_frame(sockfd, ferr);
        return;
    }

    /* How many commands are bad? */
    size_t skipped = 0;
    for (size_t i = 0; i < count; i++) {
        if (cmds[i].err != ULTRALITE_CMD_OK) {
            skipped++;
        }
    }

    /* Tell the client the frame was accepted BEFORE executing: some commands
     * (like DeepSleep) never return because the device goes to sleep. */
    if (skipped == 0) {
        snprintf(response, sizeof(response), "done\n");
    } else {
        snprintf(response, sizeof(response), "done\n");
    }
    reply(sockfd, response);
    
    /* Report each skipped command: its position in the frame (1-based) and why. */
    for (size_t i = 0; i < count; i++) {
        if (cmds[i].err != ULTRALITE_CMD_OK) {
            ESP_LOGW(TAG, "Command #%u skipped: %s", (unsigned)(i + 1), ultralite_cmd_err_str(cmds[i].err));
        }
    }

    /* Stage 3: execution (bad commands are skipped inside). Right now is called with ULTRALITE_NO_REPLY_FD so that
    * it doesn't reply through the client socket. To allow it change it for sockfd */
    ultralite_execute(ULTRALITE_NO_REPLY_FD, cmds, count);
}