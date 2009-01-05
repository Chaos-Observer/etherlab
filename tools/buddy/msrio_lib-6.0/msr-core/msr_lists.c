/**************************************************************************************************
*
*                          msr_lists.c
*
*           Listenverwaltung f�r das Echtzeitreglerkernelmodul.      
*           Hier werden die Kanal und Parameterlisten verwaltet.
*           
*           Autor: Wilhelm Hagemeister
*
*           (C) Copyright IgH 2001
*           Ingenieurgemeinschaft IgH
*           Heinz-B�cker Str. 34
*           D-45356 Essen
*           Tel.: +49 201/61 99 31
*           Fax.: +49 201/61 98 36
*           E-mail: hm@igh-essen.com
*
*
*
*
*
**************************************************************************************************/


/*--includes-------------------------------------------------------------------------------------*/

#include <msr_target.h> 

/* hier die Userbibliotheken */
//#include <linux/a.out.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>

#include <linux/errno.h>  /* error codes */
/* #include <math.h> */
#include <msr_utils.h>
#include <msr_mem.h>
#include <msr_lists.h>
#include <msr_charbuf.h>
#include <msr_reg.h>
#include <msr_messages.h>
#include <msr_interpreter.h>
#include <msr_taskname.h>
#include <msr_version.h>
#include <msr_main.h>


//f�r Userspace Applikation RTW
#ifdef RTW_BUDDY
#include "buddy_main.h"
#endif


/* F�llstand des Kanal-Ringpuffers */
#define LEV_RP ((k_buflen + msr_kanal_write_pointer - dev-> msr_kanal_read_pointer) % k_buflen)


/*--external functions---------------------------------------------------------------------------*/
extern volatile unsigned int msr_kanal_write_pointer;

extern int default_sampling_red;

extern struct new_utsname system_utsname;  //f�r Maschinenname

/*--external data--------------------------------------------------------------------------------*/
extern struct msr_kanal_list *msr_kanal_head; /* Kanalliste */
extern int k_buflen; /* in msr_reg.c */
extern struct msr_dev *msr_dev_head;  //Liste aller Verbindungen
extern struct msr_kanal_list *timechannel;

/*--public data----------------------------------------------------------------------------------*/





/*
***************************************************************************************************
*
* Function: msr_dev_printf(fmt,...)
*           Schreibt Informationen an alle Clients
*
* Parameter: siehe printf
*
* R�ckgabe: 
*
* Status: exp
*
***************************************************************************************************
*/

void msr_dev_printf(const char *format, ...) 
{
    struct msr_dev *dev_element = NULL;
    int len;
    va_list argptr;

    if(format != NULL) {
	va_start(argptr,format);
	{
	    FOR_THE_LIST(dev_element,msr_dev_head) {
		len = vsnprintf(msr_getb(dev_element->read_buffer),dev_element->read_buffer->bufsize, format, argptr);
		msr_incb(len,dev_element->read_buffer);
	    }

	}
	va_end(argptr);
    }

}


/*-----------------------------------------------------------------------------*/
void clean_send_channel_list(struct msr_send_ch_list **head)
{
    MSR_CLEAN_LIST(*head,msr_send_ch_list); 
}

/*-----------------------------------------------------------------------------*/
int add_channel_list(struct msr_send_ch_list **head,struct msr_kanal_list *akanal) 
{
   struct msr_send_ch_list *element = NULL, *prev = NULL;
   int i=0;
    /* letztes Element in der Liste suchen */
   FOR_THE_LIST(element,*head) {  /* *head */
       prev = element; 
       i++;
   }   
    element = (struct msr_send_ch_list *)getmem(sizeof(struct msr_send_ch_list));
    if (!element) return -ENOMEM;
    memset(element, 0, sizeof(struct msr_send_ch_list));                    
    element->kanal = akanal;
    element->next = NULL;  /* eigentlich nicht mehr n�tig, siehe memset */
    element->bs = 10;                                  /* Blocksize (soviele Werte werden in einen Frame zusammengefa�t */
    element->reduction = 10;                  /* Untersetzung des Kanals */
    element->codmode = MSR_CODEASCII;                                /* Sendemodus: ascii, base64 */
    element->cmode = 0;                               /* Compressionsmodus FIXME noch nicht implementiert 0: keine Kompression*/
//    element->counter = 0;
     /* place it in the list */
    if (prev) { 
	prev->next = element;  
    }
    else
    { 
	*head = element;    /* erstes Element */     /* *head = element */
	MSR_PRINT("msr_module: open: first element\n");

    }

    MSR_PRINT("msr_module: open: reg: kanal: %s nr: %i for sending...\n",element->kanal->p_bez,i);
    return 0;
} 

