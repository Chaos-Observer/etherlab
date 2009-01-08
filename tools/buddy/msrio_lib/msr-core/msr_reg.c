/**************************************************************************************************
*
*                          msr_reg.c
*
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
*           TODO: MDCT-Kompression, 
*
*           Floats werden mit %.16g formatiert, damit die Epochenzeit noch auf eine usec reinpasst!
*           Strings funktionieren wieder
*
*           2007.05.24: beim �berschreiten von Parameterlimits werden die Werte ignoriert
*           2008.12.16: Message funktionieren wieder, wie bei der msrlib Version 5.0.1 (f�r den Kernel)
*
*
*
*
**************************************************************************************************/


/*--includes-------------------------------------------------------------------------------------*/

#include <msr_target.h> 

/* hier die Userbibliotheken */
#include <linux/a.out.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#include <linux/errno.h>  /* error codes */

//#include <math.h> 
#include <msr_main.h>
#include <msr_lists.h>
#include <msr_utils.h>
#include <msr_charbuf.h>
#include <msr_reg.h>
#include <msr_messages.h>
#include <msr_utils.h>
#include <msr_base64.h>
#include <msr_hex_bin.h>
#include <msr_interpreter.h>
#include <msr_attributelist.h>
#include "etl_data_info.h"

#define DBG 0
/*--external functions---------------------------------------------------------------------------*/

/*--external data--------------------------------------------------------------------------------*/

/*--public data----------------------------------------------------------------------------------*/

/*---- Variablen f�r die Kanalablage ------------------------------------------------------------*/

int default_sampling_red = 1;    //Untersetzung 

unsigned int  k_base_rate = 1;         

void *k_base = NULL;                    //Zeiger 
int k_blocksize = 1;

int k_buflen = 10000;      //Gr��e des Ringpuffers 


/* Vorhandene Variablentypen als String */
char *enum_var_str[] = {ENUM_VAR_STR};

struct msr_param_list *msr_param_head = NULL; /* Parameterliste */
struct msr_kanal_list *msr_kanal_head = NULL; /* Kanalliste */
struct msr_mkanal_list *msr_mkanal_head = NULL;
struct msr_meta_list *msr_meta_head = NULL;
struct msr_kanal_list *timechannel = NULL;

volatile unsigned int msr_kanal_write_pointer = 0; /* Schreibzeiger auf den Kanalringpuffer */
volatile int msr_kanal_wrap = 0;                   /* wird = 1 gesetzt, wenn der Kanalringpuffer zum ersten Mal �berl�uft */


extern struct timeval msr_loading_time;  //in msr_proc.c

extern void *prtp;
extern int (*newparamflag)(void*, void*, size_t);  //Funktion, die aufgerufen werden mu�, wenn ein Parameter beschrieben wurde


#define MSR_CALC_ADR(_START,_DATASIZE,_ORIENTATION,_RNUM,_CNUM)   \
do {                                                              \
switch(_ORIENTATION) {                                            \
    case si_vector:                                               \
	p = _START + (r + c)*_DATASIZE;  		          \
        break;                                                    \
    case si_matrix_col_major:                                     \
	p = _START + (r + c * _RNUM)*_DATASIZE;	                  \
	break;                                                    \
    case si_matrix_row_major:                                     \
	p = _START + (_CNUM * r + c)*_DATASIZE;	                  \
	break;                                                    \
    default:                                                      \
        p = _START;                                               \
	break;                                                    \
}                                                                 \
} while (0)							  \


/*Hilfsfunktionen zur Formatierung von Parmetern */

#define FPARAM(VTYP,FSTR,SF)                                          \
do {                                                                  \
    unsigned int c, r;                                                \
    void *p;                                                          \
    unsigned int len = 0;                                             \
    for (r = 0; r <  self->rnum; r++) {                               \
	for (c = 0; c <  self->cnum; c++) {                           \
            MSR_CALC_ADR(self->p_adr,self->dataSize,self->orientation,self->rnum,self->cnum); \
            len+=sprintf(buf+len,FSTR,SF(*(VTYP *)(p)));              \
        if(c <  self->cnum-1)                                         \
            len+=sprintf(buf+len,",");                                \
	}                                                             \
	if(r <  self->rnum-1)                                         \
	    len+=sprintf(buf+len,",");                                \
    }                                                                 \
    return len;                                                       \
} while (0)


int r_p_int(struct msr_param_list *self,char *buf) {
    FPARAM(int,"%i",);
}

int r_p_uint(struct msr_param_list *self,char *buf) {
    FPARAM(unsigned int,"%u",);
}


int r_p_short(struct msr_param_list *self,char *buf) {
    FPARAM(short int,"%i",);
}

int r_p_ushort(struct msr_param_list *self,char *buf) {
    FPARAM(unsigned short int,"%u",);
}

int r_p_dbl(struct msr_param_list *self,char *buf) {
    FPARAM(double,"%.16g",);
}

int r_p_flt(struct msr_param_list *self,char *buf) {
    FPARAM(float,"%.16g",);
}


int r_p_char(struct msr_param_list *self,char *buf) {
    FPARAM(char,"%i",);
}

int r_p_uchar(struct msr_param_list *self,char *buf) {
    FPARAM(unsigned char,"%i",);
}

#undef FPARAM

int r_p_str(struct msr_param_list *self,char *buf) {
    return sprintf(buf,"%s",(char *)self->p_adr);
}


//Schreibfunktionen--------------------------------------------------------------------

//auch Komma separierte Listen


#define _simple_strtol(a,b) \
simple_strtol(a,b,10)

#define WPARAM(VTYP,VFUNKTION)                                                        \
do {                                                                                  \
    char *ugbuf;                                                                      \
    char *ogbuf;                                                                      \
    char *next = buf-1;                                                               \
    int result = 0;                                                                   \
    int dirty = 0;                                                                    \
    int index = si;                                                                   \
    VTYP tmp_data,ug=0,og=0;                                                          \
    int anz = self->cnum*self->rnum;                                                  \
										      \
  /*Test auf Limits	*/							      \
    ugbuf = msr_get_attrib(self->info,"ug");                                          \
    if(ugbuf == NULL)       /* Englische Variante probieren */                        \
       ugbuf = msr_get_attrib(self->info,"ll");                                       \
                                                                                      \
    ogbuf = msr_get_attrib(self->info,"og");                                          \
    if(ogbuf == NULL)      /* s.o... */                                               \
       ogbuf = msr_get_attrib(self->info,"ul");                                       \
                                                                                      \
    if(ugbuf)                                                                         \
	ug = (VTYP)VFUNKTION(ugbuf,NULL);	 				      \
    if(ogbuf)                                 					      \
	og = (VTYP)VFUNKTION(ogbuf,NULL);                                             \
                                                                                      \
    do {									      \
          dirty = 0;                                                                  \
  	  tmp_data = (VTYP)VFUNKTION(next+1,&next);				      \
  	  if(index>= anz) {							      \
  	      result|=4;							      \
              dirty = 1;                                                              \
  	      break;								      \
  	  }									      \
  	  /* Test auf Limits */							      \
  	  if(ugbuf && (tmp_data < ug)) {					      \
  	      tmp_data = ug;		               				      \
  	      result|=1;							      \
              dirty = 1;                                                              \
  	  }									      \
  	  if(ogbuf && (tmp_data > og)) {					      \
  	      tmp_data = og;							      \
  	      result|=2;							      \
              dirty = 1;                                                              \
  	  }									      \
          if (dirty == 0)                                                             \
  	    *(VTYP *)(self->p_adr+index*self->dataSize) = tmp_data; /* und zuweisen */\
  	  index++;								      \
    } while(*next);                                                                   \
                                                                                      \
    if(ugbuf)                                                                         \
	freemem(ugbuf);								      \
    if(ogbuf)                                                                         \
	freemem(ogbuf);								      \
    return -result;                                                                   \
} while (0)									      		   
										      
