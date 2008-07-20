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

#include <iostream>

using namespace std;

//************************************************************************
Task::Task(Task* parent): parent(parent)
//************************************************************************
{
    if (parent) {
        parent->adopt(this);
    }

    cerr << "Born task " << this << endl;

    readRef = writeRef = NULL;
}

//************************************************************************
Task::~Task()
//************************************************************************
{
    cerr << "Deleting Task" << this << endl;

    disableRead();
    disableWrite();

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
    cerr << "returning " << parent << endl;
    return parent; 
}

//************************************************************************
void Task::kill(Task* child, int rv)
//************************************************************************
{ 
    cerr << "killing " << child << endl;
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
void Task::disableRead()
//************************************************************************
{
    Dispatcher::remove(readRef);
    readRef = NULL;
}

//************************************************************************
void Task::disableWrite()
//************************************************************************
{
    Dispatcher::remove(writeRef);
    writeRef = NULL;
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
