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

#include "etl_datatype.h"

enum orientation_t {
    orientation_scalar,
    orientation_vector,
    orientation_matrix_row_major,
    orientation_matrix_col_major,
};

struct capi_signals {
    void* address;
    enum datatype_t datatype;
    unsigned int sample_time_domain;
    enum orientation_t orientation;
    unsigned int rows;
    unsigned int cols;
    const char* path;
};

struct capi_parameters {
    void* address;
    enum datatype_t datatype;
    enum orientation_t orientation;
    unsigned int rows;
    unsigned int cols;
    const char* path;
};
