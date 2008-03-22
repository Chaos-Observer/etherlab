/* **********************************************************************
 *
 * $Id$
 *
 * This defines the class used to interact with the real-time kernel
 * 
 * Copyright (C) 2008  Richard Hacker
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * **********************************************************************/

#ifndef RTTASK_H
#define RTTASK_H

#include "Task.h"

#include <string>
#include <include/fio_ioctl.h>

class RTTask: public Task {
    public:
        RTTask(Task* task);
        ~RTTask();

    private:
        const std::string device;
        int fd;

//        struct mdl_properties properties;
//        char* rtP;

        // Reimplemented from class Task
        int read(int fd);
};
#endif // RTTASK_H