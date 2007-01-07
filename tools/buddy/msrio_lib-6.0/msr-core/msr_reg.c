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
*           $RCSfile: msr_reg.c,v $
*           $Revision: 1.34 $
*           $Author: hm $
*           $Date: 2006/08/09 12:40:52 $
*           $State: Exp $
*
*
*           TODO: MDCT-Kompression, 
*                 Alias im Kanalnamen
*                 Floatformatierung auf ffloat() umbauen
*
*
*
*
**************************************************************************************************/


/*--includes-------------------------------------------------------------------------------------*/

#include <msr_target.h> 

#ifdef __KERNEL__
/* hier die Kernelbiblotheken */
#include <linux/config.h>
#include <linux/module.h>

#include <linux/sched.h>
#include <linux/kernel.h>
//#include <linux/malloc.h> 
#include <linux/slab.h> 
#include <linux/vmalloc.h> 
#include <linux/fs.h>     /* everything... */
#include <linux/proc_fs.h>
#include <linux/interrupt.h> /* intr_count */
#include <linux/mm.h>
#include <asm/msr.h> /* maschine-specific registers */
#include <asm/segment.h>
#include <asm/uaccess.h>
#include <asm/semaphore.h>  /* f�r das Locking von Stringvariablen */


#else
/* hier die Userbibliotheken */
#include <linux/a.out.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#endif

#include <linux/errno.h>  /* error codes */

//#include <math.h> 
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

#include <msr_rcsinfo.h>

RCS_ID("$Header: /vol/projekte/msr_messen_steuern_regeln/linux/kernel_space/msrio_lib-0.9/msr-core/RCS/msr_reg.c,v 1.34 2006/08/09 12:40:52 hm Exp hm $");

#define DBG 1
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
struct msr_meta_list *msr_meta_head = NULL;

volatile unsigned int msr_kanal_write_pointer = 0; /* Schreibzeiger auf den Kanalringpuffer */
volatile int msr_kanal_wrap = 0;                   /* wird = 1 gesetzt, wenn der Kanalringpuffer zum ersten Mal �berl�uft */


extern struct timeval msr_loading_time;  //in msr_proc.c

extern struct msr_char_buf *msr_in_int_charbuffer;  /* in diesen Puffer darf aus Interruptroutinen
                                                       hereingeschrieben werden */

extern struct msr_char_buf *msr_user_charbuffer;    /* in diesen Puffer wird aus Userroutinen read,write,... */

#ifdef __KERNEL__
DECLARE_MUTEX(strwrlock);  /* String-Read-Write-Lock */

#else
extern void *prtp;
extern int (*newparamflag)(void*, void*, size_t);  //Funktion, die aufgerufen werden mu�, wenn ein Parameter beschrieben wurde

#endif

void msr_reg_meta(char *path,char *tag){ //opsolete
}


