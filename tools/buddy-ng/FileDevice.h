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

#ifndef FILEDEVICE_H
#define FILEDEVICE_H

#include <string>

class FileDevice {
    public:
        FileDevice(const std::string& path);
        ~FileDevice();

        int ioctl(int request, long data, const std::string& errctxt);
        int ioctl(int request, const std::string& errctxt);
        void *mmap(size_t len);
        int getFileNo() const { return fd; }
        int read(void* buf, size_t len);

    private:
        int fd;
        void *io_mem;
        size_t io_mem_len;
};
#endif // FILEDEVICE_H
