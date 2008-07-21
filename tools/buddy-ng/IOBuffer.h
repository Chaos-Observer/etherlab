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

#ifndef IOBUFFER_H
#define IOBUFFER_H

#include <string>
#include <list>

namespace LayerStack {

class Layer;

class IOBuffer: public std::string {
    public:
        IOBuffer(Layer* owner);
        IOBuffer(Layer* owner, const std::string& str );
        IOBuffer(Layer* owner, const std::string& str, 
                size_t pos, size_t n = npos );
        IOBuffer(Layer* owner, const char * s, size_t n );
        IOBuffer(Layer* owner, const char * s );
        IOBuffer(Layer* owner, size_t n, char c );

        ~IOBuffer();

        Layer* getOwner() const;

        /** Transmit the buffer down the layer stack.
         *
         * Return: true = buffer was successfully transmitted and
         *                      can be reused or deleted
         *         false = buffer must be stored. It will be deleted
         *                      later when layer->finished(buf) is called
         */
        bool transmit();

        void reset();

        IOBuffer& appendInt(uint32_t);

//        size_t receive();

    private:
        Layer* const owner;

        typedef struct {
            Layer* layer;
            ssize_t headerLen;
        } LayerDef;

        typedef std::list<LayerDef> LayerList;
        LayerList layerList;

        size_t headerLen;

        void init();
};

} // namespace LayerStack

#endif // IOBUFFER_H

