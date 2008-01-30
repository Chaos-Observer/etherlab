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

#ifndef SOCKETSERVERTASK_H
#define SOCKETSERVERTASK_H

#include "Task.h"
#include "SocketExcept.h"

#include <sys/socket.h>

class SocketServerTask: public Task {
    public:
        SocketServerTask(Task* parent);
        ~SocketServerTask();

    protected:
        int fd;

        void bindAndListen(struct sockaddr *addr, socklen_t sun_len)
            throw(SocketExcept);

        int accept(struct sockaddr *addr, size_t size)
            throw(SocketExcept);

    private:
        void setNonblock(int fd) throw(SocketExcept);
};

#endif // SOCKETSERVERTASK_H

