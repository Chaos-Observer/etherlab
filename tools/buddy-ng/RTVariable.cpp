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

#include "RTVariable.h"

RTVariable::RTVariable(const std::string &_path, const std::string &_alias,
        si_datatype_t _dataType, si_orientation_t _orientation,
        unsigned int _rnum, unsigned int _cnum) :
    path(_path), alias(_alias), dataType(_dataType), orientation(_orientation),
    rnum(_rnum), cnum(_cnum)
{
}

RTVariable::~RTVariable()
{
}
