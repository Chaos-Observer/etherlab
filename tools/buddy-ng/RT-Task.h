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
#include "FileDevice.h"

#include <string>
#include <map>
#include "include/fio_ioctl.h"

class RTModel;
class RTComTask;

class RTTask: public Task {
    public:
        typedef std::map<std::string,RTModel*> ModelMap;

        RTTask(Task* task);
        ~RTTask();

        const ModelMap& getModelMap() const {
            return modelMap;
        }
        const std::string& getDevice() const { return device; }

        void setComTask(RTComTask* rtComTask);
        void clrComTask(RTComTask* rtComTask);

    private:
        const std::string device;
        FileDevice fd;

        std::list<RTComTask*> rtComTaskList;

        ModelMap modelMap;

        // Reimplemented from class Task
        int read(int fd);
        void kill(Task* child, int rv);
};
#endif // RTTASK_H
