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

#ifndef LAYER_H
#define LAYER_H

#include <cstddef>

#include "IOBuffer.h"

class Layer {
    public:
        Layer(Layer* below, const std::string& name, size_t headerLen);
        virtual ~Layer();

        void sendName();

        /** Called by the layer below to say that the buffer was sent */
        virtual void finished(const IOBuffer*);

        virtual std::string getHeader(const char* puf, size_t len) const;
        size_t getHeaderLength() const;

        Layer* getNext() const;
        Layer* getPrev() const;

        /** Called by the layer above to say that the buffer was accepted */
        virtual bool send(const IOBuffer*);
        virtual size_t receive(const char* data_ptr, size_t data_len);

    private:
        Layer* const below;
        const std::string name;
        const size_t headerLen;

        IOBuffer *nameBuf;

        Layer* above;

        /** Layers placed above must introduce themselves to the 
         * layer below */
        void introduce(Layer* newLayer);
};

#endif // LAYER_H

