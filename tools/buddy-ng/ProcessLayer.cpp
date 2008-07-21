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
#include <arpa/inet.h>

#include "IOBuffer.h"
#include "ProcessLayer.h"

#include "RTComVocab.h"

namespace LS = LayerStack;

/*****************************************************************/
LS::ProcessLayer::ProcessLayer(Layer* _below): 
    Layer(_below, "ProcessLayer", 0), cmd(this)
{
    setRecvTerminal(true);
}

/*****************************************************************/
LS::ProcessLayer::~ProcessLayer()
{
}

/*****************************************************************/
void LS::ProcessLayer::finished(const LS::IOBuffer* buf)
{
    delete buf;
}

/*****************************************************************/
size_t LS::ProcessLayer::receive(const char* buf, size_t data_len)
{
    const char* buf_end = buf + data_len;
    uint32_t channel = ntohl(*(uint32_t*)buf);
    buf += sizeof(channel);

    if (channel == 0) {
        uint32_t command = ntohl(*(uint32_t*)(buf));
        buf += sizeof(channel);
        LayerStack::IOBuffer *cmd = new LayerStack::IOBuffer(this);

        switch (command) {
            case NEW_APPLICATION:
                std::cerr << "new application " 
                    << std::string(buf, buf_end - buf)
                    << std::endl;
                cmd->appendInt(GET_APPLICATION);
                cmd->append(std::string(buf, buf_end - buf));
                if (cmd->transmit())
                    delete cmd;
                break;
            default:
                break;
        }
    }

    return data_len;
}