#define WHEXPARAM(VTYP,VFUNKTION)                                                     \
do {                                                                                  \
    char *ugbuf;                                                                      \
    char *ogbuf;                                                                      \
    int result = 0;                                                                   \
    int dirty = 0;                                                                    \
    int index = si;                                                                   \
    int i;                                                                            \
    VTYP tmp_data,ug=0,og=0;                                                          \
    int anz = self->cnum*self->rnum;                                                  \
										      \
  /*Test auf Limits	*/							      \
    ugbuf = msr_get_attrib(self->info,"ug");                                          \
    if(ugbuf == NULL)       /* Englische Variante probieren */                        \
       ugbuf = msr_get_attrib(self->info,"ll");                                       \
                                                                                      \
    ogbuf = msr_get_attrib(self->info,"og");                                          \
    if(ogbuf == NULL)      /* s.o... */                                               \
       ogbuf = msr_get_attrib(self->info,"ul");                                       \
                                                                                      \
    if(ugbuf)                                                                         \
	ug = (VTYP)VFUNKTION(ugbuf,NULL);					      \
    if(ogbuf)  									      \
	og = (VTYP)VFUNKTION(ogbuf,NULL);                                             \
                                                                                      \
    for(i=0;i<(strlen(buf)/2/self->dataSize);i++) {			              \
          dirty = 0;                                                                  \
  	  hex_to_bin(buf+(i*2*self->dataSize),(unsigned char*)&tmp_data,self->dataSize*2,sizeof(tmp_data));  \
  	  if(index>= anz) {							      \
  	      result|=4;							      \
              dirty = 1;                                                              \
  	      break;								      \
  	  }									      \
  	  /* Test auf Limits */							      \
  	  if(ugbuf && (tmp_data < ug)) {					      \
  	      tmp_data = ug;		               				      \
  	      result|=1;							      \
              dirty = 1;                                                              \
  	  }									      \
  	  if(ogbuf && (tmp_data > og)) {					      \
  	      tmp_data = og;							      \
  	      result|=2;							      \
              dirty = 1;                                                              \
  	  }									      \
          if (dirty == 0)                                                             \
    	    *(VTYP *)(self->p_adr+index*self->dataSize) = tmp_data; /* und zuweisen */\
  	  index++;								      \
    }                                                                                 \
                                                                                      \
    if(ugbuf)                                                                         \
	freemem(ugbuf);								      \
    if(ogbuf)                                                                         \
	freemem(ogbuf);								      \
    return -result;                                                                   \
} while (0)									      		   
				




						      
int w_p_int(struct msr_param_list *self,char *buf,unsigned int si,int mode) {	
    if(mode == MSR_CODEASCII)
	WPARAM(int,_simple_strtol);
    else
	if(mode == MSR_CODEHEX)
	    WHEXPARAM(int,_simple_strtol);
    return 0;
}										      
										      
int w_p_uint(struct msr_param_list *self,char *buf,unsigned int si,int mode) {				      
    if(mode == MSR_CODEASCII)
	WPARAM(unsigned int,_simple_strtol);						      
    else
	if(mode == MSR_CODEHEX)
	    WHEXPARAM(unsigned int,_simple_strtol);
    return 0;
}

int w_p_short(struct msr_param_list *self,char *buf,unsigned int si,int mode) {				      
    if(mode == MSR_CODEASCII)
	WPARAM(short int,_simple_strtol);							      
    else
	if(mode == MSR_CODEHEX)
	    WHEXPARAM(short int,_simple_strtol);
    return 0;
}										      

int w_p_ushort(struct msr_param_list *self,char *buf,unsigned int si,int mode) {				      
    if(mode == MSR_CODEASCII)
	WPARAM(unsigned short int,_simple_strtol);							      
    else
	if(mode == MSR_CODEHEX)
	    WHEXPARAM(unsigned short int,_simple_strtol);
    return 0;
}										      

int w_p_char(struct msr_param_list *self,char *buf,unsigned int si,int mode) {				      
    if(mode == MSR_CODEASCII)
	WPARAM(char,_simple_strtol);							      
    else
	if(mode == MSR_CODEHEX)
	    WHEXPARAM(char,_simple_strtol);
    return 0;
}										      

int w_p_uchar(struct msr_param_list *self,char *buf,unsigned int si,int mode) {				      
    if(mode == MSR_CODEASCII)
	WPARAM(unsigned char,_simple_strtol);							      
    else
	if(mode == MSR_CODEHEX)
	    WHEXPARAM(unsigned char,_simple_strtol);
    return 0;
}										      

int w_p_flt(struct msr_param_list *self,char *buf,unsigned int si,int mode) {
    if(mode == MSR_CODEASCII)
	WPARAM(float,strtod);
    else
	if(mode == MSR_CODEHEX)
	    WHEXPARAM(float,strtod);
    return 0;
}

int w_p_dbl(struct msr_param_list *self,char *buf,unsigned int si,int mode) {
    if(mode == MSR_CODEASCII)
	WPARAM(double,strtod);
    else
	if(mode == MSR_CODEHEX)
	    WHEXPARAM(double,strtod);
    return 0;
}



int w_p_str(struct msr_param_list *self,char *buf,unsigned int si,int mode) {

    int anz = self->cnum * self->rnum;

    memset(self->p_adr, 0, anz); //Nullen

    //und neu beschreiben
    snprintf((char*)self->p_adr,anz,"%s",buf); //hinten immer eine Null stehen lassen
    return 0;
}


#undef WPARAM

// usw.



//Vergleichsfunktion mit cbuf f�r numerische Werte
int p_num_chk(struct msr_param_list *self) {

    int i;
    int flag = 0;
    int companz;

    if(self->p_read)  /* erst die Aktualisierungsfunktion (lesend) aufrufen, falls die vorhanden ist,
					weil sonst eventuell nicht erkannt wird, dass sich Werte geaendert haben*/
	self->p_read(self);

    companz = self->dataSize * self->cnum * self->rnum; //soviel bytes m�ssen verglichen werden

    //jetzt den Inhalt von p_adr und cbuf vergleichen
    for(i=0;i<companz;i++) {
	if(((char *)self->cbuf)[i] != ((char *)self->p_adr)[i]) {
	    flag = 1; //gefunden
	}
	((char *)self->cbuf)[i] = ((char *)self->p_adr)[i]; //und kopie in cbuf
    }
    return flag;
}

//Freigabe methode f�r numerische Werte
void num_free(struct msr_param_list *self) {
    if(DBG > 0) printk("Free on %s\n",self->p_bez);
    if(self->cbuf)  {//Vergleichspuffer freigeben
	freemem(self->cbuf);
    }
}


int msr_cfi_reg_param2(char *bez,char *indexstr,char *einh,void *adr,int rnum, int cnum,int orientation,enum enum_var_typ typ,
		       char *info,
		       unsigned int flags,                                      //mit Flags 
		       void (*write)(struct msr_param_list *self),              //und Callbacks
		       void (*read)(struct msr_param_list *self)) {

    struct msr_param_list *element = NULL, *prev = NULL;                                                 
    char *initbuf;												   

    if(DBG > 0) printk("Initializing Listheader of Parameter %s,%s,%s\n",bez,einh,info);

    MSR_INIT_LIST_HEADER(msr_param_head,msr_param_list);
    element->p_flags = flags;                                                                    
    element->p_write = write;                                                                    
    element->p_read = read;                                                                      
    element->p_var_typ = typ;  
    element->cnum = cnum;
    element->rnum = rnum;
    element->orientation = orientation;
    element->free = num_free;
    element->p_chk = p_num_chk;

    do_gettimeofday(&element->mtime);

    if(DBG > 0) printk("done...\n");
    switch(typ) {
	case TINT:
	case TLINT:
	    element->dataSize = sizeof(int);
	    element->r_p = r_p_int;
	    element->w_p = w_p_int;
	    break;
	case TUINT:
	case TULINT:
	case TENUM:
	    element->dataSize = sizeof(int);
	    element->r_p = r_p_uint;
	    element->w_p = w_p_uint;
	    break;
	case TSHORT:
	    element->dataSize = sizeof(short int);
	    element->r_p = r_p_short;
	    element->w_p = w_p_short;
	    break;
	case TUSHORT:
	    element->dataSize = sizeof(short int);
	    element->r_p = r_p_ushort;
	    element->w_p = w_p_ushort;
	    break;
	case TCHAR:
	    element->dataSize = sizeof(char);
	    element->r_p = r_p_char;
	    element->w_p = w_p_char;
	    break;
	case TUCHAR:
	    element->dataSize = sizeof(char);
	    element->r_p = r_p_uchar;
	    element->w_p = w_p_uchar;
	    break;
	case TFLT:
	    element->dataSize = sizeof(float);
	    element->r_p = r_p_flt;
	    element->w_p = w_p_flt;

	    break;
	case TDBL:
	    element->dataSize = sizeof(double);
	    element->r_p = r_p_dbl;
	    element->w_p = w_p_dbl;
	    break;
	case TSTR:
	    element->dataSize = sizeof(char);
	    element->r_p = r_p_str;
	    element->w_p = w_p_str;
	    break;
	case TTIMEVAL:
	    printk("noch nicht implementiert.\n");
	    return 0;
	    break;
	case TFCALL:
	    printk("Funktioncalls k�nnen mit dieser Funktion nicht registriert werden.\n");
	    return 0;
	    break;

    }

    //FIXME usw.
    element->cbuf = (void *)getmem(element->dataSize*cnum*rnum); //Vergleichspuffer holen FIXME aufpassen bei Listen....

    //initialisieren, falls gew�nscht
    if(DBG > 0) printk("Init of Parameter %s\n",bez);


    initbuf = msr_get_attrib(element->info,"init");   
    if(initbuf && element->w_p) {
	element->w_p(element,initbuf,0,MSR_CODEASCII);
	freemem(initbuf);
    }    

    element->p_chk(element);  //die Checkfunktion einmal aufrufen, damit der Vergleichsbuffer mit den richtigen Daten gef�llt ist

    //FIXME, hier noch FLAGS �berschreiben, falls die in info auftauchen

    return (int)element;

}

