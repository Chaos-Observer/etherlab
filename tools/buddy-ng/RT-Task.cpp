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

#include "RT-Task.h"
#include "RT-Model.h"
#include "RTAppClient.h"
#include "ConfigFile.h"
#include "Exception.h"

#include <iostream>

RTTask::RTTask(): Task(NULL),
    device(ConfigFile::getString("", "device", "/dev/etl")), fd(device)
{
    std::cout << "opened " << device << std::endl;

    fd.ioctl(SELECT_KERNEL, "SELECT_KERNEL");

    enableRead(fd.getFileNo());
}

RTTask::~RTTask()
{
    for (ModelMap::iterator m = modelMap.begin(); m != modelMap.end(); m++) {
        delete m->second;
    }
}

int RTTask::read(int)
{
    struct rtcom_event event;
    int n;
    std::list<RTAppClient*>::iterator it;

    n = fd.read(&event, sizeof(event));

    if (n % sizeof(event)) {
        // Protocol error
        throw Exception("RTCom IO error: read() returned a "
                "length that is not an integer multiple of "
                "struct rtcom_event.");
    }

    std::cout << "read " << n << "bytes." << std::endl;
    switch (event.type) {
        case rtcom_event::new_app:
            std::cout << "new RTModel " << event.id << std::endl;

            try {
                RTModel *app = new RTModel(this, event.id);
                modelMap[event.id] = app;
                taskMap[app] = event.id;
                for (it = rtComTaskList.begin(); it != rtComTaskList.end();
                        it++) {
                    (*it)->newApplication(app);
                }
            } catch (Exception& e) {
                std::cerr << "Could not open model id " << event.id 
                    << ": " << e.what() << std::endl;
            }

            break;

        case rtcom_event::del_app:
            std::cout << "delete RTModel " << event.id 
                << std::endl;

            delete modelMap[event.id];

            modelMap.erase(event.id);
            taskMap.erase(modelMap[event.id]);

            break;
    }

    return n;
}

const RTModel* RTTask::getApplication(unsigned int id)
{
    ModelMap::iterator it = modelMap.find(id);
    return (it == modelMap.end()) ? 0 : it->second;
}

void RTTask::kill(Task* child, int rv)
{
    unsigned int childId = taskMap[child];

    delete modelMap[childId];

    modelMap.erase(childId);
    taskMap.erase(child);
}

void RTTask::regAppClient(RTAppClient* appClient)
{
    rtComTaskList.push_back(appClient);
}

void RTTask::deregAppClient(RTAppClient* appClient)
{
    rtComTaskList.remove(appClient);
}
