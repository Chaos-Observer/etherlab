/* Define the RTAI fifo channels that will be used */

#include "defines.h"

#ifndef _FIFO_DEFINES_H
#define _FIFO_DEFINES_H

#define RTP_FIO 1		/* fifo number for parameter io */
#define RTB_FIO 2		/* fifo number for message io */

#define FIFO(F) STR(CONCAT(/dev/rtw,F))

#endif
