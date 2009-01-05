/**************************************************************************************************
*
*                          msr_main.h
*
*           Init von RT-lib
*           
*           Autor: Wilhelm Hagemeister
*
*           (C) Copyright IgH 2002
*           Ingenieurgemeinschaft IgH
*           Heinz-BÅ‰cker Str. 34
*           D-45356 Essen
*           Tel.: +49 201/61 99 31
*           Fax.: +49 201/61 98 36
*           E-mail: hm@igh-essen.com
*
*
*           $RCSfile: msr_main.h,v $
*           $Revision: 1.12 $
*           $Author: hm $
*           $Date: 2006/05/12 12:39:46 $
*           $State: Exp $
*
*
*           $Log: msr_main.h,v $
*           Revision 1.12  2006/05/12 12:39:46  hm
*           *** empty log message ***
*
*           Revision 1.11  2006/03/29 13:57:57  hm
*           *** empty log message ***
*
*           Revision 1.10  2006/01/19 10:30:42  hm
*           *** empty log message ***
*
*           Revision 1.9  2006/01/12 13:41:01  hm
*           *** empty log message ***
*
*           Revision 1.8  2006/01/04 11:30:46  hm
*           *** empty log message ***
*
*           Revision 1.7  2005/12/22 11:11:42  hm
*           *** empty log message ***
*
*           Revision 1.6  2005/12/13 14:22:01  hm
*           *** empty log message ***
*
*           Revision 1.5  2005/09/19 16:46:07  hm
*           *** empty log message ***
*
*           Revision 1.4  2005/09/02 10:26:34  hm
*           *** empty log message ***
*
*           Revision 1.3  2005/08/24 16:49:18  hm
*           *** empty log message ***
*
*           Revision 1.2  2005/08/24 15:22:32  ab
*           *** empty log message ***
*
*           Revision 1.1  2005/06/17 11:35:34  hm
*           Initial revision
*
*
*
*
**************************************************************************************************/


/*--includes-------------------------------------------------------------------------------------*/

#ifndef _MSR_MAIN_H_
#define _MSR_MAIN_H_

#define MSR_ALL_VARIABLES 1


#include <stdlib.h>


extern unsigned int msr_flag;
							       
/*							       
***************************************************************************************************
*							       
* Function: msr_rtlib_init				       
*
* Beschreibung: Initialisierung der RTLIB
*
* Parameter: red: Untersetzung aller KanÅ‰le, die mit reg_kanal() registriert werden
*            hz: Abtastfrequenz mit der der Echtzeitprozess lÅ‰uft 
*            tbuf: Pufferzeit in sec fÅ¸r die KanÅ‰le
*            Zeiger auf eine Projektspezifische Initroutine
*
* RÅ¸ckgabe:  0 wenn alles ok, sonst < 0
*               
* Status: exp
*
***************************************************************************************************
*/

int msr_rtlib_init(int red,double hz,int tbuf,int (*prj_init)(void));


void msr_rtlib_cleanup(void);


//Ab hier User Space Schnittstelle

/*
*******************************************************************************
*
* Function: msr_init()
*
* Beschreibung: Initialisierung der msr_lib
*
*               
* Parameter: rtp:	Private data for RTP process. Must be passed on 
* 			when calling (*newparamflag)
*            newparamflag: Callback fé¸r Parmeteré‰nderung
*                          Ré¸ckgabe: 0: erfolgreich -1: nicht erfolgreich 
*                          Parameter fé¸r Callback: void * : *rtp from above
*                          			void *    : Startadresse, die geé‰ndert wurde
*                                                  size_t : Lé‰nge in byte die geé‰ndert wurden
*                          NULL: Funktion wird nicht aufgerufen						  
*               
*                          base_rate: Rate des Echtzeitprozesses in usec                
*                          base: Zeiger auf den Anfang des Kanalpuffers
*                          blocksize: Lé‰nge einer Zeitscheibe (in byte)
*                          buflen: Anzahl Zeitscheiben total (nach buflen wrappt der Schreibzeiger

* Ré¸ckgabe: 0: wenn ok, sonst -1
*               
* Status: 
*
*******************************************************************************
*/

int msr_init(void *_rtp, int (*_newparamflag)(void*,void*,size_t),unsigned long _base_rate,void *_base,unsigned int _blocksize,unsigned int _buflen,unsigned int _flags);


int msr_reg_time(void *time);
int msr_reg_task_stats(
        int tid,
        void *time, 
        void *exec_time,
        void *period,
        void *overrun);

/*
*******************************************************************************
*
* Function: msr_disconnect()
*
* Beschreibung: Schickt an alle angeschlossenen Clients ein disconnect commando
*
*               
* Parameter:
*               
* Ré¸ckgabe: 
*               
* Status: 
*
*******************************************************************************
*/

void msr_disconnect(void);


/*
*******************************************************************************
*
* Function: msr_cleanup()
*
* Beschreibung: Freigeben der msr_lib
*
*               
* Parameter:
*               
* Ré¸ckgabe: 
*               
* Status: 
*
*******************************************************************************
*/

void msr_cleanup(void);

/* A new image has arrived from the real time process. If there is new data
 * for a client, tell dispatcher by calling set_wfd(client_privdata) */
int msr_update(unsigned int wp);

#endif

















