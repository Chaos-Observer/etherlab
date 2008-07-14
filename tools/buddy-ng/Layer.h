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

class Layer {
    public:
        Layer(size_t preamble, Layer* below = 0);
        virtual ~Layer();

        /** Called by the layer below to say that the buffer was sent */
        virtual void transmitted(const char* buf);

        /** Called by the layer above to say that the buffer was accepted */
        virtual void received(const char* buf);

    protected:
        virtual void pass_down(Layer* client, const char* buf, size_t len);
        virtual void push_up(Layer* caller, const char* buf, size_t len);

        /** Return how many bytes preamble is necessary for all
         * layers below */
        size_t getPreamble() const;

    private:
        const size_t preamble;

        Layer* const below;
        Layer* above;

        /** Layers placed above must introduce themselves to the 
         * layer below */
        void introduce(Layer* above);
};

#endif // LAYER_H

