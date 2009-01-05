/**************************************************************************************************
*
*                          msr_charbuf.c
*
*           Verwaltung von Character Ringpuffern
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



/*--includes-------------------------------------------------------------------------------------*/

#include <msr_target.h>
#include <msr_mem.h>

#ifdef __KERNEL__
/* hier die Kernelbiblotheken */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/vmalloc.h> 
#include <asm/segment.h>
#include <asm/uaccess.h>

#else
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#endif

#include <msr_utils.h>
#include <msr_charbuf.h>

#include <msr_rcsinfo.h>

RCS_ID("$Header: /home/hm/projekte/msr_messen_steuern_regeln/linux/2.6/kernel_modules/simulator/rt_lib/msr-core/RCS/msr_charbuf.c,v 1.5 2006/01/04 11:31:04 hm Exp $");

/*--defines--------------------------------------------------------------------------------------*/

/*--external functions---------------------------------------------------------------------------*/

/*--external data--------------------------------------------------------------------------------*/

/*--public data----------------------------------------------------------------------------------*/

/*--prototypes-----------------------------------------------------------------------------------*/

/*
***************************************************************************************************
*
* Function: msr_create_charbuf
*
* Beschreibung: Erzeugt einen neuen Ringpuffer
*
* Parameter: size: Größe des Puffers
*
* Rückgabe:  Zeiger auf den neuen Puffer 0:wenn nicht erfolgreich
*               
* Status: exp
*
***************************************************************************************************
*/

struct msr_char_buf *msr_create_charbuf(unsigned int size)
{
    struct msr_char_buf *abuffer;

    abuffer = (struct msr_char_buf *)getmem(sizeof(struct msr_char_buf));
    if (!abuffer) return NULL;                        /* hat wohl nicht funktioniert */
    memset(abuffer,0,sizeof(struct msr_char_buf));

    /* jetzt Speicher für den Ringpuffer */
    abuffer->buf = (char *)getmem(2*size);     /* 100% mehr anfordern für Überhang*/
    if(!abuffer->buf) {
	freemem(abuffer);
	return NULL;
    } 
    abuffer->bufsize = size;
    abuffer->write_pointer = 0;
    memset(abuffer->buf,0,size);
    printf("Charbuffer allocated: %i\n",abuffer->bufsize);
    return abuffer;
}

unsigned int msr_charbuf_realloc(struct msr_char_buf *abuffer,unsigned int nsize)
{

    //FIXME FIXME Funktion wird noch nicht genutzt (erst in V6.0.11) dann die nächste Zeile löschen
#if 0
    void *nb;
    unsigned int osize;

    if(abuffer == NULL)
	return 0;


    if (nsize < abuffer->bufsize)  //verkleinern geht nicht, fertig
	return abuffer->bufsize;

    printf("Growing Charbuffer, avail len: %i,req len: %i\n",abuffer->bufsize,nsize); 

    osize = abuffer->bufsize; //alte Größe merken

    nb = realloc(abuffer->buf,nsize*2); //100% mehr, siehe oben 
    if(nb) { //ok, allozierung war erfolgreich
	abuffer->buf = (char *)nb;
	abuffer->bufsize = nsize;


	//jetzt muss noch das Stück von 0 .. min(wp,nsize-osize) hinten angehängt werden
	if((nsize-osize) < abuffer->write_pointer) {
	    memcpy(abuffer->buf + osize,abuffer->buf,nsize-osize);
	    //und falls noch was übrig gebrieben ist bis wp, das nach vorne schieben
	    memmove(abuffer->buf,abuffer->buf+(nsize-osize),abuffer->write_pointer - (nsize-osize));
	}
	else {
	    memcpy(abuffer->buf + osize,abuffer->buf,abuffer->write_pointer);
	}

    }
#endif
    return abuffer->bufsize;
}

/*
***************************************************************************************************
*
* Function: msr_free_charbuf
*
* Beschreibung: gibt den Puffer frei
*
* Parameter: zeiger auf Puffer
*
* Rückgabe: 
*               
* Status: exp
*
***************************************************************************************************
*/

