#ifndef ULTRALITE_FRAME_H
#define ULTRALITE_FRAME_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Frame (20 bytes): bytes 0..18 = commands + 0x00 padding, byte 19 = command count.
 *   query: [0|index]          set: [1|index] [N] [N value bytes, big-endian]
 * Frame errors reject everything; command errors only skip that command. */

#define ULTRALITE_FRAME_LEN 20
#define ULTRALITE_CMD_AREA  19   /* bytes 0..18 */
#define ULTRALITE_COUNT_POS 19   /* byte 19 */
#define ULTRALITE_MAX_N     17   /* 2 header bytes + 17 = 19 */
#define ULTRALITE_MAX_CMDS  19   /* 19 one-byte queries */

/* Errors that reject the whole frame. */
typedef enum {
    ULTRALITE_FRAME_OK = 0,
    ULTRALITE_FRAME_ERR_DIRTY_PADDING,
    ULTRALITE_FRAME_ERR_TRUNCATED_HEADER,  /* set header at byte 18 */
    ULTRALITE_FRAME_ERR_OVERRUN,           /* command does not fit in bytes 0..18 */
    ULTRALITE_FRAME_ERR_COUNT_MISMATCH,    /* commands found != byte 19 */
} ultralite_frame_err_t;

/* Errors that skip one command. */
typedef enum {
    ULTRALITE_CMD_OK = 0,
    ULTRALITE_CMD_ERR_RESERVED_INDEX,
    ULTRALITE_CMD_ERR_UNKNOWN_TOPIC,
    ULTRALITE_CMD_ERR_NO_GETTER,
    ULTRALITE_CMD_ERR_NO_SETTER,
    ULTRALITE_CMD_ERR_NOT_IN_POMA,
    ULTRALITE_CMD_ERR_EMPTY_SET,
    ULTRALITE_CMD_ERR_SIZE_TOO_BIG,
} ultralite_cmd_err_t;

/* Maps a frame index to a PoMA topic name. */
typedef struct {
    uint8_t     index;      /* 1..127 */
    const char *name;       /* same key as in createTopic() */
    uint8_t     max_size;   /* 1..8 value bytes */
    bool        is_signed;
} ultralite_topic_t;

typedef struct {
    bool                     is_set;
    const ultralite_topic_t *topic;
    void (*handler)(int, char *);   /* getter/setter found in PoMA */
    const uint8_t           *params; /* points into the frame */
    uint8_t                  len;    /* N */
    ultralite_cmd_err_t      err;
} ultralite_cmd_t;

struct Topic;   /* defined in poma.h */

/* Call once from main after registering all topics. */
void ultralite_init(struct Topic *topics);

/* Stage 1: checks structure and count byte. */
ultralite_frame_err_t ultralite_check_frame(const uint8_t frame[ULTRALITE_FRAME_LEN]);

/* Stage 2: lists all commands (good and bad). Run stage 1 first. */
ultralite_frame_err_t ultralite_decode(const uint8_t frame[ULTRALITE_FRAME_LEN],
                                       ultralite_cmd_t cmds[ULTRALITE_MAX_CMDS],
                                       size_t *count);

/* Stage 3: runs the commands without errors, in order. */
void ultralite_execute(int sockfd, const ultralite_cmd_t *cmds, size_t count);

const char *ultralite_frame_err_str(ultralite_frame_err_t err);
const char *ultralite_cmd_err_str(ultralite_cmd_err_t err);

#endif