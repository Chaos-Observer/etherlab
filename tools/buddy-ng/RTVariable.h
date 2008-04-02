/* **********************************************************************
 *
 * $Id$
 *
 * This defines the base class of a Real-Time Variable, the aggregation of
 * signal and parameters
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

#ifndef RTVARIABLE_H
#define RTVARIABLE_H

#include <string>
#include <vector>
#include "include/etl_data_info.h"

class RTVariable {
    public:
        RTVariable(const std::string &path, const std::string &alias,
                si_datatype_t dataType, std::vector<size_t>& dims);
        virtual ~RTVariable();

        const std::string& getPath() const { return path; }
        si_datatype_t getDataType() const { return dataType; }
        const std::vector<size_t>& getDims() const { return dims; }

        virtual unsigned int getSampleTime() const {return 0;}
        virtual bool isWriteable() const = 0;

    private:
        const std::string path;
        const std::string alias;
        const si_datatype_t dataType; 
        const std::vector<size_t> dims;
};
#endif // RTVARIABLE_H
