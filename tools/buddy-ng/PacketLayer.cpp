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
#include "IOBuffer.h"

#include <arpa/inet.h>

namespace LS = LayerStack;

////////////////////////////////////////////////////////////////////////
LS::PacketLayer::PacketLayer(Layer* below):
    Layer(below,"PacketLayer\n",sizeof(uint32_t))
{
    setRecvTerminal(true);
}

////////////////////////////////////////////////////////////////////////
LS::PacketLayer::~PacketLayer()
{
}

////////////////////////////////////////////////////////////////////////
// Method from class Layer
std::string LS::PacketLayer::getHeader(const char*, size_t len) const
{
    uint32_t packetLen = htonl(len);
    std::cerr << "Packet layer here; sending header " 
        << packetLen << " " << len << std::endl;
    return std::string((char*)&packetLen, sizeof(packetLen));
}

////////////////////////////////////////////////////////////////////////
// Method from class Layer
size_t LS::PacketLayer::receive(const char* buf, size_t data_len)
{
    size_t processed = 0;

    while (data_len - processed >= sizeof(packetLen)) {

        packetLen = ntohl(*(uint32_t*)(buf + processed));
        processed += sizeof(packetLen);

        std::cerr << "Expecting " << packetLen 
            << " got " << data_len - processed << std::endl;

        if (data_len - processed < packetLen) {
            processed -= sizeof(packetLen);
            break;
        }
        std::cerr << "got it " << packetLen << std::endl;

        if (packetLen != post(buf + processed, packetLen)) {
            std::cerr << "did not process everything" << std::endl;
        }

        processed += packetLen;
    }

    return processed;
}
