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
*           Heinz-B��cker Str. 34
*           D-45356 Essen
*           Tel.: +49 201/61 99 31
*           Fax.: +49 201/61 98 36
*           E-mail: hm@igh-essen.com
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

//f��r Userspace Applikation RTW
#ifdef RTW_BUDDY
#include "buddy_main.h"
#endif

/*--external functions---------------------------------------------------------------------------*/

/*--external data--------------------------------------------------------------------------------*/

struct msr_dev *msr_dev_head = NULL;  

void *prtp,*krtp;

int (*newparamflag)(void*, void*, size_t) = NULL;
unsigned int msr_flag;


extern struct msr_char_buf *msr_kanal_puffer;

extern unsigned int msr_kanal_write_pointer; /* Schreibzeiger auf den Kanalringpuffer */

double msr_sample_freq = 1.0;

void msr_rtlib_cleanup(void)
{

    /* Eintr��ge in den Listen l��schen */
    msr_print_info("msr_control: cleanup parameters...");
    msr_clean_param_list();

    msr_print_info("msr_control: cleanup channels...");
    msr_clean_kanal_list();

//    msr_print_info("msr_control: cleanup errorlists...");
//    msr_clean_error_list();
    msr_print_info("msr_control: cleanup lists...");

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
*            newparamflag: Callback f��r Parmeter��nderung
*                          R��ckgabe: 0: erfolgreich -1: nicht erfolgreich 
*                          Parameter f��r Callback: void * : *rtp from above
*                          			void *    : Startadresse, die ge��ndert wurde
*                                                  size_t : L��nge in byte die ge��ndert wurden
*                          NULL: Funktion wird nicht aufgerufen						  
*               
*                          base_rate: Rate des Echtzeitprozesses in usec                
*                          base: Zeiger auf den Anfang des Kanalpuffers
*                          blocksize: L��nge einer Zeitscheibe (in byte)
*                          buflen: Anzahl Zeitscheiben total (bis zum wrap des Schreibzeigers)

* R��ckgabe: 0: wenn ok, sonst -1
*               
* Status: 
*
*******************************************************************************
*/


int msr_init(void *_rtp, int (*_newparamflag)(void*,void*,size_t),unsigned long _base_rate,void *_base,unsigned int _blocksize,unsigned int _buflen,unsigned int _flags){

    /* Save data for real time communications client */
    prtp = _rtp;
    newparamflag = _newparamflag;
    msr_flag = _flags;

    
    msr_sample_freq = 1.0e6/(double)_base_rate;

    msr_print_info("msr_modul: init of rt_lib...");



    msr_init_kanal_params(_base_rate,_base,_blocksize,_buflen);

    printf("msrio:init: baserate: %lu, blocksize: %u, buflen: %u\n",_base_rate,_blocksize,_buflen);  //BDG
    //Registrierung von wichtigen Kan��len und Parametern, die immer gebraucht werden
    /* als erste Kan��le immer die Zeit registrieren, wichtig f��r die Datenablage */

    /* Im der Matlab RTW-Variante mu�� daf��r gesorgt werden, da�� zumindest 
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
* R��ckgabe: 
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
* R��ckgabe: 
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

//hm, FIXME, the first time set_wfd ist called after opening the device is in msr_update
//           it should be called in msr_open because there already is data in the readbuffer
//if due to a model error msr_update does not get called the connected tag is not transmittet automatically


int msr_update(unsigned int wp){

    struct msr_dev *element = NULL;

    static int counter = 0;

    msr_kanal_write_pointer = wp;


    //im 20 Hz-(Echtzeit)Takt alle Listen durchlaufen und testen, ob Daten angefallen sind
    if(++counter >= (unsigned int)(msr_sample_freq/20)) { 
	counter = 0;
	update_messages_to_dev(); //Meldungen erzeugen
	FOR_THE_LIST(element,msr_dev_head) {
#ifdef RTW_BUDDY
	    if(msr_len_rb(element,2) > 0)
		set_wfd(element->client_wfd); 
#endif
	}

    }
    return 0; //FIXME, was wird zur��ckgegeben ?????
}

#endif















