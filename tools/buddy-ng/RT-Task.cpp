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
#include "ConfigFile.h"
#include "Exception.h"

#include <iostream>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

RTTask::RTTask(Task* parent): Task(parent),
    device(ConfigFile::getString("", "device", "/dev/etl"))
{
    std::cout << "opening " << device << std::endl;
    fd = open(device.c_str(), O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        throw Exception(
                std::string("Failed to open ") + device + ": " 
                + strerror(errno));
    }

    if (::ioctl(fd, LOCK_KERNEL)) {
        throw Exception(
                std::string("Could not get Kernel Properties: ")
                + strerror(errno));
    }

    enableRead(fd);
}

RTTask::~RTTask()
{
    // TODO: neatly kill all children

    disableRead();
    close(fd);
}

int RTTask::read(int)
{
    struct rtcom_event event[1024], *p = event;
    int n, total;

    n = ::read(fd, event, sizeof(event));

    if (n % sizeof(*p)) {
        // Protocol error
        throw Exception("RTCom IO error: read() returned a "
                    "length that is not an integer multiple of "
                    "struct rtcom_event.");
    }
    else if (n < 0) {
        // n < 0 here. Return the negative value. This means trouble, 
        // we're going to die now :(
        return errno;
    }

    total = n;
    while (n) {
        std::cout << "read " << n << "bytes." << std::endl;
        switch (p->type) {
            case rtcom_event::new_model:
                {
                    RTModel *model = new RTModel(this, p->model_name);

                    std::cout << "new RTModel " << p->model_name << std::endl;

                    modelMap[p->model_name] = model;
//                    children[model] = modelMap.find(p->model_name);

                    break;
                }
            case rtcom_event::del_model:
                {
                    std::cout << "delete RTModel " << p->model_name 
                        << std::endl;
                    ModelMap::iterator m = modelMap.find(p->model_name);
                    if (m != modelMap.end()) {
                        kill(m->second, 0);
                    }
                }
                break;
        }
        n -= sizeof(*p);
        p++;
    }

    return total;
}

void RTTask::kill(Task* child, int rv)
{
//    delete children(child)->second;
//    modelMap.erase(children(child));
//    children.erase(child);
}
