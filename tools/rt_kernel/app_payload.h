/***********************************************************************
 *
 * $Id$
 *
 * Header file for payload file data structure.
 * Payload files are files that are attached verbatim to the kernel
 * module. These are accessible via the /proc interface
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

struct payload_files {
    char *file_name;    // File name
    size_t len;         // Length of the data string
    char *data;         // Compressed data string
};

extern const struct payload_files payload_files[];
