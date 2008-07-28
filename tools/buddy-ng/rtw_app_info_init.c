/******************************************************************************
 *
 *           $RCSfile: msr_io.c,v $
 *           $Revision$
 *           $Author$
 *           $Date$
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
 *           $Log:$
 *
 ******************************************************************************/


#include "include/defines.h"
#include "rtmodel.h"

#include "include/rt_app.h"

RT_MODEL  rtM_;
RT_MODEL  *rtM = &rtM_;

BlockIO rtB;
/*Parameters rtP;*/

struct rt_app* get_app_info(void)
{
    capi_init();
    return app_info_init(rtM) ? NULL : &rt_app;
}

int main(void) 
{
    printf("%p\n", get_app_info());
    return 0;
}
