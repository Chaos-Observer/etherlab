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

#include "IOBuffer.h"
#include "Layer.h"

#include <iostream>

IOBuffer::IOBuffer(Layer* _owner):
    std::string(), owner(_owner)
{
    init();
}

IOBuffer::IOBuffer(Layer* _owner, const std::string& str ): 
    std::string(), owner(_owner)
{
    init();
    append(str);
}

IOBuffer::IOBuffer(Layer* _owner, const std::string& str, 
        size_t pos, size_t n):
    std::string(), owner(_owner)
{
    init();
    append(str,pos,n);
}

IOBuffer::IOBuffer(Layer* _owner, const char * s, size_t n ):
    std::string(), owner(_owner)
{
    init();
    append(s,n);
}

IOBuffer::IOBuffer(Layer* _owner, const char * s ):
    std::string(), owner(_owner)
{
    init();
    append(s);
}

IOBuffer::IOBuffer(Layer* _owner, size_t n, char c ):
    std::string(), owner(_owner)
{
    init();
    append(n,c);
}

void IOBuffer::init()
{
    headerLen = 0;

    for( Layer* layer = owner; layer; layer = layer->getNext()) {
        LayerDef l;
        l.layer = layer;
        l.headerLen = layer->getHeaderLength();

        layerList.push_back(l);

        headerLen += l.headerLen;
    }
    headerLen -= owner->getHeaderLength();
    std::cerr << "Setting headerlength " << headerLen << std::endl;
    append(headerLen, '-');

}

IOBuffer::~IOBuffer()
{
}

size_t IOBuffer::receive()
{
    // go through the layerlist until someone returns true to 
    // say the the channel has ended
    size_t consumed = 0;

    for (Layer* layer = owner->getPrev();
            layer && consumed != length(); layer = layer->getPrev()) {
        consumed += layer->receive(c_str() + consumed, length() - consumed);
    }

    return consumed;
}

bool IOBuffer::transmit()
{
    size_t hptr = headerLen; /* pointer to header area of current layer */

    for (LayerList::iterator it = layerList.begin();
            ++it != layerList.end();) {
        size_t blen = length() - hptr;
        size_t bptr = hptr;
        hptr -= it->headerLen;

        replace(hptr, it->headerLen, 
                it->layer->getHeader(c_str() + bptr, blen), 0, it->headerLen);
    }

    std::cerr << "want to queue " << *this << std::endl;

    return layerList.back().layer->send(this);
}

Layer* IOBuffer::getOwner() const
{
    return owner;
}
