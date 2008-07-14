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

class SocketLayer: public Task, public Layer {
    public:
        SocketLayer(Task *parent);
        ~SocketLayer();

    protected:
        int write(int fd);

    private:
        int fd;

        int read(int fd);

        const char* buf;
        const char* wptr;
        size_t bufLen;
        Layer* client;

        void pass_down(Layer*, const char*, size_t);
        void received(const char* buf);
};

#endif // SOCKETLAYER_H

