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
#include <sys/mman.h>

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

    std::cout << "rt_properties = " << (void*)&rt_properties << std::endl;
    if (ioctl(fd, GET_RTK_PROPERTIES, &rt_properties)) {
        throw Exception(
                std::string("Could not get Kernel Properties: ")
                + strerror(errno));
    }

    io_mem = mmap(0, rt_properties.iomem_len, PROT_READ, MAP_PRIVATE, fd, 0);
    if (io_mem == MAP_FAILED) {
        throw Exception(
                std::string("Could not map IO memory to kernel: ")
                + strerror(errno));
    }

    enableRead(fd);
}

RTTask::~RTTask()
{
    munmap(io_mem, rt_properties.iomem_len);
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
        RTModel *model = reinterpret_cast<RTModel*>(p->priv_data);
        switch (p->type) {
            case rtcom_event::new_model:
                {
                    RTModel *model = new RTModel(fd, p->data.model_ref);
                    struct rtcom_ioctldata id;

                    std::cout << "new RTModel " << model << std::endl;

                    id.model_id = p->data.model_ref;
                    id.data = model;

                    if (::ioctl(fd, SET_PRIV_DATA, &id)) {
                        return errno;
                    }
                    modelMap[model->getName()] = model;

                    break;
                }
            case rtcom_event::del_model:
                {
                    std::cout << "delete RTModel " << model << std::endl;
                    ModelMap::iterator m;
                    modelMap.erase(model->getName());
                    delete model;
                }
                break;
        }
        n -= sizeof(*p);
        p++;
    }

    return total;
}
