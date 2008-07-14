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
#include "PacketLayer.h"

#include <arpa/inet.h>

////////////////////////////////////////////////////////////////////////
PacketLayer::PacketLayer(Layer* below):
    Layer(below,"PacketLayer\n",sizeof(uint32_t))
{
    sendName();
}

////////////////////////////////////////////////////////////////////////
PacketLayer::~PacketLayer()
{
}

////////////////////////////////////////////////////////////////////////
// Method from class Layer
std::string PacketLayer::getHeader(const char* puf, size_t len) const
{
    uint32_t packetLen = htonl(len);
    return std::string((char*)&packetLen, sizeof(packetLen));
}

////////////////////////////////////////////////////////////////////////
// Method from class Layer
size_t PacketLayer::receive(const char* buf, size_t data_len)
{
    size_t consumed = 0;
    IOBuffer inbuf(this);

    while (data_len >= sizeof(packetLen)) {
        packetLen = ntohl(*(uint32_t*)buf);
        
        std::cerr << "Expecting " << packetLen << std::endl;

        if (data_len - consumed - sizeof(packetLen) < packetLen)
            break;

        consumed += sizeof(packetLen);

        inbuf.append(buf + consumed, packetLen);
        inbuf.receive();
        inbuf.clear();

        consumed += packetLen;
    }

    return consumed;
}
