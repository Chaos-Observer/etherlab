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

#include "RTComIOTask.h"
#include "SocketExcept.h"
#include "ConfigFile.h"
#include "RT-Task.h"
#include "RT-Model.h"
#include "RTVariable.h"

#undef DEBUG
#define DEBUG 1

#include <iostream>
#include <arpa/inet.h>
//#include <sstream>
//#include <iterator>
//#include <string>
//#include <cstring>
//#include <cerrno>

using namespace std;

//************************************************************************
RTComIOTask::RTComIOTask(Task* parent, RTTask* _rtTask, int _fd, 
        unsigned int _buflen, unsigned int _maxbuf): 
    Task(parent), rtTask(_rtTask), streambuf(), fd(_fd), buflen(_buflen), 
    max_buffers(_maxbuf)
//************************************************************************
{
//    sasl_callback_t callbacks[] = {
//        {SASL_CB_GETOPT, (int (*)())sasl_getopt, this},
//        {SASL_CB_LIST_END, NULL, NULL},
//    };
//    sasl_callbacks = 
//        new sasl_callback_t[sizeof(callbacks)/sizeof(sasl_callback_t)];
//    memcpy(sasl_callbacks, callbacks, sizeof(callbacks));
//
//    if (SASL_OK != sasl_server_new( "login",
//                NULL, /* my fully qualified domain name; 
//                         NULL says use gethostname() */
//                NULL, /* The user realm used for password
//                         lookups; NULL means default to serverFQDN
//                         Note: This does not affect Kerberos */
//                NULL, NULL, /* IP Address information strings */
//                sasl_callbacks, /* Callbacks supported only for this 
//                                   connection */
//                0, /* security flags (security layers are enabled
//                    * using security properties, separately)
//                    */
//                &sasl_connection)) {
//        throw SocketExcept("Could not initiate a new SASL session.");
//    }
//
//
//    if (SASL_OK != sasl_listmech(
//                sasl_connection,  // sasl_conn_t *conn,
//                NULL,             // const char *user,
//                "",              // const char *prefix,
//                " ",              // const char *sep,
//                "",              // const char *suffix,
//                &mechanisms,      // const char **result,
//                &mechanism_len,   // unsigned *plen,
//                &mechanism_count  // unsigned *pcount
//                )) {
//        throw SocketExcept("Could not get SASL mechnism list.");
//    }
//
//    cerr << "sasl_listmech " << mechanisms << " " << mechanism_len << " " << mechanism_count << endl;
//
}

//************************************************************************
RTComIOTask::~RTComIOTask()
//************************************************************************
{
//    sasl_dispose(&sasl_connection);
//    delete[] sasl_callbacks;
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
            ConfigFile::getString("sasl", key, ""));
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

//                    retval = sasl_server_start(sasl_connection,
//                            client_mech.c_str(), 
//                            rest.length() ? rest.c_str() : NULL, 
//                            rest.length(), 
//                            &reply, &reply_len);
//
//                            retval = sasl_server_step(sasl_connection,
//                                    s.c_str(), s.length(),
//                                    &reply, &reply_len);
