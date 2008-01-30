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
#include "ConfigFile.h"
#include "Dispatcher.h"

#include <iostream>

using namespace std;

//************************************************************************
Task::Task(Task* parent): 
    parent(parent),
    dispatcher(parent 
            ? parent->getDispatcher() 
            : new Dispatcher())
//************************************************************************
{
    if (parent)
        parent->adopt(this);

    cerr << "Born task " << this << endl;

    readRef = writeRef = NULL;
}

//************************************************************************
Task::~Task()
//************************************************************************
{
    for (list<Task*>::iterator it = children.begin(); 
            it != children.end(); it++)
        delete *it;

    dispatcher->remove(readRef);
    dispatcher->remove(writeRef);
}

//************************************************************************
Task* Task::getParent() const 
//************************************************************************
{ 
    cerr << "returning " << parent << endl;
    return parent; 
}

//************************************************************************
Dispatcher* Task::getDispatcher() const
//************************************************************************
{
    return dispatcher;
}

//************************************************************************
void Task::kill(Task* child, int rv)
//************************************************************************
{ 
    cerr << "killing " << child << endl;
    children.remove(child);
    delete child;
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
void Task::enableRead(int fd)
//************************************************************************
{
    readRef = dispatcher->setReadable(this, fd);
}

//************************************************************************
void Task::enableWrite(int fd)
//************************************************************************
{
    writeRef = dispatcher->setWriteable(this, fd);
}
