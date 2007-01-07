/******************************************************************************
 *
 *           $RCSfile: rtai_module.c,v $
 *           $Revision: 1.2 $
 *           $Author: rich $
 *           $Date: 2006/02/04 11:07:15 $
 *           $State: Exp $
 *
 *           Autor: Richard Hacker
 *
 *           (C) Copyright IgH 2005
 *           Ingenieurgemeinschaft IgH
 *           Heinz-Baecker Str. 34
 *           D-45356 Essen
 *           Tel.: +49 201/36014-0
 *           Fax.: +49 201/36014-14
 *           E-mail: info@igh-essen.com
 *
 * 	This file uses functions in mdl_wrapper.c to start the model.
 * 	This two layer concept is required because it is not (yet) possible
 * 	to compile the RTW generated C-code with kernel header files.
 *
 *           $Log: rtai_module.c,v $
 *
 *
 *****************************************************************************/ 

#include "mdl_wrapper.h"


int register_rtw_model(const struct rtw_model *rtw_model, size_t struct_len,
        const char *revision_str, struct module *owner);
void free_rtw_model(int tid);