/*
***************************************************************************************************
*
* Function: msr_unique_param_name
*
* Beschreibung: erzeugt einen eindeutigen Parameternamen
*                      
* Parameter: Namensvorschlag
*            
*
* R�ckgabe:  Zeiger auf neuen Namen (dieser mu� wieder freigegeben werden)
*               
* Status: exp
*
***************************************************************************************************
*/

char *msr_unique_param_name(char *bez){
    char *rbuf;
    MSR_UNIQUENAME(bez,rbuf,msr_param_head,msr_param_list); 
    return(rbuf);

}


/*
***************************************************************************************************
*
* Function: msr_clean_param_list
*
* Beschreibung: Gibt Speicherplatz wieder frei
*
* Parameter: 
*
* R�ckgabe:  
*               
* Status: exp
*
***************************************************************************************************
*/

void msr_clean_param_list(void)
{
    struct msr_param_list *element;
    FOR_THE_LIST(element,msr_param_head) {
	if (element->free)
	    element->free(element);                //die in den Parameter allozierten Speicher freigeben

	if(element->p_bez) 
	    freemem(element->p_bez);

	if(element->p_rbez) 
	    freemem(element->p_rbez);

	if(element->p_ibuf) 
	    freemem(element->p_ibuf);

	if(element->p_einh) 
	    freemem(element->p_einh);

	if(element->info) 
	    freemem(element->info);

    }
    MSR_CLEAN_LIST(msr_param_head,msr_param_list);  //die Liste selber freigeben
}


/*
***************************************************************************************************
*
* Function: msr_check_param_list
*
* Beschreibung: Testet, ob sich Parameter ge�ndert haben und macht Meldung an alle Clients 
*
 * Parameter: 
*
* R�ckgabe:  
*               
* Status: exp
*
***************************************************************************************************
*/

void msr_check_param_list(struct msr_param_list *p) 
{
    struct msr_param_list *element;

    if(p == NULL) {
        FOR_THE_LIST(element,msr_param_head) {
            if(element->p_chk) 
                if(element->p_chk(element) == 1) 
                    msr_dev_printf("<pu index=\"%i\"/>\n",element->index);
        }
    }
    else {
        element = p;
        if(element->p_chk) 
            if(element->p_chk(element) == 1) 
                msr_dev_printf("<pu index=\"%i\"/>\n",element->index);
    }
}


/*
***************************************************************************************************
*
* Function: msr_print_param_list
*
* Beschreibung:schreibt die Beschreibung der Parameter in buf
*                      
* Parameter: buf: array of char
*            aname: der Name eines Kanals der gedruckt werden soll
*                   wenn = NULL dann alle
*            id: mit id kann eine Parameternachricht eindeutig gekennzeichnet werden
*            shrt: kurzform
*            mode: 0:formatiert, 2:hex
*
* R�ckgabe:  L�nge des Puffers
*               
* Status: exp
*
***************************************************************************************************
*/

char *toxml(char *from)
{
    int len = (strlen(from)+1)*3/2;
    char *str = (char*) malloc(len);
    char *ptr = str;
    char *i, *s;

    if (!str)
        return NULL;

    for (i = from; *i; i++) {
        if (str + len < ptr + 7) {
            s = str;
            len *= 2;
            str = (char*) realloc (str, len);
            ptr = str + (ptr - s);
            if (!str)
                return NULL;
        }
        if (!strchr("<>\"'&", *i)) {
            *ptr++ = *i;
            continue;
        }
        switch (*i) {
            case '<':
                ptr = strcpy(ptr, "&lt;");
                ptr += 4;
                break;
            case '>':
                ptr = strcpy(ptr, "&gt;");
                ptr += 4;
                break;
            case '"':
                ptr = strcpy(ptr, "&quot;");
                ptr += 6;
                break;
            case '\'':
                ptr = strcpy(ptr, "&apos;");
                ptr += 6;
                break;
            case '&':
                ptr = strcpy(ptr, "&amp;");
                ptr += 5;
                break;
        }
    }
    *ptr = '\0';
    return str;
}

int msr_print_param_list(struct msr_char_buf *abuf,char *aname,char *id,int shrt,int mode)
{
    struct msr_param_list *element;
    unsigned int len = 0;
    char *or_str[] = {ENUM_OR_STR};
    char *xml_name = NULL;

    char *buf = msr_getb(abuf);

    /* Element in der Liste suchen */
    FOR_THE_LIST(element,msr_param_head) {
	if (element &&  ((aname == NULL) || strcmp(aname,element->p_bez) == 0)){

	                                    //(strstr(element->p_bez,aname) == element->p_bez))){  //suche alle Parameter, die mit aname anfangen 

	    if(element->p_read !=NULL) /* erst die Aktualisierungsfunktion aufrufen */
		element->p_read(element);

            xml_name = toxml(element->p_bez);
	    if(shrt) {
		len+=sprintf(buf+len,"<parameter name=\"%s\" index=\"%i\"",
			     xml_name,element->index);
	    }
	    else {

		len+=sprintf(buf+len,"<parameter name=\"%s\" index=\"%i\" flags=\"%u\" mtime=\"%u.%.6u\" datasize=\"%i\"",
			     xml_name,element->index,element->p_flags,(unsigned int)element->mtime.tv_sec,(unsigned int)element->mtime.tv_usec,element->dataSize);


		if(strlen(element->p_einh) > 0)
		    len+=sprintf(buf+len," unit=\"%s\"",element->p_einh);


		//Infostring
		if(element->info) //info ist schon im Format name="value"
		    len+=sprintf(buf+len," %s",element->info);


		//Kompatibilit�t zu alten Version in der Behandlung von Listen und Matrizen
		len+=sprintf(buf+len," typ=\"%s",enum_var_str[element->p_var_typ]); 

		if(element->cnum + element->rnum > 2 && element->p_var_typ != TSTR) {  //Vektor oder Matrize aber kein String
		    if(element->cnum == 1 || element->rnum == 1) {
			len+=sprintf(buf+len,"_LIST\"");
		    }
		    else
			len+=sprintf(buf+len,"_MATRIX\"");

		    //wenn Matrize oder Vektor, dann alles
		    len+=sprintf(buf+len," anz=\"%i\" cnum=\"%i\" rnum=\"%i\" orientation=\"%s\"",
				 element->cnum*element->rnum,element->cnum,element->rnum,or_str[element->orientation]);
		}
		else
		    len+=sprintf(buf+len,"\"");
	    }
            free(xml_name);

	    //Id
	    if(id)
		len+=sprintf(buf+len," id=\"%s\"",id);   //FIXME, noch notwendig; ja f�r R�ckw�rtskompatibilit�t

	    //FIXME, was noch fehlt ist num= bei Listen FIXME

	    //jetzt den Rest
	    if(element->r_p) {
		if(mode == MSR_CODEHEX && element->dataSize > 0) {
		    len+=sprintf(buf+len," hexvalue=\"");
		    len+=bin_to_hex(element->p_adr,buf+len,element->cnum*element->rnum*element->dataSize,-1);
		}
		else {
		    len+=sprintf(buf+len," value=\"");
		    len+=element->r_p(element,buf+len);
		}
		len+=sprintf(buf+len,"\"");
	    }
	    len+=sprintf(buf+len,"/>\n");
	    if(DBG > 0) printk("%s\n",buf);
	}
    }
    
    msr_incb(len,abuf);  //schreibzeiger weiterschieben

    return len;
}


/*
***************************************************************************************************
*
* Function: msr_print_param_valuelist
*
* Beschreibung:schreibt die Beschreibung der Parameter (nur die Values) in buf
*                      
* Parameter: buf: array of char
*            mode: 0:formatiert, 2:hex
*
*
* R�ckgabe:  L�nge des Puffers
*               
* Status: exp
*
***************************************************************************************************
*/

int msr_print_param_valuelist(struct msr_char_buf *abuf,int mode)
{
    struct msr_param_list *element;
    unsigned int len = 0;

    char *buf = msr_getb(abuf);

    if(mode == MSR_CODEHEX)
	len+=sprintf(buf+len,"<param_values hexvalue=\"");
    else
	len+=sprintf(buf+len,"<param_values value=\"");

    FOR_THE_LIST(element,msr_param_head) {
	if (element) 
	{
	    if(element->p_read !=NULL) /* erst die Aktualisierungsfunktion aufrufen */
		element->p_read(element);

	    if(element->r_p) {
		if(mode == MSR_CODEHEX && element->dataSize > 0) {
		    len+=bin_to_hex(element->p_adr,buf+len,element->cnum*element->rnum*element->dataSize,-1);
		}
		else
		    len+=element->r_p(element,buf+len);	    
	    }
	    if(element->next) 
		len+=sprintf(buf+len,"|");
	    else 
		len+=sprintf(buf+len,"\"/>\n");  /* war wohl das Ende der Parameter */

	}
    }
    msr_incb(len,abuf);  //schreibzeiger weiterschieben

    return len;
}


/*
***************************************************************************************************
*
* Function: msr_write_param
*
* Beschreibung: beschreibt einen Wert
*                      
* Parameter: dev: devicezeiger
*            aname:  der Name eines Kanals der beschrieben  werden soll
*            avalue: der Wert als String        
*            si: Startindex
*            mode: Schreibmodus CODEASCII, oder CODEHEX
*            aic: 1 = asynchroner-input-Kanal 
*
* R�ckgabe: 
*               
* Status: exp
*
***************************************************************************************************
*/