/*-----------------------------------------------------------------------------*/
struct msr_send_ch_list *add_or_get_channel_list2(struct msr_send_ch_list **head,struct msr_kanal_list *akanal) 
{
   struct msr_send_ch_list *element = NULL, *prev = NULL;

    /* letztes Element oder schon vorhandenens Element in der Liste suchen */
   FOR_THE_LIST(element,*head) {  /* *head */
       prev = element; 
       if (element->kanal == akanal) //kanal ist schon in der Liste
	   return element;
   }   
   /* ansonsten Liste erweitern */
    element = (struct msr_send_ch_list *)getmem(sizeof(struct msr_send_ch_list));
    if (!element) return NULL;
    memset(element, 0, sizeof(struct msr_send_ch_list));                    
    element->kanal = akanal;
    element->next = NULL;  /* eigentlich nicht mehr n�tig, siehe memset */
    element->bs = 10;                                  /* Blocksize (soviele Werte werden in einen Frame zusammengefa�t */
    element->reduction = 10;                  /* Untersetzung des Kanals */
    element->codmode = MSR_CODEASCII;                                /* Sendemodus: b:bin�r, x:hexadezimal, d:dezimal */
    element->cmode = 0;                               /* Compressionsmodus FIXME noch nicht implementiert 0: keine Kompression*/
    element->prec = 2;                       /* zwei Nachkommastellen */
    //    element->counter = 0;

     /* place it in the list */
    if (prev) { 
	prev->next = element;  
    }
    else
    { 
	*head = element;    /* erstes Element */     /* *head = element */
    }
    return element;
} 

/*----ein Element aus der Liste rausnehmen-------------------------------------------*/
void rm_channel_list_item(struct msr_send_ch_list **head,struct msr_kanal_list *akanal)
{
   struct msr_send_ch_list *element = NULL, *prev = NULL;

    /* vorhandenens Element in der Liste suchen */
   FOR_THE_LIST(element,*head) {  /* *head */
       if (element->kanal == akanal) {//kanal ist gefunden jetzt l�schen
	   if(prev) { //Zeiger des vorg�ngers richtig setzen
	       prev->next = element->next;
	   }
	   else { //es gibt keinen Vorg�nger also ist es das erste Element
	       if(element->next == NULL) //hat aber keinen Nachfolger, also ist die Liste dann leer !!
		   *head = NULL;
	       else
		   *head = element->next;
	   }
	   //jetzt l�schen
	   freemem(element);
	   element = NULL;
	   return;
       }
	   prev = element; 
   }

}


//Funktion ist nicht reentrant
//Testet die Messagekan�le und erzeugt Meldungen in alle angeschlossenen Devices

void update_messages_to_dev(void)
{
    static int rp = 0;
#define LEVRPUB ((k_buflen + msr_kanal_write_pointer - rp) % k_buflen)

    while(LEVRPUB > 0)
    {
	msr_write_messages_to_buffer(rp);
	rp+=default_sampling_red; 
	rp%=k_buflen;//MSR_KANAL_BUF_SIZE;
    }
#undef LEVRPUB
}

/*
***************************************************************************************************
*   character device functions
***************************************************************************************************
*/

/* dev msr kann zum Lesen und Schreiben von mehreren Prozessen gleichzeitig ge�ffnet werden 
   jeder "file" verwaltet seinen eigenen Lese- und Schreibpuffer */


/*
***************************************************************************************************
*
* Function: msr_dev_read_data
*           Diese Funktion geht alle Character Ringpuffer durch und sieht nach, ob von dort
*           Daten in den Leseringpuffer des dev geladen werden k�nnen
* Parameter: count gew�nschte Anzahl von bytes im read-Ringpuffer         
*            Die Funktion versucht immer ein bischen mehr in den Puffer zu laden.
*
* R�ckgabe: aktueller F�llstand des Ringpuffers
*
* Status: exp
*
***************************************************************************************************
*/

