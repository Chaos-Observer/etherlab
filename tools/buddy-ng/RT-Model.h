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

#ifndef RTMODEL_H
#define RTMODEL_H

#include "include/fio_ioctl.h"
#include "RT-Task.h"

#include <vector>
#include <string>

class RTVariable;
class RTSignal;
class RTParameter;
struct timeval;

class RTModel: public Task {
    public:
        RTModel(RTTask* parent, const std::string& modelName);
        ~RTModel();

        const std::string& getName() const { return name; }
        const std::string& getVersion() const { return version; }
        const std::vector<RTVariable*>& getVariableList() const { 
            return variableList; 
        }

    private:
        const Task* parent;
        const std::string name;
        std::string version;
        int fd;

        char *rtP;
        void *io_mem;

        struct rt_kernel_prop rt_properties;

        std::vector<RTVariable*> variableList;
        std::vector<uint32_t> sampleTime;

        void getDims(std::vector<size_t>& dims, const struct signal_info *si,
                unsigned int type);

        // Reimplemented from class Task
        int read(int fd);
};
#endif // RTMODEL_H