void msr_write_param(struct msr_dev *dev/*struct msr_char_buf *buf*/,char *aname,char* avalue,unsigned int si,int mode,int aic)
{
    struct msr_param_list *element;
    struct timeval now;
    int result;

    struct msr_char_buf *buf = NULL;

    if(dev) 
	buf = dev->read_buffer;  //sonst NULL, dann gibts keine R�ckmeldungen, aber die Variablen werden doch geschrieben

    /* Element in der Liste suchen welches gemeint ist. */
    FOR_THE_LIST(element,msr_param_head) {
	if (element && (strcmp(aname,element->p_bez) == 0)) {  /* gefunden */

	    if((element->p_flags & MSR_W) == MSR_W || 
	       ((element->p_flags & MSR_P) == MSR_P) ||
	       (((element->p_flags & MSR_AIC) == MSR_AIC) && (aic == 1))) {

		if(element->p_adr && element->w_p) {

		    result = element->w_p(element,avalue,si,mode);

		    if (result < 0 && dev)
			msr_buf_printf(buf,"<%s text=\"%s: Fehler beim Beschreiben eines Parameters. (%d)\"/>\n",
						    MSR_WARN,aname,result);

		    if(element->p_write !=NULL) /* noch die Aktualisierungsfunktion aufrufen */
			element->p_write(element);


		    if(newparamflag)  //FIXME Teile eines Arrays beschreiben mit si > 0 mu� noch ber�cksichtigt werden
			newparamflag(prtp,element->p_adr,element->dataSize*element->rnum*element->cnum); 


		    do_gettimeofday(&now); 

		    //die Inputchannels in der Updaterate auf max alle 2 Sec begrenzen
		    if((element->p_flags & MSR_AIC) == MSR_AIC && now.tv_sec < 2 + element->mtime.tv_sec) {
			//nix tun
		    }
		    else {
			//im Userspace auch die Checkfunktion f�r dieses Element aufrufen
			if((element->p_flags & MSR_DEP) == MSR_DEP) //alle Parameter �berpr�fen
			    msr_check_param_list(NULL);
			else
			    msr_check_param_list(element);
			element->mtime = now; //und die Modifikationszeit/bei aic = Meldezeit merken
		    }
			


		}

		return;
	    }
	    else {
		if (dev) msr_buf_printf(buf,"<%s text=\"Schreiben auf Parameter %s nicht zul�ssig.\"/>\n",MSR_WARN,aname);
		return;
	    }
	}
    }
    /* wenn bis hierher gekommen, dann ist der Name nicht vorhanden */
    if (dev) msr_buf_printf(buf,"<%s text=\"Parametername nicht vorhanden.(%s)\"\n>",MSR_WARN,aname);
}

/* ab hier Kanaldefinitionen */


/*
***************************************************************************************************
*
* Function: msr_init_kanal_params
*
* Beschreibung: Stellt voreingestellt Werte f�r die Ablage der Daten in Kan�le ein
*
* Parameter: base_rate: Rate des Echtzeitprozesses in usec
*            base: Zeiger auf den Anfang des Kanalpuffers
*            blocksize: L�nge einer Zeitscheibe (in byte)
*            buflen: Anzahl Zeitscheiben total (nach buflen wrappt der Schreibzeiger
*            
*            
*
* R�ckgabe:  Adresse der Struktur wenn alles ok, sonst < 0
*               
* Status: exp
*
***************************************************************************************************
*/

void msr_init_kanal_params(unsigned int _base_rate,void *_base,unsigned int _blocksize,unsigned int _buflen){

    k_base_rate = _base_rate;
    k_base = _base;
    k_blocksize = _blocksize;
    k_buflen = _buflen;

}



/*
***************************************************************************************************
*
* Function: msr_reg_kanal(2)
*
* Beschreibung: Registriert einen Kanal in der Kanalliste 
*
* Parameter: siehe Kanalstrukt 
*
* R�ckgabe:  0 wenn alles ok, sonst < 0
*               
* Status: exp
*
***************************************************************************************************
*/
int msr_reg_kanal(char *bez,char *einh,void *adr,enum enum_var_typ typ)

{
    return msr_reg_kanal2(bez,"",einh,adr,typ,default_sampling_red);
}




int msr_reg_kanal2(char *bez,void *alias,char *einh,void *adr,enum enum_var_typ typ,int red) {
    return msr_reg_kanal3(bez,alias,einh,adr,typ,"",red);
}

//check for uniqueness
int msr_reg_kanal3(char *bez,void *alias,char *einh,void *adr,enum enum_var_typ typ,char *info,int red) {
    char *upath;
    upath = msr_unique_channel_name(bez);
    return msr_reg_kanal4(upath,"",alias,einh,adr,typ,info,red);
    freemem(upath);
}

//leave the check vor uniqueness to the user ...
int msr_reg_kanal4(char *bez,char *indexstr,void *alias,char *einh,void *adr,enum enum_var_typ typ,char *info,int red)
{
    struct msr_kanal_list *element = NULL, *prev = NULL;                                                 

#if 0
    printf("Reg-Kanal: %s|%s\n",bez,indexstr);
#endif
    MSR_INIT_LIST_HEADER(msr_kanal_head,msr_kanal_list); //psize wird sp�ter zugewiesen
 
    element->p_var_typ = typ;                                           

    if(red >=0)
	element->sampling_red = red;
    else
	element->sampling_red = default_sampling_red;
    //Speicher reservieren f�r die Daten 
    element->bufsize = (k_buflen)/element->sampling_red; 
    //Gr��e berechnen
    switch(typ){
	case TCHAR:
	case TUCHAR:
	    element->dataSize = sizeof(char);
	    break;
	case TSHORT:
	case TUSHORT:
	    element->dataSize = sizeof(short int);
	    break;
	case TINT:
	case TUINT:
	case TLINT:
	case TULINT:
	    element->dataSize = sizeof(int);
	    break;
	case TTIMEVAL:
	    element->dataSize = sizeof(struct timeval);
	    break;
	case TFLT: 
	    element->dataSize = sizeof(float);
	    break;
	case TDBL:
	    element->dataSize = sizeof(double);
	    break;
	default:
	    element->dataSize = sizeof(int);
	    element->p_var_typ = TINT;
	    break;
    }



    //und noch den alias
    element->alias = (char *)getmem(strlen(alias)+1);
    if(!element->alias) {
	printk("Out of Memory for Channel allokation: %s\n",bez);
    }
    else
	strcpy(element->alias,alias);

    return (int)element;
    //FIXME wer sagt eigentlich, das der Zeiger (element) nicht negativ wird? negative Zahlen sind Fehlermeldungen (sieh z.B. -ENOMEM) ????


}



/*
***************************************************************************************************
*
* Function: msr_unique_channel_name
*
* Beschreibung: erzeugt einen eindeutigen Kanalnamen
*                      
* Parameter: Namensvorschlag
*            
*
* R�ckgabe:  Zeiger auf neuen Namen (dieser mu� wieder freigegeben werden)
*               
* Status: exp
*
***************************************************************************************************
*/

char *msr_unique_channel_name(char *bez)
{
    char *rbuf;
    MSR_UNIQUENAME(bez,rbuf,msr_kanal_head,msr_kanal_list); 
    return(rbuf);
}


int msr_reg_mkanal(struct msr_kanal_list *channel,char *messagetyp,char *messagetext)
{
    struct msr_mkanal_list *element = NULL, *prev = NULL;                                                 
    MSR_ADD_LIST(msr_mkanal_head,msr_mkanal_list);
    element->kelement = channel;
    element->mtyp = strdup(messagetyp);
    element->mtext = strdup(messagetext);
    element->prevvalue = NULL;
    return (int)element;
}

void msr_clean_kanal_list(void)
{
    struct msr_kanal_list *element;
    struct msr_mkanal_list *melement;
    //den Speicherplatz der Kanalbuffer wieder frei geben
    FOR_THE_LIST(element,msr_kanal_head) {
	if(element) {
	    if(element->bufsize > 0) {
		element->bufsize = 0;
		//	kfree(element->kbuf);
//		freemem(element->kbuf);  //siehe oben
//		free_pages((unsigned int)element->kbuf,element->order);
	    }
	    if(element->p_bez) 
		freemem(element->p_bez);

	    if(element->p_einh) 
		freemem(element->p_einh);

	    if(element->p_rbez) 
		freemem(element->p_rbez);

	    if(element->p_ibuf) 
		freemem(element->p_ibuf);

	    if(element->info) 
		freemem(element->info);

	    if(element->alias) 
		freemem(element->alias);
	}
    }
    MSR_CLEAN_LIST(msr_kanal_head,msr_kanal_list);

    //Messagelist
    FOR_THE_LIST(melement,msr_mkanal_head) {
	if(melement) {
	    if(melement->mtyp)
		freemem(melement->mtyp);
	    if(melement->mtext)
		freemem(melement->mtext);
	}
    }

    MSR_CLEAN_LIST(msr_mkanal_head,msr_mkanal_list); //Messages

}


