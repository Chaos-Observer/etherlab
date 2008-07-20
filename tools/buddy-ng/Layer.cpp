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

namespace LS = LayerStack;

/*****************************************************************/
LS::Layer::Layer(Layer* _below, const std::string& _name, size_t _headerLen): 
    name(_name), headerLen(_headerLen), above(0)
{
    std::cerr << "XX New layer " << name << std::endl;
    sendTerminal = recvTerminal = false;
    setLayerBelow(_below);
}

/*****************************************************************/
LS::Layer::~Layer()
{
    setLayerBelow(0);
    delete above;
    std::cerr << "XX Delete layer " << name << std::endl;
}

/*****************************************************************/
void LS::Layer::setLayerBelow(Layer* _below)
{
    if (below)
        below->introduce(0);

    below = _below;

    if (below) {
        below->introduce(this);
    }
}

/*****************************************************************/
const std::string& LS::Layer::getName() const
{
    return name;
}

///*****************************************************************/
//void LS::Layer::sendName()
//{
//    nameBuf = new LS::IOBuffer(this, name);
//
//    if (nameBuf->transmit()) {
//        finished(nameBuf);
//    }
//}

/*****************************************************************/
std::string LS::Layer::getHeader(const char*, size_t) const
{
    return std::string();
}

/*****************************************************************/
size_t LS::Layer::getHeaderLength() const
{
    return headerLen;
}

/*****************************************************************/
void LS::Layer::introduce(Layer* _above)
{
    above = _above;
}

/*****************************************************************/
void LS::Layer::setSendTerminal(bool terminate)
{
   sendTerminal = terminate;
}

/*****************************************************************/
void LS::Layer::setRecvTerminal(bool terminate)
{
   recvTerminal = terminate;
}

/*****************************************************************/
bool LS::Layer::isRecvTerminal() const
{
   return recvTerminal;
}

/*****************************************************************/
LS::Layer* LS::Layer::getNext() const
{
    return sendTerminal ? 0 : below;
}

// /*****************************************************************/
// LS::Layer* LS::Layer::getPrev() const
// {
//     return recvTerminal ? 0 : above;
// }
// 
/*****************************************************************/
void LS::Layer::finished(const LS::IOBuffer* buf)
{
}

/*****************************************************************/
bool LS::Layer::send(const LS::IOBuffer *)
{
    /* By default, tell the client that the data was sent */
    return true;
}

/*****************************************************************/
size_t LS::Layer::post(const char* buf, size_t data_len) const
{
    return above ? above->receive(buf, data_len) : data_len;
}

/*****************************************************************/
size_t LS::Layer::post(const std::string *buf, size_t offset) const
{
    return above 
        ? above->receive(buf->c_str() + offset, buf->length() - offset) 
        : buf->length() - offset;
}

/*****************************************************************/
size_t LS::Layer::receive(const char* buf, size_t data_len)
{
    return post(buf, data_len);
}
