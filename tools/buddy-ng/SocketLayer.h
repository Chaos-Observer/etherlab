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

#ifndef SOCKETLAYER_H
#define SOCKETLAYER_H

#include "Task.h"
#include "Layer.h"

#include <queue>

class SocketLayer: public Task, public Layer {
    public:
        SocketLayer(Task *parent, int fd);
        ~SocketLayer();

    private:
        const int fd;

        const char* wptr;
        size_t bufLen;

        typedef std::queue<const IOBuffer*> BufferQ;
        BufferQ sendq;
        IOBuffer inBuf;

        /* Method implemented from Task */
        int read(int fd);
        int write(int fd);

        /* Methods implemented from Layer */
        bool send(const IOBuffer*);
};

#endif // SOCKETLAYER_H