/*
***************************************************************************************************
*
* Function: msr_print_kanal_list
*
* Beschreibung:schreibt die Beschreibung der Kan�le in buf (f�r Procfile und Befehl read_kanaele)
*                      
* Parameter: buf: array of char, 
*            mode: 0: alle Informationen rausschreiben, 1: nur index und Name 
*
* R�ckgabe:  L�nge des Puffers
*               
* Status: exp
*
***************************************************************************************************
*/


int msr_print_kanal_list(struct msr_char_buf *abuf,char *aname,int mode)
{
    struct msr_kanal_list *element;
    unsigned int len = 0;
    int index = 0;
    int wp = msr_kanal_write_pointer;  //aktuellen Schreibzeiger merken 
    char *xml_name = NULL;

    char *buf = msr_getb(abuf);

    //und den Vorg�nger bestimmen

    wp = (wp+k_buflen-default_sampling_red)%k_buflen;


    FOR_THE_LIST(element,msr_kanal_head) {
	if (element && ((aname == NULL) || (strcmp(aname,element->p_bez) == 0))) {
            xml_name = toxml(element->p_bez);
	    if(mode == 1) {
		len+=sprintf(buf+len,"<channel index=\"%.3i\" name=\"%s\"",
			     index,
			     xml_name);
	    } 
	    else {
		len+=sprintf(buf+len,"<channel name=\"%s\" alias=\"%s\" index=\"%i\" typ=\"%s\" datasize=\"%i\" bufsize=\"%i\" HZ=\"%.16g\"",
			     xml_name,
			     element->alias,
			     index,
			     enum_var_str[element->p_var_typ],
			     element->dataSize,
			     element->bufsize,
			     1.0e6/(double)k_base_rate);

		//	if(strlen(element->p_einh) > 0)                            //Einheit nur, wenn auch da, eventuell steht sie auch in info
		//FIXME tuts noch nicht mit dlsd hier darf die unit nicht fehlen

		// 2007-08-13, fp: unit-Attribut darf nicht doppelt vorkommen.
		// FIXME: Dieser Ansatz ist nicht ganz sauber, da "unit=" auch als
		// Attributwert vorkommen koennte...
		if (!element->info || !strstr(element->info, "unit="))
			len+=sprintf(buf+len," unit=\"%s\"",element->p_einh);

		//Infostring
		if(element->info && strlen(element->info) > 0) //info ist schon im Format name="value"
		    len+=sprintf(buf+len," %s",element->info);
	    }

	    len+=sprintf(buf+len," value=\"");

	    switch(element->p_var_typ)
	    {
		case TINT:
		    len+=sprintf(buf+len,"%i\"/>\n",(*(int*)(k_base + wp * k_blocksize + (int)element->p_adr)));
		    break;
		case TLINT:
		    len+=sprintf(buf+len,"%li\"/>\n",(*(long int*)(k_base + wp * k_blocksize + (int)element->p_adr)));
		    break;
		case TCHAR:
		    len+=sprintf(buf+len,"%i\"/>\n",(*(char*)(k_base + wp * k_blocksize + (int)element->p_adr)));
		    break;
		case TUCHAR:
		    len+=sprintf(buf+len,"%u\"/>\n",(*(unsigned char*)(k_base + wp * k_blocksize + (int)element->p_adr)));
		    break;
		case TSHORT:
		    len+=sprintf(buf+len,"%i\"/>\n",(*(short int*)(k_base + wp * k_blocksize + (int)element->p_adr)));
		    break;
		case TUSHORT:
		    len+=sprintf(buf+len,"%u\"/>\n",(*(unsigned short int*)(k_base + wp * k_blocksize + (int)element->p_adr)));
		    break;
		case TUINT:
		    len+=sprintf(buf+len,"%u\"/>\n",(*(unsigned int*)(k_base + wp * k_blocksize + (int)element->p_adr)));
		    break;
		case TULINT:
		    len+=sprintf(buf+len,"%lu\"/>\n",(*(unsigned long int*)(k_base + wp * k_blocksize + (int)element->p_adr)));
		    break;
		case TFLT:
		    len+=sprintf(buf+len,"%.16g\"/>\n",(*(float*)(k_base + wp * k_blocksize + (int)element->p_adr)));  
		    break;
		case TDBL:
		    len+=sprintf(buf+len,"%.16g\"/>\n",(*(double*)(k_base + wp * k_blocksize + (int)element->p_adr))); 
		    break;
		case TTIMEVAL:
		    len+=sprintf(buf+len,"%u.%.6u\"/>\n",(unsigned int)(*(struct timeval*)(k_base + wp * k_blocksize + (int)element->p_adr)).tv_sec,
				 (unsigned int)(*(struct timeval*)(k_base + wp * k_blocksize + (int)element->p_adr)).tv_usec);
		    break;
		default: break;
	    }
	    free(xml_name);
	}
	index++;
    }

    msr_incb(len,abuf);
    
    return len;
}

/*
***************************************************************************************************
*
* Function: printChVal(char *buf,struct msr_kanal_list *kanal,int index)
*
* Beschreibung: Druckt einen Wert eines Kanals in einen Puffer

*                      
* Parameter: 
*            
*
* R�ckgabe: string
*               
* Status: exp
*
***************************************************************************************************
*/ //FIXME diese Funktion mu� mal raus.....

int printChVal(struct msr_char_buf *buf,struct msr_kanal_list *kanal,int index)
{
#define DFFP(VTYP,FI)  msr_buf_printf(buf,FI,*(VTYP *)(k_base + index * k_blocksize + (int)kanal->p_adr))

    int cnt = 0;
    switch(kanal->p_var_typ)
    {
	case TUCHAR:
	    cnt= DFFP(unsigned char,"%u");
	    break;
	case TCHAR:
	    cnt= DFFP(char,"%i");
	    break;

	case TUSHORT: 
	    cnt= DFFP(unsigned short int,"%u");
	    break;

	case TSHORT: 
	    cnt= DFFP(short int,"%i");
	    break;
	case TINT:
	    cnt= DFFP(int,"%i");
	    break;
	case TLINT:
	    cnt= DFFP(long int,"%li");
	    break;
        case TUINT:
	    cnt= DFFP(unsigned int,"%u");
	    break;
	case TULINT:
	    cnt= DFFP(unsigned long int,"%lu");
	    break;


        case TTIMEVAL: {
	  struct timeval tmp_value;
	  tmp_value = (*(struct timeval *)(k_base + index * k_blocksize + (int)kanal->p_adr));
	  cnt=msr_buf_printf(buf,"%u.%.6u",(unsigned int)tmp_value.tv_sec,(unsigned int)tmp_value.tv_usec);
	  break;
       }
	case TFLT: {
	    float tmp_value;
	    tmp_value = (*(float *)(k_base + index * k_blocksize + (int)kanal->p_adr));
	    if((tmp_value == 0.0))
		cnt=msr_buf_printf(buf,"0");
	    else
		cnt=msr_buf_printf(buf,"%.16g",tmp_value);
	    break;
	}
	case TDBL:{
	    double tmp_value;
	    tmp_value = (*(double *)(k_base + index * k_blocksize + (int)kanal->p_adr));
	    if((tmp_value == 0.0))
		cnt=msr_buf_printf(buf,"0");
	    else
		cnt=msr_buf_printf(buf,"%.16g",tmp_value); 

	    break;
	}
	default: break;
    }
    return cnt;
#undef DFFP
}

/*
***************************************************************************************************
*
* Function: msr_write_kaenale_to_char_buffer (F�r start_data....)
*
* Beschreibung: Schreibt die Kanalwerte in einen Zeichenringpuffer (f�r die Daten�bertragung zum Client)
*                      
* Parameter: struct msr_dev *dev : Device
*            
*
* R�ckgabe:  
*               
* Status: exp
*
***************************************************************************************************
*/