void msr_free_charbuf(struct msr_char_buf **abuffer)
{
    if ((*abuffer)->buf!=0) {
	freemem((*abuffer)->buf);
	(*abuffer)->buf = NULL;
    }
    freemem(*abuffer);
    *abuffer = NULL;
}

/*
***************************************************************************************************
*
* Function: msr_write_charbuf
*
* Beschreibung: Hängt den String in "instr" an den Puffer an,
*               Vorsicht: der String darf nicht länger sein als 20% der Pufferlänge
*
* Parameter: instr: Zeiger auf einen String
*            len: Länge des Strings = Anzahl bytes 
*            struct msr_char_buf: Zeiger auf einen Ringpuffer an den der String 
*                                 angehängt werden soll
*
* Rückgabe: 
*               
* Status: exp
*
***************************************************************************************************
*/
void msr_write_charbuf(char *instr,unsigned int len,struct msr_char_buf *abuffer)
{
    memcpy(abuffer->buf+abuffer->write_pointer,instr,len); 
    msr_incb(len,abuffer);
}


/*
***************************************************************************************************
*
* Function: msr_getb
*
* Beschreibung: Gibt einen Zeiger auf die aktuelle Position im Puffer zurück
*
* Parameter: instr: Zeiger auf einen String
*            struct msr_char_buf: Zeiger auf einen Ringpuffer an den der String 
*                                 angehängt werden soll
*
* Rückgabe: 
*               
* Status: exp
*
***************************************************************************************************
*/

char *msr_getb(struct msr_char_buf *abuffer)
{
    return abuffer->buf+abuffer->write_pointer;
}

/*
***************************************************************************************************
*
* Function: msr_incb()
*
* Beschreibung: Schiebt die Position des Schreibzeigers um pos weiter und
*               sorgt automatisch für den Überlauf
*
* Parameter: cnt: um wieviel der Schreibzeiger weitergeschoben werden soll
*
*
* Rückgabe: 
*               
* Status: exp
*
***************************************************************************************************
*/
int msr_incb(unsigned int cnt,struct msr_char_buf *abuffer)
{
    unsigned int nwp = cnt + abuffer->write_pointer;
/* jetzt überprüfen, ob der Schreibzeiger über bufsize herausgelaufen ist.
   Er befindet sich dann in dem Bereich:
   oberhalb von bufsize
   Dies führt zunächst nicht zu einem Speicherüberlauf, da ja etwas mehr Speicher belegt worden war,
   allerdings müssen die Bytes oberhalb von bufsize-1 bis nwp an den Anfang von
   buf kopiert werden und nwp muß an die richtige Stelle gesetzt werden.
   nwp zeigt übrigens immer das Element nach dem letzten gültigen im Puffer!! */

    if (nwp >= abuffer->bufsize)
    {
	memcpy(abuffer->buf,abuffer->buf + abuffer->bufsize,nwp - abuffer->bufsize); 
	/* das byte, wo nwp hinzeigt wird nicht mit kopiert !! */
        /* und den Schreibzeiger wieder richtig setzen */
	nwp%= abuffer->bufsize;
    }
    abuffer->write_pointer = nwp;
    return cnt;
}

/*
***************************************************************************************************
*
* Function: msr_charbuf_lev
*
* Beschreibung: Gibt den Abstand Schreib-Lesezeiger eines Buffers zurück
*
* Parameter: read_pointer: Lesezeiger
*            abuffer:  der Puffer
*
*
* Rückgabe: Abstand Schreib-Lesezeiger in bytes
*               
* Status: exp
*
***************************************************************************************************
*/

