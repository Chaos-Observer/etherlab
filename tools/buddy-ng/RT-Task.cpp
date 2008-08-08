/* **********************************************************************
 *
 * $Id$
 *
 * This defines the class used to interact with the RT-AppCore.
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
#include "RT-App.h"
#include "RTAppClient.h"
#include "ConfigFile.h"
#include "Exception.h"

#include <iostream>

RTTask::RTTask(): Task(NULL),
    device(ConfigFile::getString("", "device", "/dev/etl")), fd(device)
{
    std::cout << "opened " << device << std::endl;

    fd.ioctl(SELECT_APPCORE, "SELECT_APPCORE");

    enableRead(fd.getFileNo());
}

RTTask::~RTTask()
{
    for (AppMap::iterator m = appMap.begin(); m != appMap.end(); m++) {
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
            std::cout << "new RTApp " << event.id << std::endl;

            try {
                RTApp *app = new RTApp(this, event.id);
                appMap[event.id] = app;
                taskMap[app] = event.id;
                for (it = rtComTaskList.begin(); it != rtComTaskList.end();
                        it++) {
                    (*it)->newApplication(app);
                }
            } catch (Exception& e) {
                std::cerr << "Could not open app id " << event.id 
                    << ": " << e.what() << std::endl;
            }

            break;

        case rtcom_event::del_app:
            std::cout << "delete RTApp " << event.id 
                << std::endl;

            delete appMap[event.id];

            appMap.erase(event.id);
            taskMap.erase(appMap[event.id]);

            break;
    }

    return n;
}

const RTApp* RTTask::getApplication(unsigned int id)
{
    AppMap::iterator it = appMap.find(id);
    return (it == appMap.end()) ? 0 : it->second;
}

void RTTask::kill(Task* child, int rv)
{
    unsigned int childId = taskMap[child];

    delete appMap[childId];

    appMap.erase(childId);
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