void msr_write_kanaele_to_char_buffer(struct msr_dev *dev)
{
    struct msr_char_buf *kp = dev->read_buffer;        /* Characterringpuffer, in den geschrieben werden soll */

    struct msr_send_ch_list *element = NULL;

    int cnt = 0;
    int len = 0;
    /* pr�fen ob schon gesendet werden soll */
    if(dev->msr_kanal_read_pointer % dev->reduction == 0) {
	
	/* Datenfeld beginnen */
	msr_buf_printf(kp,"<D c=\"");

	switch(dev->codmode) {
	    case MSR_CODEASCII:
		FOR_THE_LIST(element,dev->send_ch_head) {
		    if ((element) && (element->kanal)){
			len+=printChVal(kp,element->kanal,dev->msr_kanal_read_pointer/element->kanal->sampling_red); 
			if(element->next) 
			    msr_buf_printf(kp,",");
			else 
			    msr_buf_printf(kp,"\"/>\n");  /* war wohl das Ende der Kan�le */
			//Test, ob noch genug Platz im Puffer ist
			if(len > kp->bufsize) 
			    msr_charbuf_realloc(kp,(kp->bufsize*3)/2); //um 50% gr��er 
			
		    }
		}
		break;
	    case MSR_CODEHEX:
	    case MSR_CODEBASE64:
		cnt = 0;
		FOR_THE_LIST(element,dev->send_ch_head) {
		    if ((element) && (element->kanal)){
			//die Werte bin�r in cinbuf kopieren
			    memcpy(((void*)dev->cinbuf)+cnt,
				   ((void*)(k_base + dev->msr_kanal_read_pointer * k_blocksize) + (int)element->kanal->p_adr),
				   element->kanal->dataSize);
			    cnt+=element->kanal->dataSize;
			    if(cnt>dev->cinbufsize) {
				msr_print_error("CINbuf zu klein");
				dev->reduction = 0; //stopt die Daten�bertragung
			    }
		    }
		}
		if(cnt > 0) {
		    //direkt in den Characterpuffer drucken
		    if (dev->codmode == MSR_CODEBASE64)
			cnt = gsasl_base64_encode ((char const *)dev->cinbuf,cnt, msr_getb(kp), MSR_CHAR_BUF_SIZE);
		    else
			cnt = bin_to_hex((unsigned char const *)dev->cinbuf,msr_getb(kp),cnt,MSR_CHAR_BUF_SIZE);
		    //und den Schreibzeiger weiterschrieben
		    msr_incb(cnt,kp); 
		    len+=cnt;
		    msr_buf_printf(kp,"\"/>\n");
		    //Test, ob noch genug Platz im Puffer ist
		    if(len > kp->bufsize) 
			msr_charbuf_realloc(kp,(kp->bufsize*3)/2); //um 50% gr��er
		    
		}
		break;
	}

    }
}


void msr_value_copy(int start,int incr,int cnt,struct msr_kanal_list *kanal,void *vbuf)
{

#define CPH(VTYP) \
            do {                                                                    \
	    for(i=0;i<cnt;i++){                                                     \
		((VTYP *)vbuf)[i] = (*(VTYP *)(k_base + (int)kanal->p_adr + j*k_blocksize));   \
		j+=incr;                                                            \
		j%=k_buflen;                                          \
                }                                                                   \
            }                                                                       \
            while (0)

    int i,j;

    j=start;

    switch(kanal->p_var_typ)
    {
	case TUCHAR:
	case TCHAR:{ CPH(char); }
	    break;
	case TSHORT:
	case TUSHORT:{ CPH(short int); }
	    break;
	case TINT: 
	case TUINT:
	case TLINT:
	case TULINT: { CPH(int);  }
	    break;
        case TTIMEVAL: {CPH(struct timeval); }
            break;
	case TFLT:{ CPH(float);   }
	    break;
	case TDBL:{ CPH(double);   }
	    break;
	default: break;
    }
#undef CPH
}


void msr_comp_grad(void *vbuf,int cnt,enum enum_var_typ p_var_typ) {

    int i;

#define CPG(VTYP) \
            do {                                            \
	    for(i=cnt-1;i>0;i--){                           \
		((VTYP *)vbuf)[i]-= ((VTYP *)vbuf)[i-1];    \
                }                                           \
            }                                               \
            while (0)

    switch(p_var_typ)
    {
	case TCHAR: { CPG(char); }   //unsigned geht nicht, da auch negative Werte m�glich....
	    break;
	case TINT: 
	case TLINT: { CPG(int); } 
	    break;
	case TFLT:{ CPG(float); }
	    break;
	case TDBL:{ CPG(double); }
	    break;
	default: break;
    }
#undef CPG
}


int msr_value_printf(struct msr_char_buf *buf,int cnt,enum enum_var_typ p_var_typ,void *vbuf,unsigned int prec)
{
    int i;
    int len = 0;

#define CPPF(VTYP,FI)                                 \
 do {                                                 \
	    VTYP tmp_value;                           \
	    for(i=0;i<cnt;i++) {                      \
		tmp_value = ((VTYP *)vbuf)[i];        \
		msr_buf_printf(buf,FI,tmp_value);     \
		if(i != cnt-1)                        \
		    msr_buf_printf(buf,",");          \
	    }                                         \
  }                                                   \
  while(0)

    switch(p_var_typ)
    {
	case TCHAR: CPPF(char,"%i");
	    break;
	case TUCHAR: CPPF(unsigned char,"%u");
	    break;
	case TSHORT:CPPF(short int,"%i");
	    break;
	case TUSHORT:CPPF(unsigned short int,"%u");
	    break;
	case TINT:
	case TLINT: CPPF(int,"%i");
	    break;

	case TUINT:
	case TULINT: CPPF(unsigned int,"%u");
	    break;

        case TTIMEVAL: {
            struct timeval tmp_value;
	    for(i=0;i<cnt;i++) {
	      tmp_value = ((struct timeval *)vbuf)[i];
	      len+=msr_buf_printf(buf,"%u.%.6u",(unsigned int)tmp_value.tv_sec,(unsigned int)tmp_value.tv_usec);
	      if(i != cnt-1)
		len+=msr_buf_printf(buf,",");
	    }
	}
	    break;

	case TFLT:
	    CPPF(float,"%.16g");
	    break;
	case TDBL:
	    CPPF(double,"%.16g");
	    break;
	default: break;
    }
    return len;
}
#undef CPPF

int msr_value_printf_base64(struct msr_char_buf *buf,int cnt,enum enum_var_typ p_var_typ,void *vbuf)
{
    int lin;  //L�nge des zu konvertierenden Puffers in byte
    int lout=0;

    switch(p_var_typ)
    {
	case TUCHAR:
	case TCHAR: {
	    lin = cnt * sizeof(char);
	    break;
	}
	case TSHORT:
	case TUSHORT: {
	    lin = cnt * sizeof(short int);
	    break;
	}

	case TINT: 
	case TUINT:
	case TLINT:
	case TULINT: { /* FIXME Zahlenformat */
	    lin = cnt * sizeof(int);
	    break;
	}
        case TTIMEVAL:{
	    lin = cnt * sizeof(struct timeval);
	    break;
	}

	case TFLT:{
	    lin = cnt * sizeof(float);
	    break;
	}
	case TDBL:{
	    lin = cnt * sizeof(double);
	    break;
	}
	default: 
	    lin = 0;
	    break;
    }
    if(lin > 0) {
	//direkt in den Characterpuffer drucken
	lout=gsasl_base64_encode ((char const *)vbuf,lin, msr_getb(buf), MSR_CHAR_BUF_SIZE);
	//und den Schreibzeiger weiterschrieben
	msr_incb(lout,buf); 
    }
    return lout;
}

int channel_data_change(struct msr_kanal_list *kanal,unsigned int index) { //vergleicht, ob sich der Wert eines Kanal zwischen zwei Abtastschritten ge�ndert hat

    char *p1;
    char *p2;

    int j;
    p1 = (char *)(k_base + index * k_blocksize + (int)kanal->p_adr);  //Startadresse des aktuellen Wertes
    j = (index + k_buflen - kanal->sampling_red) % k_buflen;  //der vorherige Wert
    p2 = (char *)(k_base + j * k_blocksize + (int)kanal->p_adr);

    for(j=0;j<kanal->dataSize;j++)
	if(p1[j] != p2[j]) 
	    return 1;

    return 0;
}



/*
***************************************************************************************************
*
* Function: msr_write_kaenale_to_char_buffer2
*
* Beschreibung: Schreibt die Kanalwerte in einen Zeichenringpuffer (f�r die Daten�bertragung zum Client)
*               F�r individuellen Datenverkehr, diese Funktion wird jede Zeitscheibe aufgerufen
*                      
* Parameter: struct msr_dev *dev : Device
*            
*
* R�ckgabe:  L�nge des Strings
*               
* Status: exp
*
***************************************************************************************************
*/

