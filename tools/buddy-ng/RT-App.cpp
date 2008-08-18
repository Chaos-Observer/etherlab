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

#include "RT-App.h"
#include "ConfigFile.h"
#include "Exception.h"
#include "RTSignal.h"
#include "RTParameter.h"

#include <iterator>
#include <algorithm>
#include <iostream>
#include <memory>

RTApp::RTApp(RTTask *parent, unsigned int _id):
    Task(parent), id(_id), fd(parent->getDevice())
{
    struct signal_info si;
    struct app_properties app_properties;

    // First choose the application we're dealing with
    fd.ioctl(SELECT_APP, id, "SELECT_APP");

    std::cout << "rt_properties = " << (void*)&rt_properties << std::endl;
    fd.ioctl(GET_RTAC_PROPERTIES, (long)&rt_properties, "GET_RTAC_PROPERTIES");

    io_mem = fd.mmap(rt_properties.iomem_len);

    // First of all get the application properties
    fd.ioctl(GET_APP_PROPERTIES, (long)&app_properties, "GET_APP_PROPERTIES");
    version = app_properties.version;
    name = app_properties.name;

    // Now that the number of sample times is known, get them and copy to
    // sampleTime
    uint32_t st[app_properties.num_st];
    fd.ioctl(GET_APP_SAMPLETIMES, (long)st, "GET_APP_SAMPLETIMES");
    sampleTime.resize(app_properties.num_st);
    std::copy(st, st+app_properties.num_st, sampleTime.begin());

    std::ostream_iterator<uint32_t> oo(std::cout, " ");
    std::cout << "application properties " 
        << app_properties.variable_path_len << " " 
        << app_properties.signal_count << " " 
        << app_properties.param_count << " " 
        << name << " " 
        << version << " ";
    copy(sampleTime.begin(), sampleTime.end(), oo);
    std::cout << std::endl;

    // Knowing the number of signals and parameters, as well as the length
    // of the longest path name, a structure can be allocated so that the
    // information can be fetched from the appcore
    si.path = new char[app_properties.variable_path_len + 1];
    si.path_buf_len = app_properties.variable_path_len + 1;
    std::auto_ptr<char> signal_path(si.path);

    std::vector<size_t> dims;
    unsigned int varIdx = 0;

    variableList.resize(app_properties.signal_count 
            + app_properties.param_count);
    for (si.index = 0; si.index < app_properties.signal_count; 
            si.index++) {
        fd.ioctl(GET_SIGNAL_INFO, (long)&si, "GET_SIGNAL_INFO");
        getDims(dims, &si, GET_SIGNAL_DIMS);
        variableList[varIdx++] = new RTSignal(si.path, si.name, si.alias,
                si.data_type, dims, sampleTime.at(si.st_index));
    }

    for (si.index = 0; si.index < app_properties.param_count; si.index++) {
        fd.ioctl(GET_PARAM_INFO, (long)&si, "GET_PARAM_INFO");
        getDims(dims, &si, GET_PARAM_DIMS);
        variableList[varIdx++] = new RTParameter( si.path, si.name, si.alias,
                si.data_type, dims);
    }

    enableRead(fd.getFileNo());
}

RTApp::~RTApp()
{
    for (std::vector<RTVariable*>::iterator it = variableList.begin();
            it != variableList.end(); it++)
        delete *it;
}

/** Get signal's vector dimensions.
 *
 * A signal's array dimensions are coded in si->dims[2]:
 * dims[0] == 1; dims[1] == 0     : scalar
 * dims[0] >  1; dims[1] == 0     : vector
 * dims[0] >  0; dims[1] >  0     : matrix
 * dims[0] == 0; dims[1] >  0     : n-Dimensional matrix. Number of dimensions
 *                                  is coded in dims[1]. Have to fetch dims
 *                                  in a second step
 */
void RTApp::getDims(std::vector<size_t>& dims, const struct signal_info *si,
        unsigned int dim_type)
{
    if (!si->dim[0]) {
        // Number of dimensions is >2. Actual dimensions is in si->dim[1]
        // Get the new dimensions
        size_t new_dims[si->dim[1]];

        // Signal index gets passed in first array element
        new_dims[0] = si->index;

        fd.ioctl(dim_type, (long)new_dims, "GET_SIGNAL/PARAM_DIMS");

        dims.resize(si->dim[1]);
        std::copy(new_dims, new_dims + si->dim[1], dims.begin());
    }
    else {
        dims.resize(si->dim[1] ? 2 : 1);
        std::copy(si->dim, si->dim + dims.size(), dims.begin());
    }
}

int RTApp::read(int)
{
    int total = 0;
    return total;
}