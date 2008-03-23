/***********************************************************************
 *
 * $Id$
 *
 * Header for rtw_data_info.c
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

#include <stddef.h>
#include "defines.h"    // RT_MODEL

#include "rtmodel.h"

const char* rtw_capi_init(
        RT_MODEL *rtM,
        const char* (**si)(struct signal_info*, const char**),
        const char* (**pi)(struct signal_info*, const char**),
        unsigned int *max_signals,
        unsigned int *max_parameters,
        unsigned int *max_path_len
        );
