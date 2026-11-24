#ifndef ULTRALITE_FRAME_H
#define ULTRALITE_FRAME_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Frame (20 bytes): bytes 0..18 = commands + 0x00 padding, byte 19 = command count.
 *   query: [0|index]          set: [1|index] [N] [N value bytes, big-endian]
 *
 *   1) ultralite_check_frame() : structure + count byte. Error = nothing is produced.
 *   2) ultralite_to_text()     : valid commands -> "=Period 1;?Period;" (malloc'd).
 *   3) ultralite_split()       : text -> vector of commands (malloc'd).
 * Nothing is executed yet. */

#define ULTRALITE_FRAME_LEN 20
#define ULTRALITE_CMD_AREA  19   /* bytes 0..18 */
#define ULTRALITE_COUNT_POS 19   /* byte 19 */
#define ULTRALITE_MAX_CMDS  19   /* 19 one-byte queries */
#define ULTRALITE_SEPARATOR ';'

/* Worst-case text size, so stage 2 needs a single malloc. */
#define ULTRALITE_MAX_NAME     19   /* PoMA key[20] */
#define ULTRALITE_MAX_VALUE    20   /* digits of a 64-bit value (or sign + 19) */
#define ULTRALITE_MAX_CMD_TEXT (1 + ULTRALITE_MAX_NAME + 1 + ULTRALITE_MAX_VALUE + 1)   /* "=Name value;" */
#define ULTRALITE_MAX_TEXT     (ULTRALITE_MAX_CMDS * ULTRALITE_MAX_CMD_TEXT + 1)       /* + '\0' */

/* Errors that reject the whole frame. */
typedef enum {
    ULTRALITE_FRAME_OK = 0,
    ULTRALITE_FRAME_ERR_DIRTY_PADDING,
    ULTRALITE_FRAME_ERR_TRUNCATED_HEADER,  /* set header at byte 18 */
    ULTRALITE_FRAME_ERR_OVERRUN,           /* command does not fit in bytes 0..18 */
    ULTRALITE_FRAME_ERR_COUNT_MISMATCH,    /* commands found != byte 19 */
} ultralite_frame_err_t;

/* Maps a frame index to a PoMA topic name. */
typedef struct {
    uint8_t     index;      /* 1..127 */
    const char *name;       /* same key as in createTopic(), max ULTRALITE_MAX_NAME chars */
    uint8_t     max_size;   /* 1..8 value bytes */
} ultralite_topic_t;

/* Stage 1 */
ultralite_frame_err_t ultralite_check_frame(const uint8_t frame[ULTRALITE_FRAME_LEN]);

/* Stage 2: run stage 1 first. Returns a malloc'd string (caller frees), NULL if out of memory. */
char *ultralite_to_text(const uint8_t frame[ULTRALITE_FRAME_LEN]);

/* Stage 3: splits text in place at each ';'. Returns a malloc'd vector of pointers
 * into text (free the vector, then the text). NULL if *count is 0 or out of memory. */
char **ultralite_split(char *text, size_t *count);

const char *ultralite_frame_err_str(ultralite_frame_err_t err);

#endif // ULTRALITE_FRAME_H