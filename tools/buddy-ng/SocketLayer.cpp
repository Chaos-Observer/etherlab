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
#include "IOBuffer.h"

////////////////////////////////////////////////////////////////////////
SocketLayer::SocketLayer(Task *_parent, int _fd): 
    Task(_parent), Layer(0, "RTCom", 0), 
    fd(_fd), bufLen(0)
{
    LayerStack::IOBuffer 
        *name = new LayerStack::IOBuffer(this, "RTCom\0", 6);
    name->transmit();
    enableRead(fd);
    setSendTerminal(true);
}

////////////////////////////////////////////////////////////////////////
SocketLayer::~SocketLayer()
{
    std::cerr << "Kill SocketLayer 2" << std::endl;
    while(sendq.size()) {
        sendq.front()->getOwner()->finished(sendq.front());
        sendq.pop();
    }
    close(fd);
}

////////////////////////////////////////////////////////////////////////
// Method from class Layer
void SocketLayer::finished(const LayerStack::IOBuffer* buf)
{
    delete buf;
}

////////////////////////////////////////////////////////////////////////
// Method from class Layer
bool SocketLayer::send(const LayerStack::IOBuffer* buf)
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
        const LayerStack::IOBuffer* buf = sendq.front();

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
    std::cerr << "inbuf length" << inBuf.length() << std::endl;
    inBuf.erase(0, post(&inBuf));
    std::cerr << "inbuf length" << inBuf.length() << std::endl;

    return len;
}
