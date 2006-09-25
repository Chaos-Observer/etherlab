/* Public interface to module that talks to real time process 
 * Primarily used by buddy_main.c
 */

#include "buddy_main.h"
#include "fifo_defines.h"

/* Open all channels to real time process and do other initialisation */
int prepare_rtp(char *path);
