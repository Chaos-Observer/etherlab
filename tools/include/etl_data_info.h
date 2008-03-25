/***********************************************************************
 *
 * $Id$
 *
 * This is the header file for the data types and organisation that is
 * supported by EtherLab
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

#ifndef ETLDATAINFO_H
#define ETLDATAINFO_H

#include <stddef.h>

// Data organisation
enum si_orientation_t {si_scalar, si_vector, si_matrix_row_major, 
    si_matrix_col_major};

// Data type definitions. 
// DO NOT change these without updating etl_data_types.c
enum si_datatype_t {
    si_double_T, si_single_T, 
    si_uint8_T, si_sint8_T, 
    si_uint16_T, si_sint16_T, 
    si_uint32_T, si_sint32_T, 
    si_boolean_T, 
    si_datatype_max            // This must allways be last;
};
extern size_t si_data_width[];

struct signal_info {
    unsigned int index;         // Index of signal or parameter
    unsigned int st_index;      // Sample time index
    unsigned int offset;        // Offset of signal from the structure base
    unsigned int rnum;          // Number of rows
    unsigned int cnum;          // Number of columns
    enum si_datatype_t data_type;      // Data Type
    enum si_orientation_t orientation; // Data organisation
    size_t data_size;           // Size of data type
    char alias[256];            // Signal alias name
    char path[];                // Path of signal
};

#endif // ETLDATAINFO_H
