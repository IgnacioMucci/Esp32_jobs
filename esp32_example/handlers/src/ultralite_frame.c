#include <stdint.h>
#include <stdio.h>
#include "ultralite_frame.h"
#include "esp_log.h"
#include "period_handlers.h"      /* getterPeriod / setterPeriod */
#include "deepsleep_handlers.h"   /* setterDeepSleep             */

static const char *TAG = "ultralite";
 
/*  Topic table
 * This table MUST match TOPICS
 * if the ground side and the device disagree, a frame writes the wrong topic.
 * Values travel as big-endian bytes. The N byte of a set tells how many bytes
 * the value really uses: anything from 1 up to the topic's "max_size" is valid
 * (leading zero bytes can be omitted), so small values cost fewer bytes.
 * max_size must be between 1 and 8.
 * Index 0 is reserved: a query on it would be 0x00, the same as padding. */
static const ultralite_topic_t topic_table[] = {
    /* index  name         max   signed  getter        setter          */
    {  1,     "Period",    4,    false,  getterPeriod, setterPeriod    },
    {  2,     "DeepSleep", 4,    false,  NULL,         setterDeepSleep }, /* set only */
};

#define TOPIC_COUNT (sizeof(topic_table) / sizeof(topic_table[0]))

/* Linear search of a topic by its index. NULL if it is not in the table. */
static const ultralite_topic_t *find_topic(uint8_t index)
{
    for (size_t i = 0; i < TOPIC_COUNT; i++) {
        if (topic_table[i].index == index) {
            return &topic_table[i];
        }
    }
    return NULL;
}

/* Stage 1: FIRST PARSE - is the frame itself well formed?                   
 * We walk the command area (bytes 0..18) with a cursor ("pos")
 * moving over 19 boxes. The only goal is to know where every command starts
 * and ends, so we can count them. We do not look at topics or values
 * a command with an unknown topic still counts as a command.
 * Then the count we found must be equal to the last byte of the frame. */
ultralite_frame_err_t ultralite_check_frame(const uint8_t frame[ULTRALITE_FRAME_LEN])
{
    size_t pos   = 0;
    size_t found = 0;   /* commands found while walking */

    while (pos < ULTRALITE_CMD_AREA) {
        uint8_t header = frame[pos];

        /* 0x00 can never be a command header (index 0 is reserved),
         * so it means "padding starts here". */
        if (header == 0x00) {
            break;
        }

        if ((header & 0x80) == 0) {
            /* Query: just this one byte, no length byte. */
            pos += 1;
        } else {
            /* Set: [header][N][N bytes]. The N byte must be inside the command
             * area: byte 19 belongs to the count, it can never be a length. */
            if (pos + 1 >= ULTRALITE_CMD_AREA) {
                return ULTRALITE_FRAME_ERR_TRUNCATED_HEADER;
            }
            uint8_t n = frame[pos + 1];
            /* The whole command (2 + N bytes) must fit in the command area. If it
             * does not, we cannot know where the next command starts: frame error.
             * (This also rejects any N > 17.) */
            if (pos + 2 + n > ULTRALITE_CMD_AREA) {
                return ULTRALITE_FRAME_ERR_OVERRUN;
            }
            pos += 2 + n;   /* jump over header, length byte and value */
        }
        found++;
    }

    /* From the end of the last command up to byte 18 everything must be padding (0x00). */
    for (size_t i = pos; i < ULTRALITE_CMD_AREA; i++) {
        if (frame[i] != 0x00) {
            return ULTRALITE_FRAME_ERR_DIRTY_PADDING;
        }
    }

    /* The last byte must say exactly how many commands we found. */
    if (found != frame[ULTRALITE_COUNT_POS]) {
        return ULTRALITE_FRAME_ERR_COUNT_MISMATCH;
    }
    return ULTRALITE_FRAME_OK;
}


/* Stage 2: SECOND PARSE - split the frame into a list of commands           
 * Looks at ONE command and says if it can be executed.
 * Here we know exactly where the command starts and ends (stage 1 guaranteed
 * it), so a problem only affects this command. Also fills cmd->topic. */
static ultralite_cmd_err_t classify_command(ultralite_cmd_t *cmd, uint8_t index)
{
    if (index == 0) {
        return ULTRALITE_CMD_ERR_RESERVED_INDEX;   /* only possible as a set: 0x80 */
    }
    cmd->topic = find_topic(index);
    if (cmd->topic == NULL) {
        return ULTRALITE_CMD_ERR_UNKNOWN_TOPIC;
    }

    if (!cmd->is_set) {
        return (cmd->topic->getter == NULL) ? ULTRALITE_CMD_ERR_NO_GETTER : ULTRALITE_CMD_OK;
    }

    if (cmd->topic->setter == NULL) {
        return ULTRALITE_CMD_ERR_NO_SETTER;
    }
    if (cmd->len == 0) {
        return ULTRALITE_CMD_ERR_EMPTY_SET;
    }
    if (cmd->len > cmd->topic->max_size) {   /* N may be SHORTER than max_size, never longer */
        return ULTRALITE_CMD_ERR_SIZE_TOO_BIG;
    }
    return ULTRALITE_CMD_OK;
}

/* Same walk as stage 1, but now we store every command (good or bad).
 * The frame structure was already proven, the few checks below only protect us from reading outside the frame if someone
 * calls this function without running stage 1 first. */
