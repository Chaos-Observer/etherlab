/* **********************************************************************
 *  $Id$
 * **********************************************************************/

/**
 * @ingroup buddy
 * @file
 * Main file implementing the dispatcher
 * @author Richard Hacker
 *
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


/** 
@defgroup buddy User Buddy Process

The @ref buddy is a process that runs in user space and forms the connection
between the real world and the Real-Time Process that runs in RTAI context. 

Essentially this is a general implementation around the @c select(2) function
call, dispatching external communication to various registered callback funtions 
when @c select(2) reports that read or write will not block on a file descriptor.
As @c select(2) is the central functionality, be sure that you fully understand 
it. Refer to the manpage if you have doubts.

This technique is a very advanced feature that does not need multitasking,
making it very efficient as it does not require task switches and interprocess
communication. Everything is contained within the context of one process.

During initialisation of the @ref buddy, various modules are initialised,
which in turn open files to perform their respective duties. During the 
initialisation of these modules, they register their functions that they wish
to be called back when the file descriptor is ready to @c read(2) or @c write(2).
For example the @ref netio module opens a tcp server socket and waits for 
incoming connections. When that happens, the file descriptor is ready to read.
The central dispatcher then calls the function that was registered when 
@c read(2) becomes ready.
*/

#include "ConfigFile.h"
#include "RTComServer.h"
#include "RT-Task.h"
#include "Dispatcher.h"

#include <iostream>

ConfigFile *configFile;

struct MainTask: public Task {
    MainTask(): Task(NULL) {
        new RTTask(this);
        new RTComServer(this);
        getDispatcher()->run();
    }
};

int main(int argc, const char *argv[])
{
    configFile = new ConfigFile("buddy.conf");

    MainTask mainTask;

    std::cerr << "finishing" << std::endl;

    return 0;
}
