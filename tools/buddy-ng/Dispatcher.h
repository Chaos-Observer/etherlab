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

#ifndef DISPATCHER_H
#define DISPATCHER_H

#include <sys/types.h>
#include <sys/time.h>
#include <event.h>

#include <list>

class Task;

class Dispatcher {
    friend class Task;

    public:
        Dispatcher();
        ~Dispatcher();
        void addEvent(Task *t);

        int run();
        int run_detached();

        void* setReadable(Task*, int fd);
        void* setWriteable(Task*, int fd);
        void remove(void*);

    private:
        void detach();

        std::list<struct event*> events;

        static void eventCallbackFunc(int, short, void*);
};

#endif // DISPATCHER_H

