/********************************************************************
 * $Id$
 *
 * Header file for the model.
 * Here are the includes and function prototypes needed for the main 
 * model file. This file must be included in the model file.
 * 
 * Copyright (C) 2008  Richard Hacker
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *************************************************************************/

#include "model_defines.h"      // Model variables
#include "defines.h"   // STR()
#include "cvt.h"
#include "param.h"

const char * MdlInit(void);
const char *MdlStep(unsigned int, double);
void MdlExit(void);

extern unsigned int subtask_decimation[];