ultralite_frame_err_t ultralite_decode(const uint8_t frame[ULTRALITE_FRAME_LEN],
                                       ultralite_cmd_t cmds[ULTRALITE_MAX_CMDS],
                                       size_t *count)
{
    size_t pos    = 0;
    size_t n_cmds = 0;

    /* A 0x00 header means the padding started: nothing else to decode. */
    while (pos < ULTRALITE_CMD_AREA && frame[pos] != 0x00) {
        if (n_cmds >= ULTRALITE_MAX_CMDS) {
            return ULTRALITE_FRAME_ERR_OVERRUN;            
        }

        ultralite_cmd_t *cmd = &cmds[n_cmds];
        cmd->is_set = (frame[pos] & 0x80) != 0;
        uint8_t index = frame[pos] & 0x7F;
        cmd->topic  = NULL;
        cmd->params = NULL;     /* queries carry no value */
        cmd->len    = 0;

        if (cmd->is_set) {
            if (pos + 1 >= ULTRALITE_CMD_AREA) {
                return ULTRALITE_FRAME_ERR_TRUNCATED_HEADER; 
            }
            cmd->len = frame[pos + 1];
            if (pos + 2 + cmd->len > ULTRALITE_CMD_AREA) {
                return ULTRALITE_FRAME_ERR_OVERRUN;          
            }
            cmd->params = &frame[pos + 2];   /* points inside the frame */
            pos += 2 + cmd->len;
        } else {
            pos += 1;
        }

        cmd->err = classify_command(cmd, index);   /* a bad command does NOT stop the loop */
        n_cmds++;
    }

    *count = n_cmds;
    return ULTRALITE_FRAME_OK;
}

/* Stage 3: execution                                                         
 * Turns the big-endian value bytes of a set into a decimal string.
 * "len" is the N byte of the frame: it can be shorter than the topic's max_size.
 * Unsigned values are zero-extended; signed values are sign-extended from the
 * first byte (see below), so the sender can use the shortest length that fits.
 * The existing PoMA setters expect their argument as TEXT (e.g. setterPeriod
 * reads "3600" with strtoll), that is why we convert.
 * out must hold at least 22 bytes (sign + 20 digits + NUL). */
static void value_to_string(const ultralite_topic_t *topic, const uint8_t *params,
                            uint8_t len, char *out, size_t out_size)
{
    uint64_t value = 0;

    /* Big-endian: the FIRST byte is the most significant one.
     * Example: 0E 10 -> (0x0E << 8) | 0x10 = 0x0E10 = 3600. */
    for (uint8_t i = 0; i < len; i++) {
        value = (value << 8) | params[i];
    }

    if (topic->is_signed) {
        /* Sign extension for two's complement: if the top bit of the value is 1,
         * fill all the bytes above it with 1s so the 64-bit number keeps its sign.
         * Example (4 bytes): FF FF FF 06 -> 0xFFFFFF06 -> -250.
         * len is at most 8 (len <= max_size <= 8), so the shift stays below 64. */
        if (len < 8 && (params[0] & 0x80)) {
            value |= ~0ULL << (8 * len);
        }
        snprintf(out, out_size, "%lld", (long long)(int64_t)value);
    } else {
        snprintf(out, out_size, "%llu", (unsigned long long)value);
    }
}

void ultralite_execute(int sockfd, const ultralite_cmd_t *cmds, size_t count)
{
    /* Commands run strictly in fifo order, so a query
     * placed after a set on the same topic sees the new value.
     * A command with an error is skipped, and the loop goes on. */
    for (size_t i = 0; i < count; i++) {
        const ultralite_cmd_t *cmd = &cmds[i];

        if (cmd->err != ULTRALITE_CMD_OK) {
             ESP_LOGW(TAG, "#%u skipped: %s", (unsigned)(i + 1), ultralite_cmd_err_str(cmd->err));
            continue;   /* bad command: skip it, the rest still run */
        }

        if (!cmd->is_set) {
            /* Query: call the topic's getter. Getters ignore their argument. */
            ESP_LOGI(TAG, "#%u query %s", (unsigned)(i + 1), cmd->topic->name);
            cmd->topic->getter(sockfd, NULL);
        } else {
            /* Set: convert the value to text and call the topic's setter. */
            char argument[24];
            value_to_string(cmd->topic, cmd->params, cmd->len, argument, sizeof(argument));
            ESP_LOGI(TAG, "#%u set %s = %s", (unsigned)(i + 1), cmd->topic->name, argument);
            cmd->topic->setter(sockfd, argument);
        }
    }
}

/* ------------------------------------------------------------------------- */
const char *ultralite_frame_err_str(ultralite_frame_err_t err)
{
    switch (err) {
    case ULTRALITE_FRAME_OK:                    return "ok";
    case ULTRALITE_FRAME_ERR_DIRTY_PADDING:     return "dirty padding";
    case ULTRALITE_FRAME_ERR_TRUNCATED_HEADER:  return "truncated set header";
    case ULTRALITE_FRAME_ERR_OVERRUN:           return "record overruns frame";
    case ULTRALITE_FRAME_ERR_COUNT_MISMATCH:    return "command count mismatch";
    default:                                    return "unknown error";
    }
}

const char *ultralite_cmd_err_str(ultralite_cmd_err_t err)
{
    switch (err) {
    case ULTRALITE_CMD_OK:                 return "ok";
    case ULTRALITE_CMD_ERR_RESERVED_INDEX: return "reserved index 0";
    case ULTRALITE_CMD_ERR_UNKNOWN_TOPIC:  return "unknown topic";
    case ULTRALITE_CMD_ERR_NO_GETTER:      return "topic has no getter";
    case ULTRALITE_CMD_ERR_NO_SETTER:      return "topic has no setter";
    case ULTRALITE_CMD_ERR_EMPTY_SET:      return "empty set";
    case ULTRALITE_CMD_ERR_SIZE_TOO_BIG:   return "length exceeds topic size";
    default:                               return "unknown error";
    }
}