void msr_write_kanaele_to_char_buffer2(struct msr_dev *dev)
{
    struct msr_char_buf *kp = dev->read_buffer;        /* Characterringpuffer, in den geschrieben werden soll */

    struct msr_send_ch_list *element = NULL;

    unsigned int start;
    int incr; 
    int cnt;
    int dohead = 0;
    int len = 0;
    enum enum_var_typ p_var_typ;

#define LEV_RP ((k_buflen + msr_kanal_write_pointer - dev-> msr_kanal_read_pointer) % k_buflen)

    /* Datenfeld beginnen */
    FOR_THE_LIST(element,dev->send_ch_head) {
	if ((element) && (element->kanal)){
	    /* pr�fen ob schon gesendet werden soll */
	    if((element->reduction != 0 && !element->event && ((dev->msr_kanal_read_pointer % (element->reduction*element->bs*element->kanal->sampling_red) == 0))) 
	       || (element->event && dev->triggereventchannels)  //eventkanal und trigger ausgel�st
	       || ((element->event) && channel_data_change(element->kanal,dev->msr_kanal_read_pointer))) {  //eventkanal und Trigger durch �nderung des Wertes
		if(dohead == 0) {
		    if(dev->timechannel) {
			len+=msr_buf_printf(kp,"<data level=\"%d\" time=\"",(LEV_RP*100)/k_buflen);
			printChVal(kp,dev->timechannel,dev->msr_kanal_read_pointer/dev->timechannel->sampling_red); // der Lesezeiger steht immer vor dem letzten Wert siehe msr_lists.c
			len+=msr_buf_printf(kp,"\">\n");
		    }
		    else
			len+=msr_buf_printf(kp,"<data>\n");

		    dohead = 1;
		}

		if(element->event) {
		    start = dev-> msr_kanal_read_pointer;
		    incr = element->kanal->sampling_red;
		    cnt = 1;
		}
		else {
		    //startindex berechnen
		    start = (k_buflen + dev-> msr_kanal_read_pointer - element->kanal->sampling_red*element->reduction*(element->bs-1)) % k_buflen;
		    //increment berechnen
		    incr = element->reduction * element->kanal->sampling_red;
		    //Anzahl
		    cnt = element->bs;
		}
		//Vartyp
		p_var_typ = element->kanal->p_var_typ;


		msr_value_copy(start,incr,cnt,element->kanal,dev->cinbuf);
		//jetzt stehen die Werte schon in dev->cinbuf und zwar sch�n hintereinander

		//FIXME, hier mu� jetzt noch die Kompression rein
		switch(element->cmode) {
		    case MSR_NOCOMPRESSION:
			break;
		    case MSR_GRADIENT: //Gradienten
			if(p_var_typ != TUINT && p_var_typ != TUCHAR && p_var_typ != TULINT) {
			    msr_comp_grad(dev->cinbuf,cnt,p_var_typ);
			}
			break;
		    case MSR_DCT: //DCT
			//FIXME noch implementieren
			break;

		    default: 
			break;
		}

		if(element->event)
		    len+=msr_buf_printf(kp,"<E c=\"%d\" d=\"",element->kanal->index);
		else
		    len+=msr_buf_printf(kp,"<F c=\"%d\" d=\"",element->kanal->index);
		// jetzt die Codierung

		switch(element->codmode) {
		    case MSR_CODEASCII:

			len+=msr_value_printf(kp,cnt,p_var_typ,dev->cinbuf,element->prec);
			break;
		    case MSR_CODEBASE64:

			len+=msr_value_printf_base64(kp,cnt,p_var_typ,dev->cinbuf);
			break;
		}
		len+=msr_buf_printf(kp,"\"/>\n");  /* war wohl das Ende der Kan�le */
	    }
	}
	//Test, ob noch genug Platz im Puffer ist
	if(len > kp->bufsize) 
	    msr_charbuf_realloc(kp,(kp->bufsize*3)/2); //um 50% gr��er
    }
	if(dohead == 1)
	    len+=msr_buf_printf(kp,"</data>\n");
	dev->triggereventchannels = 0;
#undef LEV_RP	
}


/*
***************************************************************************************************
*
* Function: msr_write_messages_to_buffer
*
* Beschreibung: �berpr�ft Messagekan�le auf �nderung an der Position "index" im Kanalringpuffer und schreibt auf alle devices
*                
*                      
* Parameter: index:Position in Kanalring zum Test auf �nderung von 0 auf irgend einen anderen Wert
*            
*
* R�ckgabe:  
*               
* Status: exp
*
***************************************************************************************************
*/

void msr_write_messages_to_buffer(unsigned int index)
{
    struct msr_mkanal_list *melement = NULL;
    struct msr_kanal_list *kanal = NULL;                                        
    struct timeval tv;                                   

    char *p1;
    char *p2;

    int j;
    int previsNull;
    int hasChanged;

    FOR_THE_LIST(melement,msr_mkanal_head) {           
	kanal = melement->kelement;
	previsNull = 1;
	hasChanged = 0;
	p1 = (char *)(k_base + index * k_blocksize + (int)kanal->p_adr);  //Startadresse des aktuellen Wertes
	if (melement->prevvalue == NULL) //nur beim ersten Durchgang .... 
	    melement->prevvalue = p1;

	p2 = melement->prevvalue;

	for(j=0;j<kanal->dataSize;j++) {
	    if(p1[j] != p2[j]) 
		hasChanged = 1;
	    if(p2[j] != 0) 
		previsNull = 0;
	}
	melement->prevvalue = p1;

	if(hasChanged && previsNull) {
	    if(timechannel) {
		//Timechannel ist struct timeval FIXME, hier noch abfrage auf typ rein
		tv = (*(struct timeval *)(k_base + index * k_blocksize + (int)timechannel->p_adr));
	    }
	    else { //lokale Zeit nehmen
		do_gettimeofday(&tv);                                                                    
	    }
	    msr_dev_printf("<%s time=\"%u.%.6u\" text=\"%s (%s)\"/>\n",
			   melement->mtyp,(unsigned int)tv.tv_sec,
			   (unsigned int)tv.tv_usec,melement->mtext,kanal->p_bez);       
	}
    }
}

int msr_anz_kanal(void)
{ 
    int count = 0;
    struct msr_kanal_list *element;
    FOR_THE_LIST(element,msr_kanal_head) 
	if (element)
	    count++;
    return count;
}

int msr_anz_param(void)
{
    int count = 0;
    struct msr_param_list *element;
    FOR_THE_LIST(element,msr_param_head) 
	if (element)
	    count++;
    return count;
}


//=====================================================================================
//
//Matlab/Simulink/RTW
//
//=====================================================================================


enum enum_var_typ ETL_to_MSR(unsigned int datatyp) {

/*     SS_DOUBLE  =  1,    /\* real_T    *\/ */
/*     SS_SINGLE  =  2,    /\* real32_T  *\/ */
/*     SS_INT8    =  3,    /\* int8_T    *\/ */
/*     SS_UINT8   =  4,    /\* uint8_T   *\/ */
/*     SS_INT16   =  5,    /\* int16_T   *\/ */
/*     SS_UINT16  =  6,    /\* uint16_T  *\/ */
/*     SS_INT32   =  7,    /\* int32_T   *\/ */
/*     SS_UINT32  =  8,    /\* uint32_T  *\/ */
/*     SS_BOOLEAN =  9   */

    enum enum_var_typ tt[] = {0, TDBL,TFLT,TUCHAR,TCHAR,TUSHORT,TSHORT,TUINT,TINT,TCHAR};

    return tt[datatyp];
}


/*
*******************************************************************************
*
* Function: msr_reg_rtw_param
*
* Beschreibung: Registrierung einer Variablen als Parameter (generell sind Variablen immer Matrizen - Matlab)
*               Diese Funktion mu� f�r jeden zu registrierenden Parameter aufgerufen werden
*
*
* Parameter: path: Ablagepfad
*            name: Name der Variablen
*            cTypeName: String in der der C-Variablenname steht
*            data: Adresse der Variablen
*            rnum: Anzahl Zeilen
*            cnum: Anzahl Spalten
*            dataType: Simulink/Matlab Datentyp
*            orientation: rtwCAPI_MATRIX_COL_MAJOR/rtwCAPI_MATRIX_ROW_MAJOR
*            dataSize: Gr��e der Variablen in byte (eines Elementes)
*
* R�ckgabe: 0: wenn ok, sonst -1
*
* Status:
*
*******************************************************************************
*/

//RTW ersetzt \n durch Leerzeichen, diese werden hier entfernt
#define RTWPATHTRIM(c)                \
do {                                                             \
    int i,j=0;                                                     \
    for(i=0;i<strlen(c)+1;i++)  {                                   \
	if (i<strlen(c)-1 && ((c[i] == '/' && c[i+1] == '/') || (c[i] == ' ' && c[i+1] == '/') || (c[i] == '/' && c[i+1] == ' ')))    \
        ; \
/* do nothing*/ \
        else                                                    \
	    c[j++] = c[i];                                       \
   }            \
} while(0)

