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

#include "RTComTask.h"
#include "SocketExcept.h"
#include "ConfigFile.h"

#include <iostream>
#include <string>
#include <cstring>
#include <cerrno>

using namespace std;

//************************************************************************
RTComTask::RTComTask(Task* parent, int _fd): Task(parent), fd(_fd),
    sb(this,fd), os(&sb),
    login("LOGIN\n$"),
    capabilities("CAPABILITIES\n$"),
    auth("AUTH\\s+(\\w+)\n(.*)"),
    length("{\\d+}"),
    empty("\\s*\n$")
//************************************************************************
{
    sasl_callback_t callbacks[] = {
        {SASL_CB_GETOPT, (int (*)())sasl_getopt, this},
        {SASL_CB_LIST_END, NULL, NULL},
    };
    sasl_callbacks = 
        new sasl_callback_t[sizeof(callbacks)/sizeof(sasl_callback_t)];
    memcpy(sasl_callbacks, callbacks, sizeof(callbacks));

    enableRead(fd);
    os.stdOut("RTCom v0.1");
    inBufPos = 0;
    parserState = Idle;
    loggedIn = false;

    if (SASL_OK != sasl_server_new( "login",
                NULL, /* my fully qualified domain name; 
                         NULL says use gethostname() */
                NULL, /* The user realm used for password
                         lookups; NULL means default to serverFQDN
                         Note: This does not affect Kerberos */
                NULL, NULL, /* IP Address information strings */
                sasl_callbacks, /* Callbacks supported only for this connection */
                0, /* security flags (security layers are enabled
                    * using security properties, separately)
                    */
                &sasl_connection)) {
        throw SocketExcept("Could not initiate a new SASL session.");
    }


    if (SASL_OK != sasl_listmech(
                sasl_connection,  // sasl_conn_t *conn,
                NULL,             // const char *user,
                "{",              // const char *prefix,
                " ",              // const char *sep,
                "}",              // const char *suffix,
                &mechanisms,      // const char **result,
                &mechanism_len,   // unsigned *plen,
                &mechanism_count  // unsigned *pcount
                )) {
        throw SocketExcept("Could not get SASL mechnism list.");
    }

    cerr << "sasl_listmech " << mechanisms << " " << mechanism_len << " " << mechanism_count << endl;


}

//************************************************************************
RTComTask::~RTComTask()
//************************************************************************
{
    sasl_dispose(&sasl_connection);
    delete[] sasl_callbacks;
    cerr << "Deleting RTComTask" << this << endl;
    close(fd);
}

//************************************************************************
int RTComTask::sasl_getopt(void *context, const char *plugin_name,
                const char *option, const char **result, unsigned *len)
//************************************************************************
{
    RTComTask* instance = reinterpret_cast<RTComTask*>(context);
    string key;

    if (plugin_name)
        key.append(plugin_name).append("_");

    key.append(option);

    cerr << "Requested " << " " << key << " " << result << " " << len << endl;

    instance->sasl_options.push_front(
            configFile->getString("sasl", key, ""));
    list<string>::iterator i = instance->sasl_options.begin();

    if (i->length()) {
        *result = i->c_str();
        cerr << "RTCom::Returned configuration " << *i << endl;

        if (len)
            *len = i->length();
        return SASL_OK;
    }
    else {
        *result = NULL;
        return SASL_FAIL;
    }
}

//************************************************************************
int RTComTask::read(int)
//************************************************************************
{
    int n;

    n = ::read(fd, inBuf + inBufPos, sizeof(inBuf) - inBufPos - 1);

    if (n <= 0) {
        return n;
    }

    inBufPos += n;
    inBuf[inBufPos] = '\0';

    switch (parserState) {
        case Idle:
        cerr << " parsing: " << inBuf << endl;
            if (login.FullMatch(inBuf)) {
                os.stdOut(string(mechanisms));
                parserState = LoginInit;
            }
            else if (capabilities.FullMatch(inBuf)) {
                os.stdOut(string("CAPABILITIES: STARTTLS LOGIN"));
            }
            else if (empty.FullMatch(inBuf)) {
                cerr << "Found empty strin " << endl;
            }
            else if (strchr(inBuf, '\n') || inBufPos >= sizeof(inBuf)-1) {
                // Discard everyting in input buffer if an Enter is detected
                // or input buffer is full but no command is matched
                os.stdErr("Unknown command.");
            }
            else {
                // Command not completed; Skip setting inBufPos to 0
                break;
            }
            inBufPos = 0;
            break;
        case LoginInit:
            {
                string client_mech;
                string rest;

                if (auth.PartialMatch(inBuf, &client_mech, &rest)) {
                    const char* reply;
                    unsigned int reply_len;
                    int retval;

                    retval = sasl_server_start(sasl_connection,
                            client_mech.c_str(), 
                            rest.length() ? rest.c_str() : NULL, 
                            rest.length(), 
                            &reply, &reply_len);

                    switch (retval) {
                        case SASL_CONTINUE:
                            cerr << "SASL_CONTINUE" << endl;
                            parserState = LoginContinue;
                            break;
                        case SASL_OK:
                            cerr << "SASL_OK" << endl;
                            parserState = Idle;
                            loggedIn = true;
                            break;
                        default:
                            os.stdErr("Login failure. Reconnect.");
                            parserState = LoginFail;
                            inBufPos = 0;
                            break;
                    }
                }
                else {
                    os.stdErr("Did not find AUTH command.");
                    parserState = Idle;
                    inBufPos = 0;
                }
                break;
            }
        case LoginContinue:
            {
                const char* p = inBuf;

                const char* dataStart;

                while ((dataStart = strchr(p,'}'))) {
                        int len;

                        if (!dataStart)
                        break;

                        // Point to location after right brace
                        dataStart++;

                        if (length.PartialMatch(p, &len)) {
                            int retval;
                            const char* reply;
                            unsigned int reply_len;

                            if (inBufPos - (dataStart - p) < len) {
                                inBufPos -= p - inBuf;
                                memmove(inBuf, p, inBufPos);
                                break;
                            }

                            string s(dataStart, len);

                            retval = sasl_server_step(sasl_connection,
                                    s.c_str(), s.length(),
                                    &reply, &reply_len);
                            if (reply_len)
                                os.stdOut(string(reply,reply_len));
                            break;
                        }
                        else {
                        }
                }
                break;
            }
        case LoginFail:
            return -EPERM;
            break;
    }
                /*
                if (!memcmp(inBuf, "SET ", min(4U, inBufPos))) {
                    parserState = Write;
                }
                else if (!memcmp(inBuf, "SUBSCRIBE ", min(10U, inBufPos))) {
                    parserState = Subscribe;
                }
                else if (!memcmp(inBuf, "POLL ", min(5U, inBufPos))) {
                    parserState = Poll;
                }
                else if (!memcmp(inBuf, "USER ", min(5U, inBufPos))) {
                    parserState = SetUser;
                }
                else if (!memcmp(inBuf, "PASS ", min(5U, inBufPos))) {
                    parserState = SetPass;
                }
                else if (!memcmp(inBuf, "LIST", min(4U, inBufPos))) {
                    parserState = List;
                }
                else if (*inBuf == '\n') {
                    consumed = 1;
                }
                else {
                    consumed = inBufPos;
                    os.stdErr("Unknown command.");
                }
                break;
                */

    return n;
}

//************************************************************************
bool RTComTask::checkPass(const string& user, const string& password)
//************************************************************************
{
    return true;
}

//************************************************************************
void RTComTask::kill(Task*, int)
//************************************************************************
{
    throw SocketExcept("Internal Error: Cannot kill output stream.");
}
