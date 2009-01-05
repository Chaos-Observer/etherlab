/**************************************************************************************************
*
*                          msr_messages.h
*
*           Nachrichten
*           
*           Autor: Wilhelm Hagemeister
*
*           (C) Copyright IgH 2001
*           Ingenieurgemeinschaft IgH
*           Heinz-Bäcker Str. 34
*           D-45356 Essen
*           Tel.: +49 201/61 99 31
*           Fax.: +49 201/61 98 36
*           E-mail: hm@igh-essen.com
*
*
*
*
**************************************************************************************************/


/*--Schutz vor mehrfachem includieren------------------------------------------------------------*/

#ifndef _MSR_MESSAGES_H_
#define _MSR_MESSAGES_H_

/*--includes-------------------------------------------------------------------------------------*/

/*--defines--------------------------------------------------------------------------------------*/

#define MSR_INFO "info"
#define MSR_ERROR "error"
#define MSR_WARN "warn"

/* infos */
#define MSR_DEVOPEN "num=\"1\" text=\"device open\""
#define MSR_DEVCLOSE "num=\"2\" text=\"device close\""

/* warnings */
#define MSR_UC "num=\"1000\" text=\"unknown command\"" 
#define MSR_WERROR  "num=\"2000\" text=\"error writing a parameter\"" 


/*--typedefs/structures--------------------------------------------------------------------------*/

/*--external functions---------------------------------------------------------------------------*/

/*--external data--------------------------------------------------------------------------------*/

/*--public data----------------------------------------------------------------------------------*/

/*--prototypes-----------------------------------------------------------------------------------*/

void msr_print_error(const char *format, ...);
void msr_print_warn(const char *format, ...);
void msr_print_info(const char *format, ...);

#endif 	// __H_













