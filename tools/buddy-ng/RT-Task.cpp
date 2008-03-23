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
                    RTModel *model = new RTModel(fd, p->data.model_ref);
                    struct rtcom_privdata pd;

                    pd.ref = p->data.model_ref;
                    pd.priv_data = model;

                    modelList.push_back(model);
                    if (::ioctl(fd, SET_PRIV_DATA, &pd)) {
                        return errno;
                    }

                    break;
                }
            case rtcom_event::del_model:
                {
                    RTModel *model = reinterpret_cast<RTModel*>(p->priv_data);
                    delete model;
                    modelList.remove(model);
                }
                break;
        }
        n -= sizeof(*p);
        p++;
    }

    return total;
}
