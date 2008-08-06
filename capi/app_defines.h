/******************************************************************************
 *
 * $Id$
 *
 *****************************************************************************/

#ifndef _APP_DEFINES_H
#define _APP_DEFINES_H

#define EXPAND_STR(S) #S
#define STR(S) EXPAND_STR(S)

#define EXPAND_CONCAT(name1,name2) name1 ## name2
#define CONCAT(name1,name2) EXPAND_CONCAT(name1,name2)

/*****************************************************************************/

#endif /* _APP_DEFINES_H */
