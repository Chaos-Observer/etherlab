/**************************************************************************************************
*
*                          utils.h
*
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
*           $RCSfile: msr_utils.h,v $
*           $Revision: 1.3 $
*           $Author: hm $
*           $Date: 2006/05/12 12:39:46 $
*           $State: Exp $
*
*
*           $Log: msr_utils.h,v $
*           Revision 1.3  2006/05/12 12:39:46  hm
*           *** empty log message ***
*
*           Revision 1.2  2005/08/24 16:49:18  hm
*           *** empty log message ***
*
*           Revision 1.1  2005/06/14 12:34:59  hm
*           Initial revision
*
*           Revision 1.6  2005/01/07 09:45:54  hm
*           *** empty log message ***
*
*           Revision 1.5  2004/12/09 12:44:45  hm
*           *** empty log message ***
*
*           Revision 1.4  2004/12/02 09:08:39  hm
*           *** empty log message ***
*
*           Revision 1.3  2004/11/29 18:05:44  hm
*           *** empty log message ***
*
*           Revision 1.2  2004/09/21 18:11:14  hm
*           *** empty log message ***
*
*           Revision 1.1  2004/09/14 10:15:49  hm
*           Initial revision
*
*           Revision 1.9  2004/07/23 14:44:21  hm
*           ..
*
*           Revision 1.8  2004/06/24 15:03:35  hm
*           *** empty log message ***
*
*           Revision 1.7  2004/06/11 07:31:15  hm
*           *** empty log message ***
*
*           Revision 1.6  2004/05/26 18:37:48  hm
*           *** empty log message ***
*
*           Revision 1.5  2004/05/26 17:38:16  hm
*           *** empty log message ***
*
*           Revision 1.4  2004/02/03 15:31:18  hm
*           *** empty log message ***
*
*           Revision 1.3  2003/11/19 09:32:51  hm
*           *** empty log message ***
*
*           Revision 1.2  2003/11/19 09:31:29  hm
*           *** empty log message ***
*
*           Revision 1.1  2003/11/18 13:47:46  hm
*           Initial revision
*
*           Revision 1.2  2003/10/08 14:54:42  hm
*           *** empty log message ***
*
*           Revision 1.1  2003/07/17 09:21:11  hm
*           Initial revision
*
*           Revision 1.3  2003/02/13 14:16:27  hm
*           stringify eingefuegt
*
*           Revision 1.2  2003/01/22 11:01:01  hm
*           math.h angefuegt
*
*           Revision 1.1  2003/01/22 10:27:40  hm
*           Initial revision
*
*           Revision 1.2  2002/10/08 15:17:45  hm
*           *** empty log message ***
*
*           Revision 1.1  2002/07/09 09:11:08  sp
*           Initial revision
*
*           Revision 1.1  2002/03/28 10:34:49  hm
*           Initial revision
*
*           Revision 1.3  2002/02/16 20:58:58  hm
*           *** empty log message ***
*
*           Revision 1.2  2002/01/28 12:13:11  hm
*           *** empty log message ***
*
*           Revision 1.1  2002/01/25 13:53:54  hm
*           Initial revision
*
*
*
*
**************************************************************************************************/


/*--Schutz vor mehrfachem includieren------------------------------------------------------------*/

#ifndef _MSR_UTILS_H_
#define _MSR_UTILS_H_


#ifdef __KERNEL__
#include <linux/time.h>
#else
#include <sys/time.h>
#endif

//#include <math.h>

#define MAX(A,B) ((A) > (B) ? (A) : (B))             /* das Minimum von A und B wird zurückgeben */ 
#define MIN(A,B) ((A) < (B) ? (A) : (B))	     /* das Maximum von A und B wird zurückgeben */ 
#define BOUND(A,LOWER,UPPER) if((A)<LOWER) A = LOWER; else if ((A)>UPPER) A = UPPER /* Begrenzt A auf den Bereich von 
                                                        LOWER bis UPPER */
#define sqr(X)  ((X)*(X))			     /* Definition der sqr-Funktion */
#define pow3(X) ((X)*(X)*(X))			     /* Definition der hoch 3 Funktion	 */
#define signum(X) ((X) >= 0 ? (1) : (-1))               /* Signum Funktion */


#define MACH_EPS 0.220446049250313e-016   /* Maschinengenauigkeit = 2 hoch -52 */
#define EPSQUAD  4.930380657631324e-032
#define EPSROOT  1.490116119384766e-008
#define MAXROOT  1.0e150
/* #define PI       3.1415926535 */
#define PI (4.0*atan(1.0))
#define Pi PI
#define pi PI

#define RAD_TO_GRAD(x) ((x)/PI*180.0)
#define GRAD_TO_RAD(x) ((x)/180.0*PI)


#define int_T int
#define real_T double


#ifndef fabs
#define fabs(x) ((x) >= 0 ? (x) : (-1.0*x))
#endif

#define EQUALS_ZERO(x) (fabs(x) < MACH_EPS)



/* Macro zum einfügen der defines für die Maxwerte beim registrieren der Fehler in tvt_globals */
#define literal(val) #val
#define stringify(val) literal(val)


#endif

















