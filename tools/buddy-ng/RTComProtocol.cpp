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

#include "RTComProtocol.h"
#include "SocketLayer.h"
#include "PacketLayer.h"

#undef DEBUG
//#define DEBUG 1


using namespace std;

//************************************************************************
RTComProtocol::RTComProtocol(Task* parent, RTTask *_rtTask, int _fd, 
        unsigned int _buflen, unsigned int _maxbuf): 
    rtTask(_rtTask)
//************************************************************************
{
    layers.push(new SocketLayer(parent, _fd));
    layers.push(new PacketLayer(layers.top()));
}

//************************************************************************
RTComProtocol::~RTComProtocol()
//************************************************************************
{
}
