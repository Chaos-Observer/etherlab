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

////////////////////////////////////////////////////////////////////////
SocketLayer::SocketLayer(Task *_parent, int _fd): 
    Task(_parent), Layer(0, "SocketLayer\n", 0), 
    fd(_fd), bufLen(0), inBuf(this)
{
    enableRead(fd);
    sendName();
}

////////////////////////////////////////////////////////////////////////
SocketLayer::~SocketLayer()
{
}

////////////////////////////////////////////////////////////////////////
// Method from class Layer
bool SocketLayer::send(const IOBuffer* buf)
{
    sendq.push(buf);

    std::cerr << "queuing " << *buf << std::endl;

    if (!bufLen) {
        bufLen = buf->length();
        wptr = buf->c_str();
        enableWrite(fd);
    }

    /* return false so that the owner stores the buffer */
    return false;
}

////////////////////////////////////////////////////////////////////////
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
        const IOBuffer* buf = sendq.front();

        buf->getOwner()->finished(buf);
        sendq.pop();

        if (sendq.empty()) {
            disableWrite();
        }
        else {
            buf = sendq.front();

            wptr = buf->c_str();
            bufLen = buf->length();
        }
    }

    return len;
}

////////////////////////////////////////////////////////////////////////
// Method from class Task
int SocketLayer::read(int fd)
{
    char data[4096];

    int len = ::read(fd, data, sizeof(data));

    if (len <= 0)
        return len;

    inBuf.append(data,len);
    size_t processed = inBuf.receive();

    inBuf.erase(0, processed);

    return len;
}