/*
***************************************************************************************************
*
* Function: msr_clean_meta_list
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

void msr_clean_meta_list(void){ //opsolete
}


#define MSR_CALC_ADR(_START,_DATASIZE,_ORIENTATION,_RNUM,_CNUM)   \
do {                                                              \
if (_ORIENTATION == var_MATRIX_COL_MAJOR) 		          \
    p = _START + (_CNUM * r + c)*_DATASIZE;	                  \
else if (_ORIENTATION == var_MATRIX_ROW_MAJOR)		          \
    p = _START + (_RNUM * r + c)*_DATASIZE;	                  \
else								  \
    p = _START + (r + c)*_DATASIZE;  			          \
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
#ifdef __KERNEL__
    FPARAM(double,"%s%i.%.6i",F_FLOAT);
#else
    FPARAM(double,"%f",);
#endif
}

int r_p_flt(struct msr_param_list *self,char *buf) {
#ifdef __KERNEL__
    FPARAM(float,"%s%i.%.6i",F_FLOAT);
#else
    FPARAM(float,"%f",);
#endif
}


int r_p_char(struct msr_param_list *self,char *buf) {
    FPARAM(char,"%i",);
}

int r_p_uchar(struct msr_param_list *self,char *buf) {
    FPARAM(unsigned char,"%i",);
}

#undef FPARAM

int r_p_str(struct msr_param_list *self,char *buf) {
    int len;
#ifdef __KERNEL__
    down(&strwrlock);
//    len=sprintf(buf,"%s",*(char **)self->p_adr);
    len=sprintf(buf,"%s",(char *)self->p_adr);
    up(&strwrlock);
#else
//    len=sprintf(buf,"%s",*(char **)self->p_adr);
    len=sprintf(buf,"%s",(char *)self->p_adr);
#endif
    return len;
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
  	  tmp_data = (VTYP)VFUNKTION(next+1,&next);				      \
  	  if(index>= anz) {							      \
  	      result|=4;							      \
  	      break;								      \
  	  }									      \
  	  /* Test auf Limits */							      \
  	  if(ugbuf && (tmp_data < ug)) {					      \
  	      tmp_data = ug;		               				      \
  	      result|=1;							      \
  	  }									      \
  	  if(ogbuf && (tmp_data > og)) {					      \
  	      tmp_data = og;							      \
  	      result|=2;							      \
  	  }									      \
  	  *(VTYP *)(self->p_adr+index*self->dataSize) = tmp_data; /* und zuweisen */  \
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
  	  hex_to_bin(buf+(i*2*self->dataSize),(unsigned char*)&tmp_data,self->dataSize*2,sizeof(tmp_data));  \
  	  if(index>= anz) {							      \
  	      result|=4;							      \
  	      break;								      \
  	  }									      \
  	  /* Test auf Limits */							      \
  	  if(ugbuf && (tmp_data < ug)) {					      \
  	      tmp_data = ug;		               				      \
  	      result|=1;							      \
  	  }									      \
  	  if(ogbuf && (tmp_data > og)) {					      \
  	      tmp_data = og;							      \
  	      result|=2;							      \
  	  }									      \
  	  *(VTYP *)(self->p_adr+index*self->dataSize) = tmp_data; /* und zuweisen */  \
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
    char *tmpstr;

#ifdef __KERNEL__
    down(&strwrlock);
#endif
//    tmpstr = *(char **)self->p_adr;
    tmpstr = (char*)self->p_adr;
    if(tmpstr != NULL) 
	freemem(tmpstr);
    if(strlen(buf) > 0){
	tmpstr = getmem(strlen(buf)+1);
	strcpy(tmpstr,buf);
    }
    else { // String hat die L�nge Null
	tmpstr = getmem(1);
	strcpy(tmpstr,"");
    }
//    *(char **)self->p_adr = tmpstr;
    self->p_adr = (void *)tmpstr;
#ifdef __KERNEL__
    up(&strwrlock);
#endif
    //jetzt noch den ge�nderten String an alle anderen Clients melden
    msr_dev_printf("<pu index=\"%i\"/>",self->index);

//    cnt = msr_print_param_list(msr_getb(msr_user_charbuffer),self->p_bez,NULL,0,0);  //an alle Clients...
//    msr_incb(cnt,msr_user_charbuffer);
    return 0;
}


#undef WPARAM

// usw.



//Vergleichsfunktion mit cbuf f�r Numerische Werte
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

//und f�r Strings
void str_free(struct msr_param_list *self) {
    char *tmpstr;
    if(DBG > 0) printk("Free on %s\n",self->p_bez);
//    tmpstr = *(char **)self->p_adr;
    tmpstr = (char *)self->p_adr;
    if(tmpstr != NULL) 
	freemem(tmpstr);
}




