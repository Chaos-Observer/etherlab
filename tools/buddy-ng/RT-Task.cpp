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
#include "RTComTask.h"
#include "ConfigFile.h"
#include "Exception.h"

#include <iostream>

RTTask::RTTask(Task* parent): Task(parent),
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
    std::list<RTComTask*>::iterator it;
    std::string modelName;

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
            {
                std::cout << "new RTModel " << event.id << std::endl;

                try {
                    RTModel *model =
                        new RTModel(this, event.id);
                    modelName = model->getName();
                    modelMap[modelName] = model;
                } catch (Exception& e) {
                    std::cerr << "Could not open model id " << event.id 
                        << ": " << e.what() << std::endl;
                }

                for (it = rtComTaskList.begin(); it != rtComTaskList.end();
                        it++) {
                    (*it)->newModel(modelName);
                }

                break;
            }
        case rtcom_event::del_app:
            {
                for (ModelMap::iterator m = modelMap.begin();
                        m != modelMap.end(); m++) {
                    if (m->second->getId() == event.id) {
                        std::cout << "delete RTModel " << event.id 
                            << std::endl;
                        modelName = m->second->getName();
                        for (it = rtComTaskList.begin(); 
                                it != rtComTaskList.end(); it++) {
                            (*it)->delModel(modelName);
                        }

                        kill(m->second, 0);
                        break;
                    }
                }
            }
            break;
    }

    return n;
}

void RTTask::kill(Task* child, int rv)
{
    ModelMap::iterator old_m, m = modelMap.begin();
    while (m != modelMap.end()) {
        old_m = m++;

        if (old_m->second == child) {
            delete child;
            modelMap.erase(old_m);
        }
    }
}

void RTTask::setComTask(RTComTask* rtComTask)
{
    rtComTaskList.push_back(rtComTask);
}

void RTTask::clrComTask(RTComTask* rtComTask)
{
    rtComTaskList.remove(rtComTask);
}
