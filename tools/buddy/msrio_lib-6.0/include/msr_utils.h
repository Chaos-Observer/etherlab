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

















