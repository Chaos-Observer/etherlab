/* **********************************************************************
 *
 * $Id$
 *
 * This defines the class of file device. It simplifies cleaning up of
 * file structures when this class goes out of scope, i.e. silent 
 * destruction.
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

#include "FileDevice.h"
#include "Exception.h"

#include <cerrno>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

FileDevice::FileDevice(const std::string& path):
    io_mem(MAP_FAILED)
{
    fd = ::open(path.c_str(), O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        throw Exception(
                std::string("open(\"").append(path).append("\") failed: ") 
                .append(strerror(errno)));
    }
}

FileDevice::~FileDevice()
{
    if (io_mem != MAP_FAILED) {
        ::munmap(io_mem, io_mem_len);
    }
    if (fd >= 0)
        ::close(fd);
}


int FileDevice::ioctl(int request, const std::string& errctxt)
{
    int rv;
    rv = ::ioctl(fd, request);

    if (rv < 0) {
        throw Exception(
                std::string("ioctl(").append(errctxt).append(") failed: ")
                .append(strerror(errno)));
    }

    return rv;
}

int FileDevice::ioctl(int request, long data, const std::string& errctxt)
{
    int rv;
    rv = ::ioctl(fd, request, data);

    if (rv < 0) {
        throw Exception(
                std::string("ioctl(").append(errctxt).append(") failed: ")
                .append(strerror(errno)));
    }

    return rv;
}

int FileDevice::read(void* buf, size_t len)
{
    int rv;
    rv = ::read(fd, buf, len);

    if (rv < 0) {
        throw Exception(
                std::string("read() failed: ")
                .append(strerror(errno)));
    }

    return rv;
}

void* FileDevice::mmap(size_t len)
{
    io_mem_len = len;
    io_mem = ::mmap(0, len, PROT_READ, MAP_PRIVATE, fd, 0);
    if (io_mem == MAP_FAILED) {
        throw Exception(
                std::string("mmap() failed: ").append(strerror(errno)));
    }

    return io_mem;
}
