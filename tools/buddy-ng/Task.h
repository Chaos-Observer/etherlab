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

#ifndef TASK_H
#define TASK_H

#include "Dispatcher.h"

#include <list>

class Task {
    friend class Dispatcher;

    public:
        Task(Task* parent);
        virtual ~Task();

        Task* getParent() const;
        virtual void kill(Task* child, int rv);

    protected:
        virtual int read(int fd);
        virtual int write(int fd);

        Dispatcher* getDispatcher() const;

        void enableRead(int fd);
        void enableWrite(int fd);
        void disableRead();
        void disableWrite();

    private:
        Task * const parent;
        Dispatcher * const dispatcher;

        std::list<Task*> children;
        void adopt(Task* child);
        void release(Task* child);

        void* readRef;
        void* writeRef;
};

#endif // TASK_H
