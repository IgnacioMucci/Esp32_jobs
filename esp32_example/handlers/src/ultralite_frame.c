#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "ultralite_frame.h"
#include "esp_log.h"

static const char *TAG = "ultralite";

/* Topic table: must match TOPICS in ultralite_frame.py.
 * N may be anything from 1 to max_size (leading zero bytes can be omitted).
 * Index 0 is reserved: a query on it would be 0x00, the same as padding. */
static const ultralite_topic_t topic_table[] = {
    /* index  name         max   signed */
    {  1,     "Period",    4 },
    {  2,     "DeepSleep", 4 },
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

/* Stage 1: is the frame itself well formed?
 * Walks bytes 0..18 only to find where each command starts and ends, and counts them.
 * Topics and values are not checked: an unknown topic still counts as a command.
 * The count found must match the last byte. Any error rejects the whole frame. */
ultralite_frame_err_t ultralite_check_frame(const uint8_t frame[ULTRALITE_FRAME_LEN])
{
    size_t pos   = 0;   /* cursor over the frame */
    size_t found = 0;   /* commands found while walking */

    while (pos < ULTRALITE_CMD_AREA) {
        uint8_t header = frame[pos];

        /* 0x00 is never a header (index 0 is reserved): padding starts here. */
        if (header == 0x00) {
            break;
        }

        if ((header & 0x80) == 0) {
            /* Query: one byte, no length byte. */
            pos += 1;
        } else {
            /* Set: [header][N][N bytes]. N can not be byte 19 (that is the count). */
            if (pos + 1 >= ULTRALITE_CMD_AREA) {
                return ULTRALITE_FRAME_ERR_TRUNCATED_HEADER;
            }
            uint8_t n = frame[pos + 1];
            /* If it does not fit, the next command start is unknown. Also rejects N > 17. */
            if (pos + 2 + n > ULTRALITE_CMD_AREA) {
                return ULTRALITE_FRAME_ERR_OVERRUN;
            }
            pos += 2 + n;
        }
        found++;
    }

    /* After the last command, up to byte 18, everything must be padding. */
    for (size_t i = pos; i < ULTRALITE_CMD_AREA; i++) {
        if (frame[i] != 0x00) {
            return ULTRALITE_FRAME_ERR_DIRTY_PADDING;
        }
    }

    /* The last byte must state exactly how many commands were found. */
    if (found != frame[ULTRALITE_COUNT_POS]) {
        return ULTRALITE_FRAME_ERR_COUNT_MISMATCH;
    }
    return ULTRALITE_FRAME_OK;
}

/* Big-endian value bytes -> decimal text ("3600"), because PoMA commands are text.
 * Signed values are sign-extended from the first byte. out needs 22+ bytes. */
static void value_to_string(const ultralite_topic_t *topic, const uint8_t *params,
                            uint8_t len, char *out, size_t out_size)
{
    uint64_t value = 0;

    /* First byte is the most significant: 0E 10 -> 0x0E10 = 3600. */
    for (uint8_t i = 0; i < len; i++) {
        value = (value << 8) | params[i];
    }
    snprintf(out, out_size, "%llu", (unsigned long long)value);
    
}

/* Reason a command must be skipped, or NULL if it is fine.
 * Only what the frame can tell: getters/setters are PoMA's business. */
static const char *skip_reason(const ultralite_topic_t *topic, uint8_t index, bool is_set, uint8_t n)
{
    if (index == 0)                   return "reserved index 0";   /* only possible as 0x80 */
    if (topic == NULL)                return "unknown topic";
    if (is_set && n == 0)             return "empty set";
    if (is_set && n > topic->max_size) return "length exceeds topic size";
    return NULL;
}

/* Stage 2: one walk over the frame, writing each valid command as soon as it is found.
 * Same walk as stage 1, which already proved every command fits, so no bounds checks here. */
char *ultralite_to_text(const uint8_t frame[ULTRALITE_FRAME_LEN])
{
    /* Worst case is bounded (19 commands), so one malloc is enough. */
    char *text = malloc(ULTRALITE_MAX_TEXT);
    if (text == NULL) {
        return NULL;
    }
    text[0] = '\0';   /* valid empty string if every command is skipped */

    size_t pos   = 0;   /* cursor over the frame */
    size_t total = 0;   /* chars written so far */
    size_t num   = 0;   /* command number, for logs */

    while (pos < ULTRALITE_CMD_AREA && frame[pos] != 0x00) {
        /* Header byte: bit 7 = set/query, bits 6..0 = topic index. */
        bool    is_set = (frame[pos] & 0x80) != 0;
        uint8_t index  = frame[pos] & 0x7F;
        uint8_t n      = is_set ? frame[pos + 1] : 0;
        const uint8_t *params = &frame[pos + 2];          /* value stays in the frame, no copy */
        const ultralite_topic_t *topic = find_topic(index);

        /* Move to the next command BEFORE any skip, or the loop would get stuck. */
        pos += is_set ? 2 + n : 1;
        num++;

        /* Bad command: leave it out of the text, keep the rest. */
        const char *reason = skip_reason(topic, index, is_set, n);
        if (reason != NULL) {
            ESP_LOGW(TAG, "#%u skipped: %s", (unsigned)num, reason);
            continue;
        }

        /* Append at the end of the text; snprintf also writes the final '\0'. */
        char  *dst  = text + total;
        size_t room = ULTRALITE_MAX_TEXT - total;
        int    len;
        if (is_set) {
            char value[24];   /* sign + 20 digits + NUL */
            value_to_string(topic, params, n, value, sizeof(value));
            len = snprintf(dst, room, "=%s %s%c", topic->name, value, ULTRALITE_SEPARATOR);
        } else {
            len = snprintf(dst, room, "?%s%c", topic->name, ULTRALITE_SEPARATOR);
        }
        total += (size_t)len;
    }
    return text;
}

/* Stage 3: text -> vector. */
char **ultralite_split(char *text, size_t *count)
{
    /* Every command ends with ';', so commands = number of separators. */
    size_t n = 0;
    for (char *p = text; *p != '\0'; p++) {
        if (*p == ULTRALITE_SEPARATOR) {
            n++;
        }
    }
    *count = n;
    if (n == 0) {
        return NULL;   /* nothing to store, no malloc needed */
    }

    char **cmds = malloc(n * sizeof(char *));
    if (cmds == NULL) {
        return NULL;
    }

    /* Each slot points at a command inside text; its ';' becomes '\0' so every
     * slot reads as its own string. No command is copied. */
    char  *start = text;
    size_t i     = 0;
    for (char *p = text; *p != '\0'; p++) {
        if (*p == ULTRALITE_SEPARATOR) {
            *p = '\0';
            cmds[i++] = start;
            start = p + 1;
        }
    }
    return cmds;
}

/* Error codes -> text for logs. */
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