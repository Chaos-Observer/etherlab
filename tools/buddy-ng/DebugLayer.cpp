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
#include "DebugLayer.h"

////////////////////////////////////////////////////////////////////////
DebugLayer::DebugLayer(Layer* below):
    Layer(below,"DebugLayer\n",0)
{
}

////////////////////////////////////////////////////////////////////////
DebugLayer::~DebugLayer()
{
}

////////////////////////////////////////////////////////////////////////
// Method from class Layer
size_t DebugLayer::receive(const char* buf, size_t data_len)
{
    const char* ptr = buf;
    const char* ptr_end = buf + data_len;
    std::ios_base::fmtflags flags = std::cout.flags();
    unsigned int i = 0;

    std::cout.width(10);
    std::cout.flags(std::ios_base::right | std::ios_base::hex);
    std::cout << "Received:";
    for (ptr = buf; ptr_end != ptr; ptr++) {
        if (!(i++ & 0x0f)) 
            std::cout << std::endl;
        std::cout << " " << std::hex << (uint32_t)*ptr;
    }
    std::cout << std::endl;

    std::cout.flags(flags);

    post(buf,data_len);
    return 0;
}