unsigned int msr_dev_read_data(struct msr_dev *dev, unsigned int count)
{
    

    /* ist vielleicht schon genug im read_puffer (weil noch nicht alles gelesen wurde)?? */
    unsigned int lev_read_puffer = msr_charbuf_lev(dev->rp_read_pointer,dev->read_buffer);
    if (lev_read_puffer > count) return lev_read_puffer;

    /* Wichtig: Parameter und Kanallisten werden direkt aus der Devicewritefunktion bearbeitet */

    /* jetzt testen wir mal den Datenstrom (die Kanaele...) */
    /* wollen wir �berhaupt senden auch �ber die readfunktion und nicht �ber ioctl */

    if(dev->reduction > 0) {
	while((msr_charbuf_lev(dev->rp_read_pointer,dev->read_buffer) < count) && /* noch nicht genug Daten im read_puffer */
	      (LEV_RP > 0)) /* aber im Kanalringpuffer ist was drin */
	{
	    if(dev->datamode == 0)  //alle Kan�le zusammen senden
		msr_write_kanaele_to_char_buffer(dev);
	    if(dev->datamode == 1) //individuelle Kan�le senden 
		msr_write_kanaele_to_char_buffer2(dev);

	    /* und Lesezeiger der Kanalliste erh�hen */
	    dev->msr_kanal_read_pointer+=default_sampling_red;    //FIXME von 1 auf default_sampling_red ge�ndert Hm 3.11.2004 ist das der Grund, warum die Laufzeit seit �nderung auf DMA-Kan�le von msrd angewachsen ist ????
	    if(default_sampling_red == k_buflen) printf("k_Wrap\n");
	    dev->msr_kanal_read_pointer%=k_buflen;
	    
	    /* Pegel des read_puffers bestimmen */
	    lev_read_puffer = msr_charbuf_lev(dev->rp_read_pointer,dev->read_buffer);

	    if (lev_read_puffer > count)
		return lev_read_puffer;
	}
    }
    return msr_charbuf_lev(dev->rp_read_pointer,dev->read_buffer);
}


/* Funktion berechnet die L�nge der Daten im lese-puffer eines Devices. Gleichzeitig werden eventuell schon vorhandene Daten
   in anderen Puffern Daten,User,... in den Lesepuffer �bertragen */
int msr_len_rb(struct msr_dev *dev,int count) 
{
    int ldev = msr_dev_read_data(dev,count);
    //ldev ist n�tig, da das MIN-Makro zweimal auswertet und daher nicht die Unterbrechnung durch Interrupts ber�cksichtigt HM 16.02.2004
    return (MIN(ldev,count));
}


