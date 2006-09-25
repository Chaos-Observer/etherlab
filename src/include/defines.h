/******************************************************************************
 *
 *           $RCSfile: defines.h,v $
 *           $Revision: 1.1 $
 *           $Author: rich $
 *           $Date: 2006/02/21 07:46:46 $
 *           $State: Exp $
 *
 *           Autor: Richard Hacker
 *
 *           (C) Copyright IgH 2005
 *           Ingenieurgemeinschaft IgH
 *           Heinz-Bäcker Str. 34
 *           D-45356 Essen
 *           Tel.: +49 201/61 99 31
 *           Fax.: +49 201/61 98 36
 *           E-mail: info@igh-essen.com
 *
 *           $Log: defines.h,v $
 *           Revision 1.1  2006/02/21 07:46:46  rich
 *           Initial revision
 *
 *
 */ 

#ifndef _DEFINES_H
#define _DEFINES_H

#define EXPAND_STR(S) #S
#define STR(S) EXPAND_STR(S)

#define EXPAND_CONCAT(name1,name2) name1 ## name2
#define CONCAT(name1,name2) EXPAND_CONCAT(name1,name2)
#define HEADER CONCAT(MODEL,_capi.h)
#define CAPI_HEADER STR(HEADER)

#define CAPI_INIT       CONCAT(MODEL,_InitializeDataMapInfo)
#define rtM	        CONCAT(MODEL,_M)
#define RT_MODEL        CONCAT(MODEL,_rtModel)

#endif
