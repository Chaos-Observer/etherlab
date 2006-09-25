/**
 * @file
 * Header file for task statistics
 *
 * The following functions are defined in @c <model>.c, the RTW product of a 
 * Simulink model. These functions enable calling code that sets various
 * variables defined in @c <model>.c directly out of the main RTAI thread.
 */
void mdlSetStats(double execTime, double timeStep, unsigned int overrun);
void mdlSetWorldTime(double worldTime);
void mdlInitWorldTime(double worldTime);
