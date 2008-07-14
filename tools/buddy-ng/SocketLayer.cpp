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

#include <iostream>
#include "SocketLayer.h"

SocketLayer::SocketLayer(Task *_parent): Task(_parent), Layer(0)
{
    enableRead(fd);
}

SocketLayer::~SocketLayer()
{
}

void SocketLayer::pass_down(Layer* _client, 
        const char* _buf, size_t _bufLen)
{
    wptr = buf = _buf;
    bufLen = _bufLen;
    client = _client;

    enableWrite(fd);
}

// Method from class Task
int SocketLayer::write(int fd)
{
    int len;
    
    len = ::write(fd, wptr, bufLen);

    if (len <= 0)
        return len;

    bufLen -= len;
    wptr += len;
    if (!bufLen) {
        client->transmitted(buf);
        disableWrite();
    }

    return len;
}

int SocketLayer::read(int fd)
{
    return 0;
}

void SocketLayer::received(const char* buf)
{
    delete[] buf;
}
