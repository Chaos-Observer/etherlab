/**************************************************************************************************
*
*                          msr_lists.h
*
*           Listenverwaltung für das Echtzeitreglerkernelmodul.      
*           Hier werden die Kanal und Parameterlisten verwaltet.
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

#ifndef _MSR_LISTS_H_
#define _MSR_LISTS_H_

/*--includes-------------------------------------------------------------------------------------*/

#include "msr_target.h"

#include <unistd.h>


/*--defines--------------------------------------------------------------------------------------*/

#define MSR_CHAR_BUF_SIZE 1000000     /* Groesse des Char-Ringpuffers für die Kommunikation */
#define MSR_BLOCK_BUF_ELEMENTS 1024  /* dies ist die maximal mögliche Anzahl Werte pro Data-Tag pro Kanal */





/*--prototypes-----------------------------------------------------------------------------------*/

/* Struktur für die privaten Daten eines Devices ------------------------------------------------*/

/* eine Liste, die auf die zu senden Kanäle zeigt */
struct msr_send_ch_list
{
    struct msr_kanal_list *kanal;
    struct msr_send_ch_list *next;  
    /* die Kanäle können entweder alle in einem Vektor, mit einer Untersetzung gesendet werden, oder
       jeder mit unterschiedlicher Abtastrate, Kompression usw. Die nachfolgenden Werte sind für das individuelle sender der Kanäle */
    int bs;                                  /* Blocksize (soviele Werte werden in einen Frame zusammengefaßt */
    unsigned int reduction;                  /* Untersetzung des Kanals */
    char codmode;                            /* Sendemodus: ascii,base64  */
    int cmode;                               /* Compressionsmodus FIXME noch nicht implementiert */
    int prec;                                /* Genauigkeit in Nachkommastellen */
    int event;                              /* Übertragungsmodus 0:Standard, 1:Event (nur ein Änderung des Wertes) */
};

struct msr_dev {
    unsigned int msr_kanal_read_pointer;     /* Lesezeiger in der Kanalliste */
    unsigned int prev_msr_kanal_read_pointer;     /* Lesezeiger in der Kanalliste vom vorherigen Lesezugriff */
    struct msr_char_buf *write_buffer;
    unsigned int wp_read_pointer;            /* Lesezeiger des write_puffers :-) */
    struct msr_char_buf *read_buffer;
    unsigned int rp_read_pointer;            /* Lesezeiger des read_puffers */
    unsigned int reduction;                  /* Untersetzung für das Auslesen der Daten, 0 = nicht senden !*/
    unsigned int red_count;                  /* Zähler für Untersetzung */
    int datamode;                            /* Übertragungsmodus: 0: alle Kanäle zusammen 1:Kanäle mit unterschiedlicher Abtastrate */
    char codmode;                            /* Übertragungsmodus nur für <sad>  nichts formatiert ascii oder "base64" auf die rohwerte 
						bei xsad kann die Übertragung für die einzelnen Kanäle separat gesetzt werden */
    char triggereventchannels;               /* triggert die Übertragung von Eventkanälen an (auch, wenn sich nichts geändert hat) */
    struct msr_send_ch_list *send_ch_head;   /* Zeiger auf eine Liste der Kanäle die gesendet werden */
    int write_access;                        /* 1 wenn Schreibzugriff auf Parameter erlaubt, sonst 0 */
    int isadmin;                             /* 1 wenn sich ein Administrator angemeldet hat */
    unsigned int filp;                       /* Filp zur eindeutigen Kennung von Eingaben z.B. <wp....> */
    int echo;                                /* 1 wenn gesendete Befehle als echo zurückgeschickt werden sollen */
    int filter;                              // Filterfunktionen für die Daten siehe msr_reg.h
    int avr_filter_length;                   // Länge des gleitenden Mittelwertfilters zu jeder Seite
                                             // also insgesamt wird über avr_filter_lengt*2+1 gefiltert
    void *cinbuf;                            // Puffer für Kompression von Kanälen
    int cinbufsize;                          // und dessen Größe
    struct msr_kanal_list *timechannel;      // Zeitkanal 

    // Informationen über den angeschlossenen Client
    char *hostname;                          //Host, von dem die Verbindung gekommen ist (dies muß das Programm selber mitteilen)
    char *ap_name;                           //Name des verbundenen Programmes (dies muß das Programm selber mitteilen)
    unsigned long int count_in;              //Anzahl bytes von Applikation zum Echtzeitprozess seit Verbindungsaufbau
    unsigned long int count_out;             //Anzahl bytes vom Echtzeitprozess zur Applikation
    struct timeval connection_time;          //Zeit, wann die Verbindung geöffnet wurde (UTC)
    int client_rfd;                          // User-Variante
    int client_wfd;
    int disconnectflag;
    struct msr_dev *next;                    //für verkette Liste der devices = clients

};



/*
***************************************************************************************************
*
* Function: msr_dev_printf(fmt,...)
*           Schreibt Informationen an alle Clients
*
* Parameter: siehe printf
*
* Rückgabe: 
*
* Status: exp
*
***************************************************************************************************
*/

void msr_dev_printf(const char *format, ...); 
/*
 */ 

void clean_send_channel_list(struct msr_send_ch_list **head);
int add_channel_list(struct msr_send_ch_list **head,struct msr_kanal_list *akanal);
struct msr_send_ch_list *add_or_get_channel_list2(struct msr_send_ch_list **head,struct msr_kanal_list *akanal);
void rm_channel_list_item(struct msr_send_ch_list **head,struct msr_kanal_list *akanal);

int msr_len_rb(struct msr_dev *dev,int count);  //Hilfsfunktion 


/* Kommunikation wie Unix IO ----------------------------*/
void *msr_open(int rfd, int wfd);  //Rückgabe ist void * fuer private structuren
				   // rfd -> read file descriptor
				   // wfd -> write file descriptor
int msr_close(void *p);

/* Read ist von MSR aus gesehen. Daten gehen von Client an MSR.
 * Rueckgabe ist das resultat von der read() syscall */
void msr_read(int fd, void *p);

/* Write ist von MSR aus gesehen. Daten gehen von MSR an Client.
 * Rueckgabe ist das resultat von der write() syscall */
void msr_write(int fd, void *p);


void update_messages_to_dev(void);

#endif 	// _PARAMREG_H_













