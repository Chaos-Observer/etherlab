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

#include "SocketServerTask.h"
#include "ConfigFile.h"

#include <sstream>

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <cstring>
#include <cerrno>


#include <iostream>

using namespace std;


//************************************************************************
SocketServerTask::SocketServerTask(Task* parent): 
    Task(parent)
//************************************************************************
{
    sasl_callback_t callbacks[] = {
        {SASL_CB_GETOPT, (int (*)())sasl_getopt, this},
        {SASL_CB_LIST_END, NULL, NULL},
    };
    sasl_callbacks = 
        new sasl_callback_t[sizeof(callbacks)/sizeof(sasl_callback_t)];
    memcpy(sasl_callbacks, callbacks, sizeof(callbacks));

    if (SASL_OK != sasl_server_init(sasl_callbacks, "RTCom")) {
        throw SocketExcept("Could not initialise SASL library");
    }

}

//************************************************************************
SocketServerTask::~SocketServerTask()
//************************************************************************
{
    sasl_done();
    delete[] sasl_callbacks;
}

//************************************************************************
int SocketServerTask::sasl_getopt(void *context, const char *plugin_name,
                const char *option, const char **result, unsigned *len)
//************************************************************************
{
    SocketServerTask* instance = reinterpret_cast<SocketServerTask*>(context);
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
        cerr << "Returned configuration " << *i << endl;

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
void SocketServerTask::setNonblock(int fd) throw(SocketExcept)
//************************************************************************
{
    string msg;
    long flags;

    if ((flags = fcntl(fd, F_GETFL)) == -1) {
        msg = "Error getting socket flags: ";
        goto nonblock_out;
    }

    flags |= O_NONBLOCK;

    if (fcntl(fd, F_SETFL, flags)) {
        msg = "Error setting socket flags: ";
        goto nonblock_out;
    }

    return;

nonblock_out:
    msg.append(strerror(errno));
    throw SocketExcept(msg);
}

//************************************************************************
void SocketServerTask::bindAndListen(struct sockaddr *addr, socklen_t sun_len)
    throw(SocketExcept)
//************************************************************************
{
    string msg;

    if (bind(fd, addr, sun_len)) {
        msg = "Error binding to socket: ";
        goto bind_out;
    }

    if (listen(fd, 5)) {
        msg = "Error listening to socket: ";
        goto bind_out;
    }

    setNonblock(fd);

    return;

bind_out:
    msg.append(strerror(errno));
    throw SocketExcept(msg);
}

//************************************************************************
int SocketServerTask::accept(struct sockaddr *addr, socklen_t size)
    throw(SocketExcept)
//************************************************************************
{
    int new_fd;
    string msg;

    if ((new_fd = ::accept(fd, addr, &size)) < 0) {
        msg = "Error accepting socket flags: ";
        goto accept_out1;
    }

    if (!size) {
        msg = "Unexpected size error while calling accept(): ";
        goto accept_out2;
    }

    setNonblock(new_fd);

    return new_fd;

accept_out2:
    close(fd);
accept_out1:
    msg.append(strerror(errno));
    throw SocketExcept(msg);
}
