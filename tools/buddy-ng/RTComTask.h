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

#ifndef RTCOMTASK_H
#define RTCOMTASK_H

#include "Task.h"
#include "RTComBufTask.h"
#include "RTComOStream.h"

#include <pcrecpp.h>
#include <sasl/sasl.h>

class RTTask;

class RTComTask: public Task {
    public:
        RTComTask(Task* parent, int fd, RTTask* rtTask);
        ~RTComTask();

        int read(int fd);

        void newModel(const std::string& model);
        void delModel(const std::string& model);

    private:
        const int fd;
        RTTask* const rtTask;
        RTComBufTask sb;

        void kill(Task*, int);

        char *inBuf;
        char *inBufEnd;
        char *inBufPtr;
        unsigned int inBufLen;

        enum ParserState_t {Init, Idle, LoginInit, LoginContinue, LoginFail,
            //Unknown, Login, List, 
            //Subscribe, Poll, Write
        };

        ParserState_t parserState;

        bool checkPass(const std::string& user, const std::string& pass);
        bool loggedIn;
        std::string userName;

        sasl_conn_t *sasl_connection;
        const char* mechanisms;
        unsigned int mechanism_len;
        int mechanism_count;
        sasl_callback_t *sasl_callbacks;
        std::list<std::string> sasl_options;

        static int sasl_getopt(void *context, const char *plugin_name,
                const char *option, const char **result, unsigned *len);

        const pcrecpp::RE login;
        const pcrecpp::RE capabilities;
        const pcrecpp::RE auth;
        const pcrecpp::RE length;
        const pcrecpp::RE empty;
        const pcrecpp::RE listModels;
        const pcrecpp::RE listSignals;
};

#endif // RTCOMTASK_H