unsigned int msr_charbuf_lev(unsigned int read_pointer,struct msr_char_buf *abuffer)
{
return (abuffer->bufsize + abuffer->write_pointer - read_pointer) % abuffer->bufsize;
}
/*
***************************************************************************************************
*
* Function: msr_read_charbuf
*
* Beschreibung: liest aus inbuf entweder len, oder Abstand-Schreiblesezeiger bytes in outbuf
*                      
* Parameter:  abuffer: Quellbereich,
*             outbuf: Zielbereich
*             len: gewünschte Anzahl bytes
*             read_pointer: Aktuelle Position des Lesezeigers
*
*
* Rückgabe:   Anzahl wirklich geschriebener bytes
*               
* Status: exp
*
***************************************************************************************************
*/

int msr_read_charbuf(struct msr_char_buf *abuffer,char *outbuf,unsigned int len,unsigned int *read_pointer)
{
    /* Abstand Schreib-Lesezeiger */
    unsigned int abst_r_w =  msr_charbuf_lev(*read_pointer,abuffer);
 
    /* Berechnung der maximal zu lesenden Anzahl Bytes */
    unsigned int max_len = MIN(abst_r_w,len);

    /* ist überhaupt was zu lesen ?*/
    if(max_len == 0)
	return 0;

    if (max_len <= abuffer->bufsize - *read_pointer) 
    /* nur eine Kopie erforderlich */
    {
	memcpy(outbuf,abuffer->buf + *read_pointer,max_len);
    }
    else  /* Lesezeiger steht hinter Schreibzeiger und es muß sowohl vom hinteren Teil als auch des
	     vorderen Teils von inbuf gelesen werden*/
    {   
        /* Lesezeiger bis zum Schluss des Feldes */
	memcpy(outbuf,abuffer->buf+*read_pointer,abuffer->bufsize-*read_pointer);
        /* und den Rest von vorne kopieren */
	memcpy(&outbuf[abuffer->bufsize-*read_pointer],abuffer->buf,
	       max_len-(abuffer->bufsize-*read_pointer));
    }
    *read_pointer+= max_len;
    *read_pointer%= abuffer->bufsize;
/*    if(max_len > len)  DEBUGGING
      printk("msr_module: msr_read_charbuf: max_len > count %d %d\n",max_len,len); */
    return max_len;
}

/*
***************************************************************************************************
*
* Function: msr_charbuf_lin
*
* Beschreibung: Der Puffer ist doppelt so lang, wie die maximale Anzahl der zulässigen bytes.
*               wenn der Schreibzeiger größer als der Lesezeiger ist, heißt das, das das
*               zu lesende Segment aus zwei Bereichen besteht: Lesezeiger bis Ende und
*               Anfang bis Schreibzeiger. Für einige Stringuntersuchungen ist das
*               unkomfortabel. Diese Funktion hängt den Bereich: Anfang bis Schreibzeiger
*               hinten an.
*                      
* Parameter:  abuffer: Ringpuffer
*             read_pointer: Aktuelle Position des Lesezeigers
*
*
* Rückgabe:   
*               
* Status: exp
*
***************************************************************************************************
*/

void msr_charbuf_lin(struct msr_char_buf *abuffer,unsigned int read_pointer)
{
    /* hinter dem Lesezeiger eine 0 bitte */
    /* abuffer-buf+((abuffer->write_pointer+1) % abuffer->bufsize) = 0; */
    unsigned int lwp = 0;
    lwp = abuffer->write_pointer;
    abuffer->buf[abuffer->write_pointer] = 0; /* '\0'; */

    if(read_pointer < abuffer->write_pointer) 
	return; /* nichts zu tun .... */
    else {
        /* erstmal bis zum Schluß mit Nullen füllen */
	memset(abuffer->buf+abuffer->bufsize,0,abuffer->bufsize);
        /* jetzt das vordere Teil bis writepointer hinten anhängen */
	memcpy(abuffer->buf+abuffer->bufsize,abuffer->buf,abuffer->write_pointer);
    }

    if (lwp != abuffer->write_pointer)  //FIXME Test, ob es hier eine Racecondition gibt
	printk("msr_lib: msr_charbuf_lin interrupted! \n");
}










