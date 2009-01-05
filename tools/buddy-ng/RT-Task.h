/* **********************************************************************
 *
 * $Id$
 *
 * This defines the class used to interact with the RT-AppCore. Only one
 * instance of this class exists. This class keeps track of all real-time
 * applications that are running, as well as all network clients that are
 * being served with data.
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

#include "lib/Task.h"
#include "FileDevice.h"

#include <string>
#include <map>
#include "include/fio_ioctl.h"

class RTApp;
class RTAppClient;

class RTTask: public Task {
    public:
        typedef std::map<unsigned int,RTApp*> AppMap;

        RTTask();
        ~RTTask();

        const AppMap& getAppMap() const {
            return appMap;
        }
        const std::string& getDevice() const { return device; }

        const RTApp* getApplication(unsigned int id);

        void regAppClient(RTAppClient* rtComTask);
        void deregAppClient(RTAppClient* rtComTask);

    private:
        const std::string device;
        FileDevice fd;

        std::list<RTAppClient*> rtComTaskList;

        AppMap appMap;
        std::map<Task*,unsigned int> taskMap;

        // Reimplemented from class Task
        int read(int fd);
        void kill(Task*, int);
};
#endif // RTTASK_H
