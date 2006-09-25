/**************************************************************************************************
*
*                          msr_main.c
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
*           $RCSfile: msr_main.c,v $
*           $Revision: 1.17 $
*           $Author: hm $
*           $Date: 2006/05/12 12:40:05 $
*           $State: Exp $
*
*
*           $Log: msr_main.c,v $
*           Revision 1.17  2006/05/12 12:40:05  hm
*           *** empty log message ***
*
*           Revision 1.16  2006/04/19 09:19:29  hm
*           *** empty log message ***
*
*           Revision 1.15  2006/03/29 13:58:00  hm
*           *** empty log message ***
*
*           Revision 1.14  2006/01/20 16:30:17  hm
*           *** empty log message ***
*
*           Revision 1.13  2006/01/19 10:35:15  hm
*           *** empty log message ***
*
*           Revision 1.12  2006/01/12 13:40:50  hm
*           *** empty log message ***
*
*           Revision 1.11  2006/01/04 11:31:04  hm
*           *** empty log message ***
*
*           Revision 1.10  2006/01/02 10:39:15  hm
*           *** empty log message ***
*
*           Revision 1.9  2005/12/23 14:51:08  hm
*           *** empty log message ***
*
*           Revision 1.8  2005/12/12 17:22:25  hm
*           *** empty log message ***
*
*           Revision 1.7  2005/09/19 16:45:57  hm
*           *** empty log message ***
*
*           Revision 1.6  2005/08/24 16:50:02  hm
*           *** empty log message ***
*
*           Revision 1.5  2005/08/24 08:05:50  ab
*           *** empty log message ***
*
*           Revision 1.4  2005/07/26 08:43:08  ab
*           *** empty log message ***
*
*           Revision 1.3  2005/07/12 13:50:44  ab
*           *** empty log message ***
*
*           Revision 1.2  2005/07/01 16:09:36  hm
*           *** empty log message ***
*
*           Revision 1.1  2005/06/17 11:35:20  hm
*           Initial revision
*
*
*
*
**************************************************************************************************/


/*--includes-------------------------------------------------------------------------------------*/
 


#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <msr_lists.h>
#include <msr_charbuf.h>
#include <msr_reg.h>
#include <msr_messages.h>
#include <msr_utils.h>
#include <msr_main.h>

//fé¸r Userspace Applikation RTW
#ifdef RTW_BUDDY
#include "buddy_main.h"
#endif

/*--external functions---------------------------------------------------------------------------*/

/*--external data--------------------------------------------------------------------------------*/

struct msr_dev *msr_dev_head = NULL;  

void *prtp,*krtp;

int (*newparamflag)(void*, char*, size_t) = NULL;


extern struct msr_char_buf *msr_kanal_puffer;

extern unsigned int msr_kanal_write_pointer; /* Schreibzeiger auf den Kanalringpuffer */

static double msr_sample_freq = 1.0;

void msr_rtlib_cleanup(void)
{

    /* EintrÅ‰ge in den Listen lÅˆschen */
    msr_print_info("msr_control: cleanup metas...");
    msr_clean_meta_list();

    msr_print_info("msr_control: cleanup parameters...");
    msr_clean_param_list();

    msr_print_info("msr_control: cleanup channels...");
    msr_clean_kanal_list();

//    msr_print_info("msr_control: cleanup errorlists...");
//    msr_clean_error_list();
    msr_print_info("msr_control: cleanup lists...");

    //und die Listen lÅˆschen
    msr_lists_cleanup();    
    msr_print_info("msr_control: cleanup parameters, channels, errors done.");

}
 
#ifndef __KERNEL__

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
*                          			char *    : Startadresse, die geé‰ndert wurde
*                                                  size_t : Lé‰nge in byte die geé‰ndert wurden
*                          NULL: Funktion wird nicht aufgerufen						  
*               
*                          base_rate: Rate des Echtzeitprozesses in usec                
*                          base: Zeiger auf den Anfang des Kanalpuffers
*                          blocksize: Lé‰nge einer Zeitscheibe (in byte)
*                          buflen: Anzahl Zeitscheiben total (bis zum wrap des Schreibzeigers)

* Ré¸ckgabe: 0: wenn ok, sonst -1
*               
* Status: 
*
*******************************************************************************
*/


int msr_init(void *_rtp, int (*_newparamflag)(void*,char*,size_t),unsigned long _base_rate,void *_base,unsigned int _blocksize,unsigned int _buflen){

    int result = 0;
    /* Save data for real time communications client */
    prtp = _rtp;
    newparamflag = _newparamflag;

    
    msr_sample_freq = 1.0e6/(double)_base_rate;

    msr_print_info("msr_modul: init of rt_lib...");

    result = msr_lists_init();
    if (result < 0) {
        msr_print_warn("msr_modul: can't initialize Lists!");
        return result;
    }

    msr_init_kanal_params(_base_rate,_base,_blocksize,_buflen);

    printf("msrio:init: baserate: %lu, blocksize: %u, buflen: %u\n",_base_rate,_blocksize,_buflen);  //BDG
    //Registrierung von wichtigen KanÅ‰len und Parametern, die immer gebraucht werden
    /* als erste Kané‰le immer die Zeit registrieren, wichtig fé¸r die Datenablage */

    /* Im der Matlab RTW-Variante muéﬂ dafé¸r gesorgt werden, daéﬂ zumindest 
       /Time 
       als Kanal
       vorhanden ist.
    */

    /* als erster Parameter die Abtastfreqenz */
    msr_reg_param("/Taskinfo/Abtastfrequenz","Hz",&msr_sample_freq,TDBL);

    return 0; /* succeed */
}

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

void msr_disconnect(void){
    struct msr_dev *dev;

    FOR_THE_LIST(dev,msr_dev_head) 
	if(dev) {
	    dev->disconnectflag = 1;  //disconnect markieren
	    set_wfd(dev->client_wfd);
	}
}

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

void msr_cleanup(void){
    msr_rtlib_cleanup();
}


/* A new image has arrived from the real time process. If there is new data
 * for a client, tell dispatcher by calling set_wfd(client_privdata) */
int msr_update(unsigned int wp){

    struct msr_dev *element = NULL;

    static int counter = 0;

    msr_kanal_write_pointer = wp;
//    printf("msriolib:wp:%d\n",wp);
//    msr_write_kanal_list();  //Kanallisten beschreiben

    //im 20 Hz-(Echtzeit)Takt alle Listen durchlaufen und testen, ob Daten angefallen sind
    if(++counter >= (unsigned int)(msr_sample_freq/20)) { 
	counter = 0;
	FOR_THE_LIST(element,msr_dev_head) {
#ifdef RTW_BUDDY
	    if(msr_len_rb(element,2) > 0)
		set_wfd(element->client_wfd); 
#endif
	}

    }
    return 0; //FIXME, was wird zuré¸ckgegeben ?????
}

#endif