/* A new Client connection has arrived. Setup internal structures for Client */
void *msr_open(int client_rfd, int client_wfd)
{

    char hostname[1024]; //lang genug ??

    struct msr_dev *dev = NULL, *prev = NULL, *dev_element = NULL;

    struct msr_kanal_list *element;

    MSR_PRINT("msr_module: (open) MAJOR: %i, MINOR: %i\n",MAJOR(inode->i_rdev),num); 

    dev = (struct msr_dev *)getmem(sizeof(struct msr_dev));
    if (!dev) return NULL;
    memset(dev,0,sizeof(struct msr_dev));

    /* Lese und Schreibpuffer einrichten */
    MSR_PRINT("msr_module: dev: Readbuffer...\n"); 
    dev->write_buffer = msr_create_charbuf(MSR_CHAR_BUF_SIZE);
    if(!dev->write_buffer){
	printk(KERN_WARNING "msr_modul: kein Speicher fuer Ringpuffer bei devopen.\n");
	freemem(dev);
	return NULL;
    }
    //Filepointer merken
    dev->filp = client_rfd; //FIXME ist das gut als Erkennung 

    MSR_PRINT("msr_module: dev: Writebuffer...\n"); 
    dev->read_buffer = msr_create_charbuf(MSR_CHAR_BUF_SIZE);
    if(!dev->read_buffer){
	printk(KERN_WARNING "msr_modul: kein Speicher fuer Ringpuffer bei devopen.\n");
	msr_free_charbuf(&dev->write_buffer);
	freemem(dev);
	return NULL;
    }


    MSR_PRINT("msr_module: dev: Pointer setzten...\n"); 

    /* jetzt den kanal_lese_zeiger auf den aktuellen kanal_write_pointer setzten */
    dev->msr_kanal_read_pointer = msr_kanal_write_pointer;

    dev->wp_read_pointer = 0;
    dev->rp_read_pointer = 0;
    dev->reduction = 0;         /* nicht senden bei �ffnen des Devices */
    dev->datamode = 0;          /* Standard�bertragung = alle Kan�le zusammen */
    dev->codmode=MSR_CODEASCII;                             
    dev->write_access = 0;      /* erstmal keinen Schreibzugriff auf Parameter */
    dev->isadmin = 0;           /* kein Administrator */
    dev->echo = 0;              /* kein Echo */
    dev->filter = 0;            /* kein Filter */
    dev->avr_filter_length = 1; /* +-1 Wert filtern, falls gleitender Mittelwert als Filter gew�hlt ist */
    dev->disconnectflag = 0;
    dev->cinbufsize = MSR_BLOCK_BUF_ELEMENTS*sizeof(double);
    dev->cinbuf = getmem(dev->cinbufsize);
    if(!dev->cinbuf) {
	printk(KERN_WARNING "msr_modul: kein Speicher fuer IN-Out-Buffer bei devopen.\n");
	msr_free_charbuf(&dev->write_buffer);
	msr_free_charbuf(&dev->read_buffer);
	freemem(dev);
	return NULL;
    }

    dev->client_rfd = client_rfd;
    dev->client_wfd = client_wfd;
    dev->next = NULL;  

    dev->ap_name = strdup("unknown");  //Name des verbundenen Programmes (dies mu� das Programm selber mitteilen)
    dev->hostname = strdup("unknown");
    dev->count_in = 0;                 //Anzahl bytes von Applikation zum Echtzeitprozess seit Verbindungsaufbau
    dev->count_out = 0;                //Anzahl bytes vom Echtzeitprozess zur Applikation
    do_gettimeofday(&dev->connection_time);          //Zeit, wann die Verbindung ge�ffnet wurde (UTC)


    /*und noch in die Liste aller devices eintragen (nur Userspace) */

    FOR_THE_LIST(dev_element,msr_dev_head) 
	prev = dev_element;  //ende suchen

     /* place it in the list */      
    if (prev)  
	prev->next = dev;  
    else
	msr_dev_head = dev;    /* erstes Element */

    /*jetzt noch den Kanal f�r die Zeit suchen (dieser mu� /Time/StructTimeval hei�en) */
    dev->timechannel = timechannel;            /* Schon zugewiesen ? */

    if(!dev->timechannel) {
	FOR_THE_LIST(element,msr_kanal_head) {
	    if ((element) && strcmp(element->p_bez,"/TimeStructTimeval") == 0)
		dev->timechannel = element;
	}
    }

    //wenn Struct Timeval nicht gefunden wurde nochmal nach Time suchen
    if(!dev->timechannel) {
      FOR_THE_LIST(element,msr_kanal_head) {
	if ((element) && strcmp(element->p_bez,"/Time") == 0)
	  dev->timechannel = element;
      }
    }

    MSR_PRINT("msr_module: (open) ok\n");

    gethostname (hostname, sizeof(hostname));

    msr_buf_printf(dev->read_buffer,"<connected name=\"%s\" host=\"%s\" version=\"%d\" features=\"%s\" recievebufsize=\"%d\"/>\n",
		   MSR_TASK_NAME,hostname,
		   MSR_VERSION,
		   MSR_FEATURES,   //steht auch in msr_version.h !!
		   dev->write_buffer->bufsize);

    return (void *)dev;          /* success */
}