int msr_reg_rtw_param( const char *model_name,
                       const char *path, const char *name, const char *alias,
                       const char *cTypeName,
		       void *data,
		       unsigned int rnum, unsigned int cnum,
		       enum si_datatype_t dataType, 
                       enum si_orientation_t orientation,
		       unsigned int dataSize){
    char *buf;
    char *rbuf,*info;
    char *upath;        //unique name
    char *value;
    int result=1;
    int dohide = 0;
    int isstring = 0;
    unsigned int pflag = 0;

    struct talist *alist = NULL;


    if(DBG > 0) printf("reg_rtw_param:%s|%s|%s, rnum: %i, cnum: %i\n",model_name,path,name,rnum,cnum);

    //Hilfspuffer

    buf = (char *)getmem(
            strlen(model_name) + strlen(path) + strlen(name) + 4
            +2+20);

    //erstmal den Namen zusammensetzten zum einem g�ltigen Pfad
    if (strlen(path))
        sprintf(buf, "/%s/%s/%s", model_name, path, name);
    else {
        sprintf(buf, "/%s/%s", model_name, name);
	if (DBG >0) printf("xxxxxxxxxxxxxxxxx\n");
    }
    //jetzt alle Ausdr�cke, die im Pfad in <> stehen extrahieren und interpretieren
    rbuf = extractalist(&alist,buf);

    RTWPATHTRIM(rbuf);
    if(rbuf[strlen(rbuf)-1] == '/') 
	rbuf[strlen(rbuf)-1] = 0;

    info = alisttostr(alist);

    //und registrieren, falls gew�nscht
    if(!(msr_flag & MSR_ALL_VARIABLES) && hasattribute(alist,"hide")) {
	value = getattribute(alist,"hide");
	if (value[0] == 0 || value [0] == 'p') {
//	    printf("Hiding Parameter: %s\n",buf);
	    dohide = 1;
	}
    }


    if(hasattribute(alist,"aic"))
	pflag = MSR_R | MSR_AIC;  //Input Channels sind ro und werden �ber <wpic> beschrieben
    else 
	pflag = MSR_R | MSR_W;

    upath = msr_unique_channel_name(rbuf);

    if(!dohide) {
	if(hasattribute(alist,"isstring") && (ETL_to_MSR(dataType) == TUCHAR || ETL_to_MSR(dataType) == TCHAR)) {
	    result = msr_cfi_reg_param2(upath,"","",data,rnum,cnum,orientation,TSTR,info,pflag,NULL,NULL);
	    isstring = 1;
	}
	else
	    result = msr_cfi_reg_param2(upath,"","",data,rnum,cnum,orientation,ETL_to_MSR(dataType),info,pflag,NULL,NULL);

	    //jetzt noch zus�tzlich die einzelnen Elemente registrieren, da das MSR-Protokoll das so haben will... 
	    //FIXME, hier eventuell noch die Registrierung einzelner Elemente gro�er Matrizen unterbinden, falls nicht 
	    //explizit per "forceelements" freigeben? Hm, 2008.12.16
	if (!hasattribute(alist,"hideelements") && isstring == 0) {
		if(rnum+cnum > 2) { 
		    int r,c;
		    void *p;
		    char *ibuf = (char *)getmem(40+3); //warum 40+3 ?? 2 Indizes a max 64bit in dezimal sind 20 Stelle je, dann 2 mal / und die 0
		    for (r = 0; r < rnum; r++) {
			for (c = 0; c < cnum; c++) {
			    MSR_CALC_ADR((void *)data,dataSize,orientation,rnum,cnum);
			    //neuen Namen
			    if (rnum == 1 || cnum == 1)  //Vektor
				sprintf(ibuf,"/%i",r+c);
			    else                         //Matrize
				sprintf(ibuf,"/%i/%i",r,c);
			    //p wird in MSR_CALC_ADR berechnet !!!!!!!!!!!
			    result = msr_cfi_reg_param2(upath,ibuf,"",p,1,1,orientation,ETL_to_MSR(dataType),info,pflag | MSR_DEP,NULL,NULL);
			}
		    }
		    freemem(ibuf);
		}
	    }

    }
    freemem(info);
    freemem(rbuf);
    freemem(buf);
    freemem(upath);
    freealist(&alist);
    return result;

}

int msr_reg_time(void *time)     //FIXME, hier wird die Zeit im falschen Format �bergeben 2008.11.18 typ stimmt, siehe tools/include/app_taskstats.h
{
  timechannel =  (struct msr_kanal_list *)msr_reg_kanal3("/Time","s","",
            time,TTIMEVAL,"",default_sampling_red);
    return 0;
}

int msr_reg_task_stats(
        int tid,
        void *time,
        void *exec_time,
        void *period,
        void *overrun)
{
    char buf[50];

    snprintf(buf, 50, "/Taskinfo/%i/TaskTime", tid);
    msr_reg_kanal3(buf,"us","",
            time,TTIMEVAL,"",default_sampling_red);

    snprintf(buf, 50, "/Taskinfo/%i/ExecTime", tid);
    msr_reg_kanal3(buf,"us","",
            exec_time,TUINT,"",default_sampling_red);

    snprintf(buf, 50, "/Taskinfo/%i/Period", tid);
    msr_reg_kanal3(buf,"us","",
            period,TUINT,"",default_sampling_red);

    snprintf(buf, 50, "/Taskinfo/%i/Overrun", tid);
    msr_reg_kanal3(buf,"","",
            overrun,TUINT,"",default_sampling_red);

    return 0;
}

/*
*******************************************************************************
*
* Function: msr_reg_signal
*
* Beschreibung: Registrierung einer Variablen als Kanal/Signal (generell sind Variablen immer Matrizen - Matlab)
*               Diese Funktion mu� f�r jeden zu registrierenden Kanal/Signal aufgerufen werden
*
*
* Parameter: path: Ablagepfad
*            name: Name der Variablen
*            cTypeName: String in der der C-Variablenname steht
*            offset: Offset im Datenblock (siehe msr_update)
*            rnum: Anzahl Zeilen
*            cnum: Anzahl Spalten
*            dataType: Simulink/Matlab Datentyp
*            orientation: rtwCAPI_MATRIX_COL_MAJOR/rtwCAPI_MATRIX_ROW_MAJOR
*            dataSize: Gr��e der Variablen in byte (eines Elementes)
*
* R�ckgabe: 0: wenn ok, sonst -1
*
* Status:
*
*******************************************************************************
*/

int msr_reg_rtw_signal( const char* model_name, 
                        const char *path, const char *name, const char *alias,
                        const char *cTypeName,
			unsigned long offset,                                              // !!!
			unsigned int rnum, unsigned int cnum,
			enum si_datatype_t dataType, 
                        enum si_orientation_t orientation,
			unsigned int dataSize){

    char *buf;
    char *rbuf,*info;
    char *upath;          //unique Pathname
    char *value;
    int result = 1,r,c;
    int dohide = 0;

    void *p;

    struct msr_kanal_list *channel;

    struct talist *alist = NULL;

//    printf("Kanaloffset: %d\n",(unsigned int)offset);
    //Hilfspuffer

    if(DBG > 0) printf("reg_rtw_signal:%s|%s|%s, rnum: %i, cnum: %i\n",model_name,path,name,rnum,cnum);

    buf = (char *)getmem(
            strlen(model_name) + strlen(path) + strlen(name) + 4
            +2+20);

    //erstmal den Namen zusammensetzten zum einem g�ltigen Pfad
    if (strlen(path))
        sprintf(buf, "/%s/%s/%s", model_name, path, name);
    else
        sprintf(buf, "/%s/%s", model_name, name);

    rbuf = extractalist(&alist,buf);

    RTWPATHTRIM(rbuf);

    if(rbuf[strlen(rbuf)-1] == '/') 
	rbuf[strlen(rbuf)-1] = 0;


    info = alisttostr(alist);

    //und registrieren (hier aber f�r Vektoren und Matrizen einen eigenen Kanal)


    //und registrieren, falls gew�nscht
    if(!(msr_flag & MSR_ALL_VARIABLES) && hasattribute(alist,"hide")) {
	value = getattribute(alist,"hide");
	if (value[0] == 0 || value [0] == 's' || value [0] == 'k')  {//signal oder kanal
//	    printf("Hiding Channel: %s\n",buf);
	    dohide = 1;
	}
    }

    upath = msr_unique_channel_name(rbuf);
    //    if (strcmp(upath,rbuf) != 0) printf("%s->%s\n",rbuf,upath);   

    if(!dohide) {
	if(rnum+cnum > 2) {
	    char *ibuf = (char *)getmem(40+3); //warum 40+3 ?? 2 Indizes a max 64bit in dezimal sind 20 Stelle je, dann 2 mal / und die 0
	    for (r = 0; r < rnum; r++) {
		for (c = 0; c < cnum; c++) {
		    MSR_CALC_ADR((void *)offset,dataSize,orientation,rnum,cnum);
		    //neuen Namen
		    if (rnum == 1 || cnum == 1)  //Vektor
			sprintf(ibuf,"/%i",r+c);
		    else                         //Matrize
			sprintf(ibuf,"/%i/%i",r,c);
		    //p wird in MSR_CALC_ADR berechnet !!!!!!!!!!!
		    channel = (struct msr_kanal_list *)msr_reg_kanal4(upath,ibuf,(char *)alias,"",p,ETL_to_MSR(dataType),info,default_sampling_red);
		    //ist der Kanal ein Messagekanal, dann in die Liste eintragen
		    if(hasattribute(alist,"messagetyp")) 
			msr_reg_mkanal(channel,getattribute(alist,"messagetyp"),getattribute(alist,"text"));
		    result |= (int)channel;

		}
	    }
	    freemem(ibuf);
	}
	else {  //ein Sklarer Kanal
	    channel= (struct msr_kanal_list *)msr_reg_kanal4(upath,"",(char *)alias,"",(void *)offset,ETL_to_MSR(dataType),info,default_sampling_red);
	    if(hasattribute(alist,"messagetyp")) 
		msr_reg_mkanal(channel,getattribute(alist,"messagetyp"),getattribute(alist,"text"));
	    result |= (int)channel;
	}

    }

    freemem(info);
    freemem(rbuf);
    freemem(buf);
    freemem(upath);
    freealist(&alist);

    return result;

}





