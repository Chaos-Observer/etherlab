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

#include "RTComServer.h"
#include "RTComTask.h"
#include "ConfigFile.h"

#include <iostream>

using namespace std;

//************************************************************************
RTComServer::RTComServer(Task* parent): TCPServerTask(parent)
//************************************************************************
{
    port = ConfigFile::getInt("general", "port", 2500);
    s_addr = ConfigFile::getString("general", "interface", "0.0.0.0");

    // Open a TCP port
    open(s_addr.c_str(), port);
    enableRead(fd);
}

//************************************************************************
RTComServer::~RTComServer()
//************************************************************************
{
}

//************************************************************************
int RTComServer::read(int)
//************************************************************************
{
    struct sockaddr_in in_addr;
    socklen_t size = sizeof(in_addr);
    int new_fd;

    new_fd = SocketServerTask::accept((struct sockaddr*)&in_addr, size);
    if (new_fd >= 0)
        new RTComTask(this, new_fd);

    cerr << "New file descriptor" << new_fd << endl;

    return new_fd;
}
