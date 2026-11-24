#ifndef ULTRALITE_FRAME_H
#define ULTRALITE_FRAME_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*
 * UltraLite frame.
 *
 * A frame is ALWAYS 20 bytes:
 *
 *   bytes 0..18 : "command area" (19 bytes): commands back to back, then 0x00 padding
 *   byte  19    : COUNT BYTE = number of commands in the frame. Nothing else may live here.
 *
 *   query : 1 byte    -> [0 | index(7 bits)]
 *   set   : 2+N bytes -> [1 | index(7 bits)] [N(1 byte)] [N value bytes]      (N = 1..17)
 *
 * The frame is processed in three stages:
 *
 *   1) ultralite_check_frame() : FIRST PARSE. Walks the command area only to find where
 *                                 each command starts and ends, counts them and compares the
 *                                 result with the count byte. If the frame itself is broken,
 *                                 NOTHING is executed.
 *   2) ultralite_decode()      : SECOND PARSE. Splits the frame into a list of commands and
 *                                 checks each command on its own (topic exists, length OK...).
 *   3) ultralite_execute()     : runs the commands that are OK, in order. A bad command is
 *                                 skipped, but it never stops the other commands.
 *
 * "Frame error" and  "command error":
 * Frame error  = we cannot trust the structure so the whole frame is rejected.
 * Command error = we know exactly where the command starts and ends, but its content is not usable so only that command is skipped.
 * None of these functions know about hex text or sockets parsing: they work on raw bytes, so they can be reused later with the real frame coming from the UltraLite module.
 */

#define ULTRALITE_FRAME_LEN 20   // a frame is exactly 20 bytes                           
#define ULTRALITE_CMD_AREA  19   // bytes 0..18: commands + padding                       
#define ULTRALITE_COUNT_POS 19   // byte 19: number of commands                           
#define ULTRALITE_MAX_N     17   // biggest value block of a set (2 header + 17 = 19)     
#define ULTRALITE_MAX_CMDS  19   // worst case: 19 one-byte queries                       

// Errors that reject the WHOLE frame (stage 1, or defensive checks in stage 2). 
typedef enum {
    ULTRALITE_FRAME_OK = 0,
    ULTRALITE_FRAME_ERR_DIRTY_PADDING,     // non-zero byte after the padding started          
    ULTRALITE_FRAME_ERR_TRUNCATED_HEADER,  // set header at byte 18: its length byte is missing 
    ULTRALITE_FRAME_ERR_OVERRUN,           // a command does not fit in bytes 0..18            
    ULTRALITE_FRAME_ERR_COUNT_MISMATCH,    // commands found != value of the count byte        
} ultralite_frame_err_t;

// Errors of ONE command: that command is skipped, the others still run. 
typedef enum {
    ULTRALITE_CMD_OK = 0,
    ULTRALITE_CMD_ERR_RESERVED_INDEX,   //topic index 0 (header 0x80)                  
    ULTRALITE_CMD_ERR_UNKNOWN_TOPIC,    // index not present in the topic table         
    ULTRALITE_CMD_ERR_NO_GETTER,        // query on a topic that has no getter          
    ULTRALITE_CMD_ERR_NO_SETTER,        // set on a topic that has no setter            
    ULTRALITE_CMD_ERR_EMPTY_SET,        // set with N == 0                              
    ULTRALITE_CMD_ERR_SIZE_TOO_BIG,     // N is bigger than the topic's max_size        
} ultralite_cmd_err_t;

// One entry of the topic table: how the ground side addresses a PoMA topic. 
typedef struct {
    uint8_t     index;      // 1..127 (0 is reserved, it looks like padding)  
    const char *name;       // only used for logs                            
    uint8_t     max_size;   // a set may carry 1..max_size value bytes (max 8)*/
    bool        is_signed;  // true: value is two's complement (e.g. int32)   
    void      (*getter)(int, char *);   // NULL = topic cannot be queried     
    void      (*setter)(int, char *);   // NULL = topic cannot be set         
} ultralite_topic_t;

// One decoded command. 
typedef struct {
    bool                     is_set;  // false = query, true = set                       
    const ultralite_topic_t *topic;   // NULL if the topic is unknown / index 0          
    const uint8_t           *params;  // points INSIDE the frame (sets only)             
    uint8_t                  len;     // N: value bytes (0 for queries)                  
    ultralite_cmd_err_t      err;     // ULTRALITE_CMD_OK = it will be executed          
} ultralite_cmd_t;

// Stage 1 (first parse): ULTRALITE_FRAME_OK only if the structure and the count byte are right. 
ultralite_frame_err_t ultralite_check_frame(const uint8_t frame[ULTRALITE_FRAME_LEN]);

// Stage 2 (second parse): fills cmds[] (ALL commands, good and bad) and *count.
// The frame MUST have passed stage 1. cmds[] points into "frame", so keep the frame alive. 
ultralite_frame_err_t ultralite_decode(const uint8_t frame[ULTRALITE_FRAME_LEN],
                                       ultralite_cmd_t cmds[ULTRALITE_MAX_CMDS],
                                       size_t *count);

// Stage 3: runs, in order and one at a time, the commands whose err is ULTRALITE_CMD_OK.
// sockfd is handed to the existing PoMA getters/setters so they can answer. 
void ultralite_execute(int sockfd, const ultralite_cmd_t *cmds, size_t count);

// readable text for the error codes (used in the responses). 
const char *ultralite_frame_err_str(ultralite_frame_err_t err);
const char *ultralite_cmd_err_str(ultralite_cmd_err_t err);

#endif