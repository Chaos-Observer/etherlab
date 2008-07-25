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
        Task(Task* parent = 0);
        virtual ~Task();

        Task* getParent() const;
        virtual void kill(Task* child, int rv);

    protected:
        /* Reimplement these methods to receive the calls.
         * Return:
         *    <= 0 : Error occurred. If the task has a parent, it will
         *           be called with a kill
         *    > 0  : No problem; continue
         */
        virtual int read(int fd);
        virtual int write(int fd);
        virtual int timeout();

        void enableRead(int fd);
        void enableWrite(int fd);
        void enableTimer(struct timeval&);

        void disableRead();
        void disableWrite();
        void disableTimer();

    private:
        Task * const parent;

        std::list<Task*> children;
        void adopt(Task* child);
        void release(Task* child);

        void* readRef;
        void* writeRef;
        void* timerRef;
};

#endif // TASK_H