/* Client closed connection. Cleanup client data structures */
int msr_close(void *p)
{
    struct msr_dev *dev = (struct msr_dev *)p;
    struct msr_dev *prev = NULL,*dev_element = NULL;

    dev->reduction = 0; /* erstmal die Daten�bertragung stoppen */

    /* speicherplatz wieder freigeben */
    clean_send_channel_list(&dev->send_ch_head);


    //aus der Liste der Devices entfernen
    FOR_THE_LIST(dev_element,msr_dev_head) {
	if(dev == dev_element) { //gefunden
	    if(prev) 
		prev->next = dev->next;
	    else //ist das erste Element
		msr_dev_head = dev->next;
	    break;
	}
	prev = dev_element;  
    }


    freemem(dev->ap_name);

    if((dev->cinbuf) != NULL) { 
	freemem(dev->cinbuf);
	dev->cinbuf = NULL;
    }

    msr_free_charbuf(&dev->read_buffer);
    msr_free_charbuf(&dev->write_buffer);


    freemem(dev);

    MSR_PRINT("msr_module: (close) ok\n");
    return 0;
}


/* Data has arrived from client. read() it and process. If data has to be
 * sent back to client, store it in an internal queue, and call set_wfd() to
 * indicate to Dispatcher that there is data for the client. When parameters
 * have changed, call newparamflag() to send new parameters to real time 
 * process */
void msr_read(int fd, void *p)
{
    struct msr_dev *dev = (struct msr_dev *)p;
    int count;

    count = read(dev->client_rfd,msr_getb(dev->write_buffer), 4096); //wir lesen immer eine Seite ?

    dev->count_in+=count;  //statistics

    /* Check for errors (rv < 0) or connection close (rv == 0).
     * Do not call msr_close() here -- this will be done by the
     * dispatcher */
    if (count <= 0) {
        msr_close(p);
        clr_fd(fd);
        close(fd);
	return;
    }
    else {  //Daten bekommen, Schreibzeiger weiterschieben
	msr_incb(count,dev->write_buffer);
	/* Hier werden jetzt MSR funktionen aufgerufen um die vom Client
	 * eingegangene Befehle zu bearbeiten. Wenn daten zum client zurueck
	 * geschickt werden muessen, set_wfd(fd) aurufen. Wenn parameter
	 * geaendert wurden, newparamflag() aufrufen. */
	while(msr_interpreter(dev)); /* solange bearbeiten, bis alle neuen empfangenen Kommandos interpretiert ist */

	/* Testen, ob Daten im Ausgangspuffer stehen ...*/
#ifdef RTW_BUDDY
	if(msr_len_rb(dev,2) > 0)
	    set_wfd(dev->client_wfd);  
#endif
    }

    /* Return the return value from read() system call */
    return;
}

/* Dispatcher indicated that data channel to client is ready for write(). 
 * When output buffer is empty, tell dispatcher by calling clr_wfd() */


void msr_write(int fd, void *p)
{
    struct msr_dev *dev = (struct msr_dev *)p;
    ssize_t count = 0;

    ssize_t max_len;

    /* testen, ob ein disconnect erfolgte */
    if(dev->disconnectflag)
    {
	printf("msrio:write:disconnect\n");
        clr_fd(fd);
        close(fd);
	return;
    }

    max_len = msr_len_rb(dev,1024); //soviel bei einem Lesebefehl maximal gelesen

    /* ist �berhaupt was zu lesen? Darf eigentlich nicht nicht sein....*/
    if(max_len == 0) 
    {      
	/* If output buffer is empty */
#ifdef RTW_BUDDY
	clr_wfd(dev->client_wfd);
#endif
	return;
    }
    
    if(dev->read_buffer->write_pointer > dev->rp_read_pointer) //der Schreibzeiger steht hinter dem Lesezeiger, dann von dort bis zum Schreibzeiger lesen
	count = write(dev->client_wfd,dev->read_buffer->buf + dev->rp_read_pointer,dev->read_buffer->write_pointer - dev->rp_read_pointer);
    else  // write_pointer < read_pointer dann vom lesezeiger bis zum Ende lesen. beim n�chsten Lesen ist dann der Lesezeiger wieder kleiner als der Schreibzeiger
	count = write(dev->client_wfd,dev->read_buffer->buf + dev->rp_read_pointer,dev->read_buffer->bufsize - dev->rp_read_pointer);

    //und den Lesezeiger nachf�hren
    dev->rp_read_pointer+=count;
    dev->rp_read_pointer%=dev->read_buffer->bufsize;

    dev->count_out+=count;  //statistics

    //nachsehen, ob noch Daten im Puffer sind
#ifdef RTW_BUDDY
    if(msr_len_rb(dev,1024) == 0)
	clr_wfd(dev->client_wfd);
#endif

    return;
}



















