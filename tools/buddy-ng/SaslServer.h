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

class RTTask;

class RTComIOTask: public Task, public std::streambuf {
    public:
        RTComIOTask(Task* parent, RTTask* _rtTask, int fd, 
                unsigned int buflen = 4096, unsigned int max_buffers = 0);
        ~RTComIOTask();

        // Reimplemented from class Task
        int write(int fd);
        int read(int fd);

        void hello();
//        void send(const std::string& s);

        // Called when an application is added or removed dynamically
        void newApplication(const std::string& app);
        void delApplication(const std::string& app);

    private:
        const int fd;
        RTTask* const rtTask;

        /* The following attributes are needed to parse incoming data
         * from the network clients. */
        char *inBuf;
        char *inBufEnd;
        char *inBufPtr;
        unsigned int inBufLen;

        enum ParserState_t {Init, Idle, LoginInit, LoginContinue, LoginFail,
            //Unknown, Login, List, 
            //Subscribe, Poll, Write
        };

        ParserState_t parserState;

        /* The following attributes are needed to manage the output channel
         * to the network clients. */

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

        // List of output buffers to send to client
        std::list<char *> outBuf;

        sasl_conn_t *sasl_connection;
        const char* mechanisms;
        unsigned int mechanism_len;
        int mechanism_count;
        sasl_callback_t *sasl_callbacks;
        std::list<std::string> sasl_options;

        static int sasl_getopt(void *context, const char *plugin_name,
                const char *option, const char **result, unsigned *len);
};

#endif // RTCOMIOTASK_H
