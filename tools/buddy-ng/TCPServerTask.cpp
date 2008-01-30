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

#include "TCPServerTask.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <cstring>
#include <cerrno>

//#include <iostream>
 
using namespace std;

//************************************************************************
TCPServerTask::TCPServerTask(Task* parent): SocketServerTask(parent)
//************************************************************************
{
    fd = -1;
}

//************************************************************************
TCPServerTask::~TCPServerTask()
//************************************************************************
{
    close();
}

//************************************************************************
void TCPServerTask::close()
//************************************************************************
{
    if (fd >= 0) {
        ::close(fd);
        fd = -1;
    }
}

//************************************************************************
void TCPServerTask::open(const char* s_addr, int port) throw(SocketExcept)
//************************************************************************
{
    string msg;
    struct hostent *hostent;

    if (!(hostent = ::gethostbyname(s_addr))) {
        msg = "Could not resolve hostname: ";
        switch (h_errno) {
            case HOST_NOT_FOUND:
                msg.append("The specified host is unknown.");
                break;
            case NO_ADDRESS:
                msg.append("The requested name is valid "
                        "but does not have an IP address.");
                break;
            case NO_RECOVERY:
                msg.append("A non-recoverable name server error occurred.");
                break;
            case TRY_AGAIN:
                msg.append("A temporary error occurred "
                        "on an authoritative name server.  Try again later.");
                break;
            default:
                msg.append("An unexpected error occurred.");
                break;
        }
        goto tcp_out1;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    ::memcpy((char *)&addr.sin_addr, hostent->h_addr,
            sizeof(struct in_addr));

    if (0 > (fd = socket(PF_INET, SOCK_STREAM, 0))) {
        msg = "Error creating tcp socket: ";
        goto tcp_out2;
    }

    sockopt = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, 
                &sockopt, sizeof(sockopt))) {
        msg = "Error setting socket options: ";
        goto tcp_out3;
    }

    // Bind and listen to port. Part of SocketServerTask
    bindAndListen((struct sockaddr *)&addr, sizeof(addr));

    return;

tcp_out3:
    close();
tcp_out2:
tcp_out1:
    msg.append(strerror(errno));
    throw msg;
}
