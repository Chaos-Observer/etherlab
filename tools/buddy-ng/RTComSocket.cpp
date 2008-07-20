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

#include "RTComSocket.h"
#include "PacketLayer.h"
#include "DebugLayer.h"
#include "RTComProtocolServer.h"

#include <iostream>

#undef DEBUG
//#define DEBUG 1

//************************************************************************
RTComSocket::RTComSocket(Task* parent, RTTask *rtTask, int fd):
    SocketLayer(parent, fd)
//************************************************************************
{
    std::cerr << "XXXXXXXXXXXXX 2" << std::endl;
    Layer* layer;

    layer = new DebugLayer(this);
    layer = new LayerStack::PacketLayer(layer);
    layer = new LayerStack::Layer(layer, "dummy", 0);
    layer = new RTComProtocolServer(layer, rtTask);

}

//************************************************************************
RTComSocket::~RTComSocket()
//************************************************************************
{
    std::cerr << "Kill XXXXXXXXXXXXX 2" << std::endl;
}
