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

#include "RTSignal.h"

RTSignal::RTSignal(const std::string &_path, const std::string &_alias,
        si_datatype_t _dataType, si_orientation_t _orientation,
        unsigned int _rnum, unsigned int _cnum, 
        uint32_t _sampleTime): 
    RTVariable(_path, _alias, _dataType, _orientation, _rnum, _cnum),
    sampleTime(_sampleTime)
{
}

RTSignal::~RTSignal()
{
}
