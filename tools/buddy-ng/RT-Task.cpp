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
#include "ConfigFile.h"
#include "Exception.h"

#include <iostream>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

RTTask::RTTask(Task* parent): Task(parent),
    device(configFile->getString("", "device", "/dev/etl"))
{
    std::cout << "opening " << device << std::endl;
    fd = open(device.c_str(), O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        throw Exception(
                std::string("Failed to open ") + device + ": " 
                + strerror(errno));
    }

    //ioctl(fd, GET_RT_PROPERTIES);
    //mmap();

    enableRead(fd);
//    if (ioctl(fd, GET_MDL_PROPERTIES, &properties)) {
//        throw Exception(std::string("Could not fetch model properties: ")
//                + strerror(errno));
//    }
//
//    rtP = new char[properties.rtP_size];
//            
//    if (ioctl(fd, GET_PARAM, rtP)) {
//        throw Exception(std::string("Could not fetch model parameter set: ")
//                + strerror(errno));
//    }

}

RTTask::~RTTask()
{
}

int RTTask::read(int)
{
    struct rt_event {
        enum type_t {new_model, del_model, new_data} type;
    } event;
    int n, total = 0;

    while ((n = ::read(fd, &event, sizeof(event))) == sizeof(event)) {
        total += n;
        switch (event.type) {
            case rt_event::new_model:
                break;
            case rt_event::del_model:
                break;
            case rt_event::new_data:
                break;
            default:
                break;
        }
    }

    if (!n) {
        return total;
    }
    else if (n > 0) {
        // Protocol error
        throw;
    }
    else {
        // n < 0 here. Return the negative value. This means trouble, 
        // we're going to die now :(
        return n;
    }
}