int msr_cfi_reg_param(char *bez,char *einh,void *adr,int rnum, int cnum,int orientation,enum enum_var_typ typ,
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
	    printk("Strings k�nnen mit dieser Funktion nicht registriert werden.\n");
	    return 0;
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



/* wichtig, Vorbelegungen von **adr werden immer durch den Wert in *init �berschrieben !!!*/

int msr_reg_str_param(char *bez,char *einh,//char **adr,  FIXME 
		      unsigned int flags,
		      char *init,
		      void (*write)(struct msr_param_list *self),
		      void (*read)(struct msr_param_list *self))
{
    char *tmpstr;
    char *info = "";
    void *adr = NULL;
    struct msr_param_list *element = NULL, *prev = NULL;                                                 
    MSR_INIT_LIST_HEADER(msr_param_head,msr_param_list);  //Wichtig, strings werden per Definition nicht aus Interruptroutinen heraus beschrieben
                                                            //d.h. die Aktualisierung an alle Clients wird vorgenommen, falls der String von irgendeinem
                                                            //Client beschrieben wird
    element->p_flags = flags;                                                                    
    element->p_write = write;                                                                    
    element->p_read = read;                                                                      
    element->dataSize = 0;  //das variabel
    element->rnum = 0;
    element->cnum = 0;
    element->r_p = r_p_str;
    element->w_p = w_p_str;
    element->free = str_free;
    element->p_chk = NULL;   //wird beim Beschreiben schon zur�ckgeschickt

    /* und direkt initialisieren */
    element->p_var_typ = TSTR; 

    /*if(element->p_adr != NULL) */          /* falls der Speicher schon belegt ist, 
                                                freigeben geht nicht bei statische belegtem Speicherplatz */
/*	freemem(element->p_adr); */

    if(strlen(init) > 0){
	tmpstr =  getmem(strlen(init)+1);
	strcpy((char *)tmpstr,init);
    }
    else { // String hat die L�nge Null
      tmpstr = getmem(1);
      strcpy(tmpstr,"");
    }

//alt    *(char **)element->p_adr = tmpstr;  //und noch die �bergebende Variable aktualisieren 
                                        //hier�ber darf man nur Morgens zwischen 9 u 11 nachdenken Hm
    element->p_adr = (void *)tmpstr;


    /* else element->p_adr = NULL; HM 3.6.02 ausgeklammert, da ja in element->p_adr was ordentliches drinstehen kann*/
    return (int)element;
}


int msr_reg_funktion_call(char *bez,void (*write)(struct msr_param_list *self))
{
    char *einh = "";  //f�r MSR_INIT_LIST_HEADER
    char *info = "";  //f�r MSR_INIT_LIST_HEADER
    void *adr = NULL;
    struct msr_param_list *element = NULL, *prev = NULL;                                                 

    MSR_INIT_LIST_HEADER(msr_param_head,msr_param_list);
    element->p_flags = MSR_W | MSR_R;                                                                    
    element->p_write = write;      //FIXME, hier mu� noch was passieren... da Fehlermeldung des Compilers 
    element->p_read = NULL;                                                                      
    element->dataSize = 0;
    element->p_var_typ = TFCALL; 
    element->r_p = NULL;
    element->w_p = NULL;
    element->free = NULL;
    element->p_chk = NULL;
    return (int)element;
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
*               Vorsicht, diese Funktion darf nicht im Interrupt aufgerufen werden !!!!!!!!!
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
		    msr_dev_printf("<pu index=\"%i\"/>",element->index);  //Atomarer Aufruf
	}
    }
    else {
	element = p;
	if(element->p_chk) 
	    if(element->p_chk(element) == 1) 
		msr_dev_printf("<pu index=\"%i\"/>",element->index);  //Atomarer Aufruf
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

int msr_print_param_list(char *buf,char *aname,char *id,int shrt,int mode)
{
    struct msr_param_list *element;
    unsigned int len = 0;
    char *or_str[] = {ENUM_OR_STR};

    /* Element in der Liste suchen */
    FOR_THE_LIST(element,msr_param_head) {
	if (element &&  ((aname == NULL) || (strstr(element->p_bez,aname) == element->p_bez))){  //suche alle Parameter, die mit aname anfangen 

	    if(element->p_read !=NULL) /* erst die Aktualisierungsfunktion aufrufen */
		element->p_read(element);

	    if(shrt) {
		len+=sprintf(buf+len,"<parameter name=\"%s\" index=\"%i\"",
			     element->p_bez,element->index);
	    }
	    else {

		len+=sprintf(buf+len,"<parameter name=\"%s\" index=\"%i\" flags=\"%u\" datasize=\"%i\"",
			     element->p_bez,element->index,element->p_flags,element->dataSize);


		if(strlen(element->p_einh) > 0)
		    len+=sprintf(buf+len," unit=\"%s\"",element->p_einh);


		//Infostring
		if(element->info) //info ist schon im Format name="value"
		    len+=sprintf(buf+len," %s",element->info);


		//Kompatibilit�t zu alten Version in der Behandlung von Listen und Matrizen
		len+=sprintf(buf+len," typ=\"%s",enum_var_str[element->p_var_typ]); 
		if(element->cnum + element->rnum > 2) {  //Vektor oder Matrize
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

	    //Id
	    if(id)
		len+=sprintf(buf+len," id=\"%s\"",id);   //FIXME, noch notwendig ???

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

int msr_print_param_valuelist(char *buf,int mode)
{
    struct msr_param_list *element;
    unsigned int len = 0;

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
	 if (len > MSR_PRINT_LIMIT) return len;
    }
    return len;
}


/*
***************************************************************************************************
*
* Function: msr_write_param
*
* Beschreibung: beschreibt einen Wert 
*                      
* Parameter: buf:    Ringpuffer in dem Meldungen an die Applikation zur�ckgeschickt werden
*            aname:  der Name eines Parameters der beschrieben werden soll
*            avalue: der Wert als String        
*
* R�ckgabe: 
*               
* Status: exp
*
***************************************************************************************************
*/

#ifdef __KERNEL__
int check_loading_interval(void)
{
    int result;
    struct timeval now;
    do_gettimeofday((struct timeval*)&now);

    result=(now.tv_sec-msr_loading_time.tv_sec < MSR_PERSISTENT_LOADINGINTERVAL);
    if (!result) {
	msr_print_info("Time for persistent loading exceeded");
    }
    return result;

}
#else
int check_loading_interval(void) {
    return 0;
}
#endif

void msr_write_param(struct msr_dev *dev/*struct msr_char_buf *buf*/,char *aname,char* avalue,unsigned int si,int mode)
{
    struct msr_param_list *element;

    int result;

    struct msr_char_buf *buf = NULL;

    if(dev) 
	buf = dev->read_buffer;  //sonst NULL, dann gibts keine R�ckmeldungen, aber die Variablen werden doch geschrieben

    /* Element in der Liste suchen welches gemeint ist. */
    FOR_THE_LIST(element,msr_param_head) {
	if (element && (strcmp(aname,element->p_bez) == 0)) {  /* gefunden */

	    if((element->p_flags & MSR_W) == MSR_W || 
	       ((element->p_flags & MSR_P) == MSR_P && check_loading_interval())) {

		if(element->p_adr && element->w_p) {

		    result = element->w_p(element,avalue,si,mode);

		    if (result < 0 && dev)
			msr_buf_printf(buf,"<%s text=\"%s: Fehler beim Beschreiben eines Parameters. (%d)\"/>\n",
						    MSR_WARN,aname,result);

#ifndef __KERNEL__   
		    if(element->p_write !=NULL) /* noch die Aktualisierungsfunktion aufrufen */
			element->p_write(element);

		    if(newparamflag)  //FIXME Teile eines Arrays beschreiben mit si > 0 mu� noch ber�cksichtigt werden
			newparamflag(prtp,element->p_adr,element->dataSize*element->rnum*element->cnum); 

		    //im Userspace auch die Checkfunktion f�r dieses Element aufrufen
		    if((element->p_flags & MSR_DEP) == MSR_DEP) //alle Parameter �berpr�fen
			msr_check_param_list(NULL);
		    else
			msr_check_param_list(element);
//		    if(element->p_chk && (element->p_chk(element) == 1))
//			msr_dev_printf("<pu index=\"%i\"/>",element->index);  //Atomarer Aufruf
#endif
		}
#ifdef __KERNEL__
		/* behandelt auch FCALL !!!!!!!!!!!!!! */
		if(element->p_write !=NULL) /* noch die Aktualisierungsfunktion aufrufen */
		    element->p_write(element);
#endif
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



int getorder(unsigned int size) 
{
    unsigned int k=1;
    int i;
    for(i=0;i<=9;i++) {
	if(PAGE_SIZE*k >= size) 
	    return i;
	k*=2;
    }
    return -1;  //zu viel memoryangefordert
}

int msr_reg_kanal2(char *bez,void *alias,char *einh,void *adr,enum enum_var_typ typ,int red) {
    return msr_reg_kanal3(bez,alias,einh,adr,typ,"",red);
}

int msr_reg_kanal3(char *bez,void *alias,char *einh,void *adr,enum enum_var_typ typ,char *info,int red)
{
    struct msr_kanal_list *element = NULL, *prev = NULL;                                                 

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


/*     element->order = getorder(s); */
    
/*     if(element->order >=0) */
/* 	element->kbuf = (void *)__get_free_pages(GFP_KERNEL,element->order); */
/*     else { */
/* 	element->kbuf = 0;  //order zu gro� */
/* 	printk("Out of Memory for Channel allokation: %s\n",bez); */
/* 	msr_kanal_error_no = -1; */
/*     } */

/* nicht mehr n�tig, da auf Richards speicher zugegriffen wird
    element->kbuf = (void *)getmem(element->dataSize * element->bufsize); //vmalloc geht auch ?? ja geht auch, aber der Speicherzugriff dauert l�nger, 
                                         //da Virtueller Speicher noch umgerechnet werden mu� 26.07.2005 Ab/Hm

    if (!element->kbuf) {
	element->bufsize = 0;
	printk("Out of Memory for Channel allokation: %s\n",bez);
	msr_kanal_error_no = -1;
    }
    else
	msr_allocated_channel_memory+=element->dataSize * element->bufsize;

*/
    //und noch den alias FIXME, hier noch eine �berpr�fung
    element->alias = (char *)getmem(strlen(alias)+1);
    if(!element->alias) {
	printk("Out of Memory for Channel allokation: %s\n",bez);
    }
    else
	strcpy(element->alias,alias);

    return (int)element;


}



void msr_clean_kanal_list(void)
{
    struct msr_kanal_list *element;
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

	    if(element->info) 
		freemem(element->info);

	    if(element->alias) 
		freemem(element->alias);
	}
    }
    MSR_CLEAN_LIST(msr_kanal_head,msr_kanal_list);
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


int msr_print_kanal_list(char *buf,char *aname,int mode)
{
    struct msr_kanal_list *element;
    unsigned int len = 0;
    int index = 0;
    int wp = msr_kanal_write_pointer;  //aktuellen Schreibzeiger merken 

    //und den Vorg�nger bestimmen

    wp = (wp+k_buflen-default_sampling_red)%k_buflen;


    FOR_THE_LIST(element,msr_kanal_head) {
	if (element && ((aname == NULL) || (strcmp(aname,element->p_bez) == 0))) {
	    if(mode == 1) {
		len+=sprintf(buf+len,"<channel index=\"%.3i\" name=\"%s\"/>\n",
			     index,
			     element->p_bez);
	    }
	    else {
		len+=sprintf(buf+len,"<channel name=\"%s\" alias=\"%s\" index=\"%i\" typ=\"%s\" datasize=\"%i\" bufsize=\"%i\" HZ=\"%f\"",
			     element->p_bez,
			     element->alias,
			     index,
			     enum_var_str[element->p_var_typ],
			     element->dataSize,
			     element->bufsize,
			     1.0e6/(double)k_base_rate);

		//	if(strlen(element->p_einh) > 0)                            //Einheit nur, wenn auch da, eventuell steht sie auch in info
		//FIXME tuts noch nicht mit dlsd hier darf die unit nicht fehlen
		len+=sprintf(buf+len," unit=\"%s\"",element->p_einh);


		//Infostring
		if(element->info && strlen(element->info) > 0) //info ist schon im Format name="value"
		    len+=sprintf(buf+len," %s",element->info);

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
			len+=sprintf(buf+len,"%f\"/>\n",(*(float*)(k_base + wp * k_blocksize + (int)element->p_adr)));
			break;
		    case TDBL:
			len+=sprintf(buf+len,"%f\"/>\n",(*(double*)(k_base + wp * k_blocksize + (int)element->p_adr)));
			break;
 		    case TTIMEVAL:
			len+=sprintf(buf+len,"%u.%.6u\"/>\n",(unsigned int)(*(struct timeval*)(k_base + wp * k_blocksize + (int)element->p_adr)).tv_sec,
				                             (unsigned int)(*(struct timeval*)(k_base + wp * k_blocksize + (int)element->p_adr)).tv_usec);
			break;
		    default: break;
		}
	    }
	}
	index++;
    }

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
       }
	case TFLT: {
	    float tmp_value;
	    tmp_value = (*(float *)(k_base + index * k_blocksize + (int)kanal->p_adr));
	    if((tmp_value == 0.0))
		cnt=msr_buf_printf(buf,"0");
	    else
		cnt=msr_buf_printf(buf,"%f",tmp_value);
	    break;
	}
	case TDBL:{
	    double tmp_value;
	    tmp_value = (*(double *)(k_base + index * k_blocksize + (int)kanal->p_adr));
	    if((tmp_value == 0.0))
		cnt=msr_buf_printf(buf,"0");
	    else
		cnt=msr_buf_printf(buf,"%f",tmp_value);

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

    /* pr�fen ob schon gesendet werden soll */
    if(dev->msr_kanal_read_pointer % dev->reduction == 0) {
	
	/* Datenfeld beginnen */
	msr_buf_printf(kp,"<D c=\"");

	switch(dev->codmode) {
	    case MSR_CODEASCII:
		FOR_THE_LIST(element,dev->send_ch_head) {
		    if ((element) && (element->kanal)){
			printChVal(kp,element->kanal,dev->msr_kanal_read_pointer/element->kanal->sampling_red); 
			if(element->next) 
			    msr_buf_printf(kp,",");
			else 
			    msr_buf_printf(kp,"\"/>\n");  /* war wohl das Ende der Kan�le */
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
			cnt=gsasl_base64_encode ((char const *)dev->cinbuf,cnt, msr_getb(kp), MSR_CHAR_BUF_SIZE);
		    else
			cnt = bin_to_hex((unsigned char const *)dev->cinbuf,msr_getb(kp),cnt,MSR_CHAR_BUF_SIZE);
		    //und den Schreibzeiger weiterschrieben
		    msr_incb(cnt,kp); 
		    msr_buf_printf(kp,"\"/>\n");
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


int ffloat(char *buf,double x,unsigned int prec)
{
    return sprintf(buf,"%f",x);  
}


void msr_value_printf(struct msr_char_buf *buf,int cnt,enum enum_var_typ p_var_typ,void *vbuf,unsigned int prec)
{
    int i;

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
	      msr_buf_printf(buf,"%u.%.6u",(unsigned int)tmp_value.tv_sec,(unsigned int)tmp_value.tv_usec);
	      if(i != cnt-1)
		msr_buf_printf(buf,",");
	    }
	}
	    break;

	case TFLT:
	    CPPF(float,"%f");
/*	    for(i=0;i<cnt;i++) {
		msr_incb(ffloat(msr_getb(buf),((float *)vbuf)[i],prec),buf);
		if(i != cnt-1)
		    msr_buf_printf(buf,",");
		    } */
	    break;
	case TDBL:
	    CPPF(double,"%f"); /*
	    for(i=0;i<cnt;i++) {
		msr_incb(ffloat(msr_getb(buf),((double *)vbuf)[i],prec),buf);
		if(i != cnt-1)
		    msr_buf_printf(buf,",");
		    } */
	    break;
	default: break;
    }
}
#undef CPPF

void msr_value_printf_base64(struct msr_char_buf *buf,int cnt,enum enum_var_typ p_var_typ,void *vbuf)
{
    int lin;  //L�nge des zu konvertierenden Puffers in byte
    int lout;

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
* R�ckgabe:  
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
			msr_buf_printf(kp,"<data level=\"%d\" time=\"",(LEV_RP*100)/k_buflen);
			printChVal(kp,dev->timechannel,dev->msr_kanal_read_pointer/dev->timechannel->sampling_red); // der Lesezeiger steht immer vor dem letzten Wert siehe msr_lists.c
			msr_buf_printf(kp,"\">\n");
		    }
		    else
			msr_buf_printf(kp,"<data>\n");

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
		    msr_buf_printf(kp,"<E c=\"%d\" d=\"",element->kanal->index);
		else
		    msr_buf_printf(kp,"<F c=\"%d\" d=\"",element->kanal->index);
		// jetzt die Codierung

		switch(element->codmode) {
		    case MSR_CODEASCII:

			msr_value_printf(kp,cnt,p_var_typ,dev->cinbuf,element->prec);
			break;
		    case MSR_CODEBASE64:

			msr_value_printf_base64(kp,cnt,p_var_typ,dev->cinbuf);
			break;
		}
		msr_buf_printf(kp,"\"/>\n");  /* war wohl das Ende der Kan�le */
	    }
	}
    }
	if(dohead == 1)
	    msr_buf_printf(kp,"</data>\n");
	dev->triggereventchannels = 0;
#undef LEV_RP	
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


enum enum_var_typ RTW_to_MSR(unsigned int datatyp) {

/*     SS_DOUBLE  =  0,    /\* real_T    *\/ */
/*     SS_SINGLE  =  1,    /\* real32_T  *\/ */
/*     SS_INT8    =  2,    /\* int8_T    *\/ */
/*     SS_UINT8   =  3,    /\* uint8_T   *\/ */
/*     SS_INT16   =  4,    /\* int16_T   *\/ */
/*     SS_UINT16  =  5,    /\* uint16_T  *\/ */
/*     SS_INT32   =  6,    /\* int32_T   *\/ */
/*     SS_UINT32  =  7,    /\* uint32_T  *\/ */
/*     SS_BOOLEAN =  8   */

    enum enum_var_typ tt[9] = {TDBL,TFLT,TCHAR,TUCHAR,TSHORT,TUSHORT,TINT,TUINT,TCHAR};

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

int msr_reg_rtw_param( const char *path, const char *name, const char *cTypeName,
                   void *data,
                   unsigned int rnum, unsigned int cnum,
                   unsigned int dataType, unsigned int orientation,
		       unsigned int dataSize){
    char *buf;
    char *rbuf,*info;
    char *value;
    int result=1;
    int dohide = 0;

    struct talist *alist = NULL;


    //Hilfspuffer
    buf = (char *)getmem(strlen(path)+strlen(name)+2+20);

    //erstmal den Namen zusammensetzten zum einem g�ltigen Pfad
    if(path[0] != '/')
	sprintf(buf,"/%s/%s",path,name);
    else
	sprintf(buf,"%s/%s",path,name);




    //jetzt alle Ausdr�cke, die im Pfad in <> stehen extrahieren und interpretieren
    rbuf = extractalist(&alist,buf);

    RTWPATHTRIM(rbuf);
    if(rbuf[strlen(rbuf)-1] == '/') 
	rbuf[strlen(rbuf)-1] = 0;

    info = alisttostr(alist);

    //und registrieren, falls gew�nscht
    if(hasattribute(alist,"hide")) {
	value = getattribute(alist,"hide");
	if (value[0] == 0 || value [0] == 'p') {
//	    printf("Hiding Parameter: %s\n",buf);
	    dohide = 1;
	}
    }

    if(!dohide) {
	    result = msr_cfi_reg_param(rbuf,"",data,rnum,cnum,orientation,RTW_to_MSR(dataType),info,MSR_R | MSR_W,NULL,NULL);

	    //jetzt noch die einzelnen Elemente registrieren aber nur bis zu einer Obergrenze von ?? Stck 2006.11.06

	    if(rnum+cnum > 2 && rnum+cnum<100) {  //sonst werden es zu viele Parameter
		int r,c;
		void *p;
		char *buf2 = (char *)getmem(strlen(rbuf)+2+100); //warum 100 ??
		for (r = 0; r < rnum; r++) {
		    for (c = 0; c < cnum; c++) {
			MSR_CALC_ADR((void *)data,dataSize,orientation,rnum,cnum);
			//neuen Namen
			if (rnum == 1 || cnum == 1)  //Vektor
			    sprintf(buf2,"%s/%i",rbuf,r+c);
			else                         //Matrize
			    sprintf(buf2,"%s/%i,%i",rbuf,r,c);
			//p wird in MSR_CALC_ADR berechnet !!!!!!!!!!!
			result = msr_cfi_reg_param(buf2,"",p,1,1,orientation,RTW_to_MSR(dataType),info,MSR_R | MSR_W | MSR_DEP,NULL,NULL);
		    }
	    }
	    freemem(buf2);
	}

    }
    freemem(info);
    freemem(rbuf);
    freemem(buf);

    freealist(&alist);
    return result;

}

int msr_reg_time(void *time)
{
    msr_reg_kanal3("/Time","s","",
            time,TDBL,"",default_sampling_red);
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
            time,TDBL,"",default_sampling_red);

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

int msr_reg_rtw_signal( const char *path, const char *name, const char *cTypeName,
			unsigned long offset,                                              // !!!
			unsigned int rnum, unsigned int cnum,
			unsigned int dataType, unsigned int orientation,
			unsigned int dataSize){

    char *buf;
    char *rbuf,*info;
    char *value;
    int result = 1,r,c;
    int dohide = 0;

    void *p;

    struct talist *alist = NULL;

//    printf("Kanaloffset: %d\n",(unsigned int)offset);
    //Hilfspuffer

    buf = (char *)getmem(strlen(path)+2+20);

    //erstmal den Namen zusammensetzten zum einem g�ltigen Pfad
    if(path[0] != '/')
	sprintf(buf,"/%s",path);
    else
	sprintf(buf,"%s",path);


    rbuf = extractalist(&alist,buf);

    RTWPATHTRIM(rbuf);

    if(rbuf[strlen(rbuf)-1] == '/') 
	rbuf[strlen(rbuf)-1] = 0;


    info = alisttostr(alist);

    //und registrieren (hier aber f�r Vektoren und Matrizen einen eigenen Kanal)


    //und registrieren, falls gew�nscht
    if(hasattribute(alist,"hide")) {
	value = getattribute(alist,"hide");
	if (value[0] == 0 || value [0] == 's' || value [0] == 'k')  {//signal oder kanal
//	    printf("Hiding Channel: %s\n",buf);
	    dohide = 1;
	}
    }


    if(!dohide) {
	if(rnum+cnum > 2) {
	    char *buf2 = (char *)getmem(strlen(rbuf)+2+100); 
	    for (r = 0; r < rnum; r++) {
		for (c = 0; c < cnum; c++) {
		    MSR_CALC_ADR((void *)offset,dataSize,orientation,rnum,cnum);
		    //neuen Namen
		    if (rnum == 1 || cnum == 1)  //Vektor
			sprintf(buf2,"%s/%i",rbuf,r+c);
		    else                         //Matrize
			sprintf(buf2,"%s/%i/%i",rbuf,r,c);
		    //p wird in MSR_CALC_ADR berechnet !!!!!!!!!!!
		    result |= msr_reg_kanal3(buf2,(char *)name,"",p,RTW_to_MSR(dataType),info,default_sampling_red);
		}
	    }
	    freemem(buf2);
	}
	else {  //ein Sklarer Kanal
	    result |= msr_reg_kanal3(rbuf,(char *)name,"",(void *)offset,RTW_to_MSR(dataType),info,default_sampling_red);
	}

    }

    freemem(info);
    freemem(rbuf);
    freemem(buf);
    freealist(&alist);

    return result;

}







