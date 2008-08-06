/*********************************************************************
* $Id$
*
* These are the internal data types that are known to EtherLab.
* 
**********************************************************************/

#ifndef ETLDATATYPE_H
#define ETLDATATYPE_H

#include <linux/types.h>

typedef  uint8_t uint8_T;
typedef   int8_t sint8_T;
typedef uint16_t uint16_T;
typedef  int16_t sint16_T;
typedef uint32_t uint32_T;
typedef  int32_t sint32_T;
typedef uint64_t uint64_T;
typedef  int64_t sint64_T;
typedef float    single_T;
typedef double   double_T;
typedef struct {
    double re;
    double im;
} complex_T;

#endif // ETLDATATYPE_H
