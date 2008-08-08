/* **********************************************************************
 *
 * $Id$
 *
 * This defines the class used to interact with the real-time appcore.
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

#include "AppCtlTask.h"
#include "ConfigFile.h"
#include "Exception.h"

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "include/fio_ioctl.h"

#include <iostream>
#include <stdexcept>
#include <cerrno>
#include <cstring>
#include <string>

AppCtlTask::AppCtlTask()
{
    std::string device(ConfigFile::getString("", "device", "/dev/etl"));
    etl_fd = ::open(device.c_str(), O_RDONLY | O_NONBLOCK);
    if (etl_fd < 0) {
        throw FileOpenException(device);
    }
    std::cout << "opened " << device << std::endl;

    if (::ioctl(etl_fd, SELECT_APPCORE)) {
        throw IoctlException();
    }

    enableRead(etl_fd);
}

AppCtlTask::~AppCtlTask()
{
}

int AppCtlTask::read(int)
{
    struct rtcom_event event;
    int n;

    n = ::read(etl_fd, &event, sizeof(event));

    if (n % sizeof(event)) {
        // Protocol error
        throw Exception("RTCom IO error: read() returned a "
                "length that is not an integer multiple of "
                "struct rtcom_event.");
    }

    std::cout << "read " << n << "bytes." << std::endl;
    switch (event.type) {
        case rtcom_event::new_app:
            std::cout << "new RT-App " << event.id << std::endl;

            break;

        case rtcom_event::del_app:
            std::cout << "delete RT-App " << event.id 
                << std::endl;

            break;
    }

    return n;
}

void AppCtlTask::kill(Task* child, int rv)
{
}
