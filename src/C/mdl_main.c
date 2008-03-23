/**********************************************************************
 * $Id$
 *
 * Skeleton model file.
 *
 * This file is a skeleton for you to use to build your model. Change
 * mdl_init(), mdl_exit() and mdl_step() to suit you needs. Do not 
 * change the function names, these are required. 
 *
 *********************************************************************/

/* model.h must be included in every model */
#include "model.h"

/** Initialise the model
 *
 * This function is called during module loading. Here you can put you
 * initialisation code.
 * Return:
 *     0: No initialisation error
 *    <0: Error during initialisation. The module will not be loaded
 *        and mdl_exit() is not called.
 */
const char *MdlInit(void)
{
    return NULL;
}

/** Clean up model
 *
 * This function is called when unloading the module. Here you can put code
 * to be called when the model is exited.
 */
void MdlExit(void)
{
}

/** Single calculation step
 *
 * This function is called when the model is required to make a single
 * calculation step. Note that when more than one sample time exists,
 * mdl_step can be interrupted and called again.
 * Arguments:
 *      st: Sample Time to be calculated. Single rate models will only
 *          have one sample time st=0. Multirate models will be called
 *          with the sample time index as specified in model_data.xml
 * Return:
 *      0: No error
 *     <0: An error occurred. The model calculation will be stopped.
 */
const char *MdlStep(unsigned int stidx, double world_time)
{
    return NULL;
}


