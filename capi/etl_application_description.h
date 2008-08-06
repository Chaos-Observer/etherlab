/**************************************************************************
 *
 * $Id$
 *
 * This file defines structures that are needed to register signals and
 * parameters.
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
 ************************************************************************/

#include <etltypes.h>
#include <etl_data_info.h>

struct capi_variable {
    enum si_datatype_t data_type;
    unsigned int st_index;
    unsigned int dim[2];
    const char *name;
    const char *alias;
    const char *path;
    void *address;
    size_t *extra_dim;
};

extern struct capi_variable capi_signals[];
extern struct capi_variable capi_parameters[];
