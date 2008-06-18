/***********************************************************************
 *
 * $Id$
 *
 * This file defines the data widths for the supported types.
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
 ***********************************************************************/

#include "include/etl_data_info.h"

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
#endif

// Data type widths. 
// DO NOT change these without updating etl_data_types.h
size_t si_data_width[] = { 0,
    sizeof(double), sizeof(float),
    sizeof(uint8_t), sizeof(int8_t),
    sizeof(uint16_t), sizeof(int16_t),
    sizeof(uint32_t), sizeof(int32_t),
    sizeof(uint8_t)
};
