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

#include "Task.h"
#include "Dispatcher.h"

#undef DEBUG
#define DEBUG 0

//#include <iostream>

using namespace std;

//************************************************************************
Task::Task(Task* parent): parent(parent)
//************************************************************************
{
    if (parent) {
        parent->adopt(this);
    }

#if DEBUG
    cerr << "Born task " << this << endl;
#endif

    readRef = writeRef = timerRef = NULL;
}

//************************************************************************
Task::~Task()
//************************************************************************
{
#if DEBUG
    cerr << "Deleting Task" << this << endl;
#endif

    disableRead();
    disableWrite();
    disableTimer();

    list<Task*>::iterator it = children.begin(); 
    while (it != children.end()) {
        Task* child = *it++;
        delete child;
    }

    if (parent)
        parent->release(this);
}

//************************************************************************
Task* Task::getParent() const 
//************************************************************************
{ 
#if DEBUG
    cerr << "returning " << parent << endl;
#endif
    return parent; 
}

//************************************************************************
void Task::kill(Task* child, int rv)
//************************************************************************
{ 
#if DEBUG
    cerr << "killing " << child << endl;
#endif
    delete child;
}

//************************************************************************
void Task::release(Task* child)
//************************************************************************
{ 
    children.remove(child);
}

//************************************************************************
void Task::adopt(Task* child)
//************************************************************************
{ 
    children.push_back(child);
}

//************************************************************************
int Task::read(int)
//************************************************************************
{
    return -1;
}

//************************************************************************
int Task::write(int)
//************************************************************************
{
    return -1;
}

//************************************************************************
int Task::timeout()
//************************************************************************
{
    return -1;
}

//************************************************************************
void Task::disableRead()
//************************************************************************
{
    Dispatcher::remove(readRef, this);
    readRef = NULL;
}

//************************************************************************
void Task::disableWrite()
//************************************************************************
{
    Dispatcher::remove(writeRef, this);
    writeRef = NULL;
}

//************************************************************************
void Task::disableTimer()
//************************************************************************
{
    Dispatcher::remove(timerRef, this);
    timerRef = NULL;
}

//************************************************************************
void Task::enableRead(int fd)
//************************************************************************
{
    readRef = Dispatcher::setReadable(this, fd);
}

//************************************************************************
void Task::enableWrite(int fd)
//************************************************************************
{
    if (!writeRef)
        writeRef = Dispatcher::setWriteable(this, fd);
}

//************************************************************************
void Task::enableTimer(struct timeval& tv)
//************************************************************************
{
    if (!timerRef)
        timerRef = Dispatcher::setTimer(this, tv);
}
