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

#include <iostream>
using namespace std;

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
    for (list<Event*>::iterator it = events.begin();
            it != events.end(); it++)
        event_del(*it);
}

//************************************************************************
int Dispatcher::run()
//************************************************************************
{
    event_dispatch();
    return 0;
}

//************************************************************************
int Dispatcher::run_detached()
//************************************************************************
{
    dispatcher.detach();
    run();
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
    std::cerr << __func__ << " 0" << std::endl;
    Event* event = new Event;
    std::cerr << ">>>>> new event " << event << std::endl;

    std::cerr << __func__ << " 1" << std::endl;
    dispatcher.events.push_front(event);
    std::cerr << __func__ << " 2" << std::endl;
    event_set(event, fd, EV_WRITE | EV_PERSIST, eventCallbackFunc,
            task);
    std::cerr << __func__ << " 3" << std::endl;
    event_add(event, NULL);
    std::cerr << __func__ << " 4" << std::endl;
    return event;
}

//************************************************************************
void* Dispatcher::setReadable(Task *task, int fd)
//************************************************************************
{
    Event* event = new Event;
    std::cerr << ">>>>> new event " << event << std::endl;

    dispatcher.events.push_front(event);
    event_set(event, fd, EV_READ | EV_PERSIST, eventCallbackFunc,
            task);
    event_add(event, NULL);
    return event;
}

//************************************************************************
void Dispatcher::remove(void* p)
//************************************************************************
{
    Event* event = reinterpret_cast<Event*>(p);
    if (!p)
        return;
    event_del(event);
    dispatcher.events.remove(event);
    delete event;
    std::cerr << "<<<<< delete event " << event << std::endl;
}

//************************************************************************
void Dispatcher::eventCallbackFunc(int fd, short event, void *priv_data)
//************************************************************************
{
    Task* task = reinterpret_cast<Task*>(priv_data);
    int rv;

    if (event & EV_READ) {
        rv = task->read(fd);
        cerr << "Returnval from read: " << rv << endl;
        if (rv  <= 0) {
            Task* parent = task->getParent();
            cerr << "dispatcher read callling kill " << parent << endl;
            if (parent)
                parent->kill(task, rv);
        }
    }

    if (event & EV_WRITE) {
        rv = task->write(fd);
        cerr << "Returnval from write: " << rv << endl;
        if (rv <= 0) {
            Task* parent = task->getParent();
            cerr << "dispatcher write callling kill " << parent << endl;
            if (parent)
                parent->kill(task, rv);
        }
    }
}
