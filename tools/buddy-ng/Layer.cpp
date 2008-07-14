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

#include "IOBuffer.h"
#include "Layer.h"

/*****************************************************************/
Layer::Layer(Layer* _below, const std::string& _name, size_t _headerLen): 
    below(_below), name(_name), headerLen(_headerLen) 
{
    if (below)
        below->introduce(this);
}

/*****************************************************************/
Layer::~Layer()
{
    if (below)
        below->introduce(0);
}

/*****************************************************************/
void Layer::sendName()
{
    nameBuf = new IOBuffer(this, name);

    if (nameBuf->transmit()) {
        std::cerr << "XXXXXXXXXXXXXX" << *nameBuf << std::endl;
        finished(nameBuf);
    }
}

/*****************************************************************/
std::string Layer::getHeader(const char*, size_t) const
{
    return std::string();
}

/*****************************************************************/
size_t Layer::getHeaderLength() const
{
    return headerLen;
}

/*****************************************************************/
void Layer::introduce(Layer* _above)
{
    above = _above;
}

/*****************************************************************/
Layer* Layer::getNext() const
{
    return below;
}

/*****************************************************************/
Layer* Layer::getPrev() const
{
    return above;
}

/*****************************************************************/
void Layer::finished(const IOBuffer* buf)
{
    if (nameBuf == buf) {
        delete buf;
        nameBuf = 0;
    }
}

/*****************************************************************/
bool Layer::send(const IOBuffer *)
{
    /* By default, tell the client that the data was sent */
    return true;
}

/*****************************************************************/
size_t Layer::receive(const char*, size_t data_len)
{
    /* Consume all remaining data */
    return data_len;
}
