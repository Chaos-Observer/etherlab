/* **********************************************************************
 *
 * $Id$
 *
 * This defines the class of Real-Time Signals. Signals have a sample time
 * and cannot be written to.
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

#ifndef RTPARAMETER_H
#define RTPARAMETER_H

#include "RTVariable.h"

class RTParameter: public RTVariable {
    public:
        RTParameter(const std::string &path, const std::string &alias,
                si_datatype_t dataType, si_orientation_t orientation,
                unsigned int _rnum, unsigned int _cnum);
        ~RTParameter();

    private:
};
#endif // RTPARAMETER_H
