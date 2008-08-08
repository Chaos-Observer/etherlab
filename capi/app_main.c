/**********************************************************************
 * $Id$
 *
 * Skeleton application file.
 *
 * This file is a skeleton for you to use to build your application. Change
 * app_init(), app_exit() and app_step() to suit you needs. Do not 
 * change the function names, these are required. 
 *
 *********************************************************************/

/* must be included in every application */
#include <application.h>

/** Initialise the application
 *
 * This function is called during module loading. Here you can put you
 * initialisation code.
 * Return:
 *     0: No initialisation error
 *    <0: Error during initialisation. The module will not be loaded
 *        and app_exit() is not called.
 */
const char *AppInit(void)
{
    return NULL;
}

/** Clean up application
 *
 * This function is called when unloading the module. Here you can put code
 * to be called when the application is exited.
 */
void AppExit(void)
{
}

/** Single calculation step
 *
 * This function is called when the application is required to make a single
 * calculation step. Note that when more than one sample time exists, app_step
 * can be interrupted and called again.
 * Arguments:
 *      st: Sample Time to be calculated. Single rate applications will only
 *          have one sample time st=0. Multirate applications will be called
 *          with the sample time index as specified in application_data.xml
 * Return:
 *      0: No error
 *     <0: An error occurred. The application calculation will be stopped.
 */
const char *TaskStep(unsigned int stidx)
{
    return NULL;
}


