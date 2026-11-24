#include <stdint.h>
#include <stdio.h>
#include "esp_log.h"
#include "poma.h"
#include "ultralite_frame.h"

static const char *TAG = "ultralite";

static Topic *s_topics = NULL;   /* PoMA topic list, set by ultralite_init() */

void ultralite_init(Topic *topics)
{
    s_topics = topics;
}

/* Must match TOPICS in ultralite_frame.py. Index 0 is reserved (looks like padding). */
static const ultralite_topic_t topic_table[] = {
    /* index  name         max   signed */
    {  1,     "Period",    4,    false  },
    {  2,     "DeepSleep", 4,    false  },
};

#define TOPIC_COUNT (sizeof(topic_table) / sizeof(topic_table[0]))

static const ultralite_topic_t *find_topic(uint8_t index)
{
    for (size_t i = 0; i < TOPIC_COUNT; i++) {
        if (topic_table[i].index == index) {
            return &topic_table[i];
        }
    }
    return NULL;
}

/* ---- Stage 1: structure + count ---- */

ultralite_frame_err_t ultralite_check_frame(const uint8_t frame[ULTRALITE_FRAME_LEN])
{
    size_t pos   = 0;
    size_t found = 0;

    while (pos < ULTRALITE_CMD_AREA && frame[pos] != 0x00) {   /* 0x00 = padding starts */
        if ((frame[pos] & 0x80) == 0) {
            pos += 1;                                          /* query */
        } else {
            if (pos + 1 >= ULTRALITE_CMD_AREA) {               /* N would be byte 19 */
                return ULTRALITE_FRAME_ERR_TRUNCATED_HEADER;
            }
            uint8_t n = frame[pos + 1];
            if (pos + 2 + n > ULTRALITE_CMD_AREA) {            /* also rejects N > 17 */
                return ULTRALITE_FRAME_ERR_OVERRUN;
            }
            pos += 2 + n;                                      /* set */
        }
        found++;
    }

    for (size_t i = pos; i < ULTRALITE_CMD_AREA; i++) {
        if (frame[i] != 0x00) {
            return ULTRALITE_FRAME_ERR_DIRTY_PADDING;
        }
    }

    if (found != frame[ULTRALITE_COUNT_POS]) {
        return ULTRALITE_FRAME_ERR_COUNT_MISMATCH;
    }
    return ULTRALITE_FRAME_OK;
}

/* ---- Stage 2: list commands ---- */

/* Checks one command and resolves its handler in PoMA. */
static ultralite_cmd_err_t classify_command(ultralite_cmd_t *cmd, uint8_t index)
{
    if (index == 0) {
        return ULTRALITE_CMD_ERR_RESERVED_INDEX;
    }
    cmd->topic = find_topic(index);
    if (cmd->topic == NULL) {
        return ULTRALITE_CMD_ERR_UNKNOWN_TOPIC;
    }
    if (s_topics == NULL) {
        return ULTRALITE_CMD_ERR_NOT_IN_POMA;   /* ultralite_init() not called */
    }

    /* findX returns defaultX if the name is missing, NULL if the topic lacks that handler. */
    char *key = (char *)cmd->topic->name;

    if (!cmd->is_set) {
        void (*getter)(int, char *) = findGetter(s_topics, key);
        if (getter == defaultGetter) return ULTRALITE_CMD_ERR_NOT_IN_POMA;
        if (getter == NULL)          return ULTRALITE_CMD_ERR_NO_GETTER;
        cmd->handler = getter;
        return ULTRALITE_CMD_OK;
    }

    void (*setter)(int, char *) = findSetter(s_topics, key);
    if (setter == defaultSetter) return ULTRALITE_CMD_ERR_NOT_IN_POMA;
    if (setter == NULL)          return ULTRALITE_CMD_ERR_NO_SETTER;
    cmd->handler = setter;

    if (cmd->len == 0) {
        return ULTRALITE_CMD_ERR_EMPTY_SET;
    }
    if (cmd->len > cmd->topic->max_size) {
        return ULTRALITE_CMD_ERR_SIZE_TOO_BIG;
    }
    return ULTRALITE_CMD_OK;
}

