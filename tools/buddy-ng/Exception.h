/* **********************************************************************
 *  $Id$
 * **********************************************************************/

/*
 * @note Copyright (C) 2008 Richard Hacker
 * <lerichi@gmx.net>
 *
 *  License: GPL
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef EXCEPTION_H
#define EXCEPTION_H

#include <stdexcept>
#include <string>
#include <sstream>

#include <cerrno>
#include <cstring>

struct Exception: public std::runtime_error {
    Exception(const std::string& s): std::runtime_error(s) {}
    Exception(const std::ostringstream& s): std::runtime_error(s.str()) {}
};

struct FileOpenException: public std::runtime_error {
    FileOpenException(const std::string& filename): std::runtime_error(
            std::string("open(): Could not open file ")
            .append(filename)
            .append(": ")
            .append(strerror(errno))) {}
};

struct IoctlException: public std::runtime_error {
    IoctlException(): std::runtime_error(
            std::string("ioctl(): ioctl error: ")
            .append(strerror(errno))) {}
};


#endif // EXCEPTION_H

