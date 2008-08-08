
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

#ifndef RTAPP_H
#define RTAPP_H

#include "include/fio_ioctl.h"
#include "RT-Task.h"
#include "FileDevice.h"

#include <vector>
#include <string>

class RTVariable;
class RTSignal;
class RTParameter;

class RTApp: public Task {
    public:
        RTApp(RTTask* parent, unsigned int id);
        ~RTApp();

        unsigned int getId() const { return id; }
        const std::string& getName() const { return name; }
        const std::string& getVersion() const { return version; }

        typedef std::vector<RTVariable*> VariableList;
        const VariableList& getVariableList() const { 
            return variableList; 
        }

        typedef std::vector<uint32_t> StList;
        const StList& getSampleTimes() const {
            return sampleTime;
        }

    private:
        const unsigned int id;
        std::string name;
        std::string version;
        FileDevice fd;

        char *rtP;
        void *io_mem;

        struct rt_appcore_prop rt_properties;

        VariableList variableList;
        StList sampleTime;

        void getDims(std::vector<size_t>& dims, const struct signal_info *si,
                unsigned int type);

        // Reimplemented from class Task
        int read(int fd);
};
#endif // RTAPP_H