ultralite_frame_err_t ultralite_decode(const uint8_t frame[ULTRALITE_FRAME_LEN],
                                       ultralite_cmd_t cmds[ULTRALITE_MAX_CMDS],
                                       size_t *count)
{
    size_t pos    = 0;
    size_t n_cmds = 0;

    while (pos < ULTRALITE_CMD_AREA && frame[pos] != 0x00) {
        if (n_cmds >= ULTRALITE_MAX_CMDS) {
            return ULTRALITE_FRAME_ERR_OVERRUN;              /* defensive */
        }

        ultralite_cmd_t *cmd = &cmds[n_cmds];
        cmd->is_set  = (frame[pos] & 0x80) != 0;
        cmd->topic   = NULL;
        cmd->handler = NULL;
        cmd->params  = NULL;
        cmd->len     = 0;
        uint8_t index = frame[pos] & 0x7F;

        if (cmd->is_set) {
            if (pos + 1 >= ULTRALITE_CMD_AREA) {
                return ULTRALITE_FRAME_ERR_TRUNCATED_HEADER; /* defensive */
            }
            cmd->len = frame[pos + 1];
            if (pos + 2 + cmd->len > ULTRALITE_CMD_AREA) {
                return ULTRALITE_FRAME_ERR_OVERRUN;          /* defensive */
            }
            cmd->params = &frame[pos + 2];
            pos += 2 + cmd->len;
        } else {
            pos += 1;
        }

        cmd->err = classify_command(cmd, index);   /* a bad command does not stop the loop */
        n_cmds++;
    }

    *count = n_cmds;
    return ULTRALITE_FRAME_OK;
}

/* ---- Stage 3: execute ---- */

/* Big-endian bytes -> decimal text (setters expect text). Sign-extends signed values. */
static void value_to_string(const ultralite_topic_t *topic, const uint8_t *params,
                            uint8_t len, char *out, size_t out_size)
{
    uint64_t value = 0;

    for (uint8_t i = 0; i < len; i++) {
        value = (value << 8) | params[i];
    }

    if (topic->is_signed) {
        if (len < 8 && (params[0] & 0x80)) {
            value |= ~0ULL << (8 * len);   /* FF FF FF 06 -> -250 */
        }
        snprintf(out, out_size, "%lld", (long long)(int64_t)value);
    } else {
        snprintf(out, out_size, "%llu", (unsigned long long)value);
    }
}

void ultralite_execute(int sockfd, const ultralite_cmd_t *cmds, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        const ultralite_cmd_t *cmd = &cmds[i];

        if (cmd->err != ULTRALITE_CMD_OK) {
            ESP_LOGW(TAG, "#%u skipped: %s", (unsigned)(i + 1), ultralite_cmd_err_str(cmd->err));
            continue;
        }

        if (!cmd->is_set) {
            ESP_LOGI(TAG, "#%u query %s", (unsigned)(i + 1), cmd->topic->name);
            cmd->handler(sockfd, NULL);
        } else {
            char argument[24];
            value_to_string(cmd->topic, cmd->params, cmd->len, argument, sizeof(argument));
            ESP_LOGI(TAG, "#%u set %s = %s", (unsigned)(i + 1), cmd->topic->name, argument);
            cmd->handler(sockfd, argument);
        }
    }
}

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
    case ULTRALITE_CMD_ERR_NOT_IN_POMA:    return "topic not registered in PoMA";
    case ULTRALITE_CMD_ERR_EMPTY_SET:      return "empty set";
    case ULTRALITE_CMD_ERR_SIZE_TOO_BIG:   return "length exceeds topic size";
    default:                               return "unknown error";
    }
}