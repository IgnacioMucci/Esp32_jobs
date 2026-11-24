#ifndef ULTRALITE_HANDLERS_H
#define ULTRALITE_HANDLERS_H

/* PoMA topic "ultralite" (setter only).
 * Usage:  =ultralite <40 hex characters>   (one 20-byte frame, hex encoded)
 * The topic itself only coordinates; the real work is done by the functions
 * in ultralite_frame.c and hex_utils.c. */
void setterUltralite(int sockfd, char *argument);

#endif