/* **********************************************************************
 *  $Id$
 * **********************************************************************/

/*
 * Copyright (C) 2008 Richard Hacker
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

#include "Dispatcher.h"
#include "Task.h"

#undef DEBUG
#define DEBUG 0

#if DEBUG
#include <iostream>
using namespace std;
#endif

Dispatcher dispatcher;

//************************************************************************
Dispatcher::Dispatcher()
//************************************************************************
{
    event_init();
}

//************************************************************************
Dispatcher::~Dispatcher()
//************************************************************************
{
    for (std::list<Event*>::iterator it = events.begin();
            it != events.end(); it++)
        event_del(&(*it)->ev);
}

//************************************************************************
int Dispatcher::run()
//************************************************************************
{
    event_dispatch();
    return 0;
}

//************************************************************************
void Dispatcher::detach()
//************************************************************************
{
}

//************************************************************************
void* Dispatcher::setWriteable(Task* task, int fd)
//************************************************************************
{
    Event* event = new Event;
#if DEBUG
    std::cerr << ">>>>> write event " << event << std::endl;
#endif

    event->task = task;

    dispatcher.events.push_front(event);
    event_set(&event->ev, fd, EV_WRITE | EV_PERSIST, writeCallbackFunc, task);
    event_add(&event->ev, NULL);
    return event;
}

//************************************************************************
void* Dispatcher::setReadable(Task *task, int fd)
//************************************************************************
{
    Event* event = new Event;
#if DEBUG
    std::cerr << ">>>>> read event " << event << std::endl;
#endif

    event->task = task;

    dispatcher.events.push_front(event);
    event_set(&event->ev, fd, EV_READ | EV_PERSIST, readCallbackFunc, task);
    event_add(&event->ev, NULL);
    return event;
}

//************************************************************************
void* Dispatcher::setTimer(Task *task, struct timeval& tv)
//************************************************************************
{
    Event* event = new Event;
#if DEBUG
    std::cerr << ">>>>> timer event " << event << std::endl;
#endif

    event->tv = tv;
    event->task = task;

    dispatcher.events.push_front(event);
    evtimer_set(&event->ev, timerCallbackFunc, event);
    event_add(&event->ev, &event->tv);
    return event;
}

//************************************************************************
void Dispatcher::remove(void* p, Task* task)
//************************************************************************
{
    Event* event = reinterpret_cast<Event*>(p);
    if (!p)
        return;

    // Check the task pointer. If this is null, do not remove the Event
    // object because it is still required. See timerCallbackFunc()
    if (!event->task) {
        event->task = task;
        return;
    }

    event_del(&event->ev);
    dispatcher.events.remove(event);
    delete event;
#if DEBUG
    std::cerr << "<<<<< delete event " << event << std::endl;
#endif
}

//************************************************************************
void Dispatcher::readCallbackFunc(int fd, short event, void *priv_data)
//************************************************************************
{
    Task* task = reinterpret_cast<Task*>(priv_data);
    int rv;

    rv = task->read(fd);
#if DEBUG
    cerr << "Returnval from read: " << rv << endl;
#endif
    if (rv  <= 0) {
        Task* parent = task->getParent();
#if DEBUG
        cerr << "dispatcher read callling kill " << parent << endl;
#endif
        if (parent)
            parent->kill(task, rv);
        else
            task->disableRead();
    }
}

//************************************************************************
void Dispatcher::writeCallbackFunc(int fd, short event, void *priv_data)
//************************************************************************
{
    Task* task = reinterpret_cast<Task*>(priv_data);
    int rv;

    rv = task->write(fd);
#if DEBUG
    cerr << "Returnval from write: " << rv << endl;
#endif
    if (rv <= 0) {
        Task* parent = task->getParent();
#if DEBUG
        cerr << "dispatcher write callling kill " << parent << endl;
#endif
        if (parent)
            parent->kill(task, rv);
        else
            task->disableWrite();
    }
}

//************************************************************************
void Dispatcher::timerCallbackFunc(int fd, short _event, void *priv_data)
//************************************************************************
{
    Event* event = reinterpret_cast<Event*>(priv_data);
    Task* task = event->task;
    int rv;

    // Setting event->task to NULL is a signal to remove() that, if
    // the application called remove() during this thread of execution,
    // the object is not really removed. This is important since it is
    // required further down.
    event->task = 0;

    rv = task->timeout();
#if DEBUG
    cerr << "Returnval from timeout: " << rv << endl;
#endif
    if (rv <= 0) {
        Task* parent = task->getParent();
#if DEBUG
        cerr << "dispatcher timer callling kill " << parent << endl;
#endif
        if (parent)
            parent->kill(task, rv);
        else 
            task->disableTimer();
    }

    if (event->task) {
        // If event->task is set here, the application called
        // remove() during this thread. The event was not really removed,
        // so do this now
        remove(event, task);
    }
    else {
        // Place it on the timer queue again.
        event->task = task;
        event_add(&event->ev, &event->tv);
    }
}
