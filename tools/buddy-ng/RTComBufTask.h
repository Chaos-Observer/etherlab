/* **********************************************************************
 *  $Id$
 * **********************************************************************/

/*
 * Copyright (C) 2008 Richard Hacker
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

#ifndef RTCOMIOTASK_H
#define RTCOMIOTASK_H

#include "Task.h"

#include <streambuf>

class RTComIOTask: public Task, public std::streambuf {
    public:
        RTComIOTask(Task* parent, int fd, 
                unsigned int buflen = 4096, unsigned int max_buffers = 0);
        ~RTComIOTask();

        int write(int fd);

        void reset();

        void hello();
        void send(const std::string& s);

    private:
        const int fd;

        const unsigned int buflen;
        const unsigned int max_buffers;

        int new_page();

        // Output functions reimplemented from streambuf
        std::streamsize xsputn(const char *s, std::streamsize n);
        int overflow(int c);
        int sync();

        char *wptr;     // Write pointer of the output
        char *wbuf;     // Current write buffer
        char *ibuf;     // Current input buffer

        char* lastPptr;
        unsigned int bufSize;
        void pbump(int);
        void setp(char*, char*);
//        std::streampos seekoff ( std::streamoff off, 
//                std::ios_base::seekdir way, 
//                std::ios_base::openmode which = 
//                   std::ios_base::in | std::ios_base::out );

        // Buffer list.
        // Output buffer is the first buffer
        // Input buffer is the last buffer
        std::list<char *> buf;
};

#endif // RTCOMIOTASK_H
