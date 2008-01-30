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

#include "RTComTask.h"
#include "ConfigFile.h"

#include <iostream>

using namespace std;

//************************************************************************
RTComTask::RTComTask(Task* parent, int _fd): Task(parent), fd(_fd)
//************************************************************************
{
    enableRead(fd);
}

//************************************************************************
RTComTask::~RTComTask()
//************************************************************************
{
}

//************************************************************************
int RTComTask::read(int)
//************************************************************************
{
    char buf[1024];
    int n;

    n = ::read(fd, buf, sizeof(buf));

    if (n > 0) {
        cerr << "received: " << string(buf,n) << endl;
    }

    return n;
}
