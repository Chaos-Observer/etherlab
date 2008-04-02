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

#include "RT-Model.h"
#include "ConfigFile.h"
#include "Exception.h"
#include "RTSignal.h"
#include "RTParameter.h"

#include <iterator>
#include <algorithm>
#include <iostream>
#include <cerrno>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

RTModel::RTModel(int _fd, struct model *_model_ref): 
    fd(_fd), model_ref(_model_ref)
{
    struct signal_info si;
    struct mdl_properties mdl_properties;
    struct rtcom_ioctldata id;

    id.model_id = model_ref;
    id.data = &mdl_properties;
    id.data_len = sizeof(mdl_properties);

    // First of all get the model properties
    if (::ioctl(fd, GET_MDL_PROPERTIES, &id)) {
        throw Exception("Could not get model properties.");
    }
    name = mdl_properties.name;
    version = mdl_properties.version;

    // Now that the number of sample times is known, get them and copy to
    // sampleTime
    uint32_t st[mdl_properties.num_st];
    id.data = st;
    id.data_len = sizeof(st);
    if (::ioctl(fd, GET_MDL_SAMPLETIMES, &id)) {
        throw Exception("Could not get model sample times.");
    }
    sampleTime.resize(mdl_properties.num_st);
    std::copy(st, st+mdl_properties.num_st, sampleTime.begin());

    std::ostream_iterator<uint32_t> oo(std::cout, " ");
    std::cout << "model properties " 
        << mdl_properties.variable_path_len << " " 
        << mdl_properties.signal_count << " " 
        << mdl_properties.param_count << " " 
        << name << " " 
        << version << " ";
    copy(sampleTime.begin(), sampleTime.end(), oo);
    std::cout << std::endl;

    // Knowing the number of signals and parameters, as well as the length
    // of the longest path name, a structure can be allocated so that the
    // information can be fetched from the kernel
    si.path = new char[mdl_properties.variable_path_len + 1];
    id.data = &si;

    // Not quite true, here the length of the path is added too
    id.data_len = sizeof(si) + mdl_properties.variable_path_len + 1;

    std::vector<size_t> dims;
    unsigned int varIdx = 0;

    variableList.resize(mdl_properties.signal_count 
            + mdl_properties.param_count);
    for (si.index = 0; si.index < mdl_properties.signal_count; 
            si.index++) {
        if (::ioctl(fd, GET_SIGNAL_INFO, &id)) {
            throw Exception("Error on GET_SIGNAL_INFO.");
        }
        getDims(dims, &si, GET_SIGNAL_DIMS);
        variableList[varIdx++] = new RTSignal(si.path, si.name, 
                    si.data_type, dims, sampleTime.at(si.st_index));
    }

    for (si.index = 0; si.index < mdl_properties.param_count; si.index++) {
        if (::ioctl(fd, GET_PARAM_INFO, &id)) {
            throw Exception("Error on GET_SIGNAL_INFO.");
        }
        getDims(dims, &si, GET_PARAM_DIMS);
        variableList[varIdx++] = new RTParameter(
                    std::string(si.path) + '/' + si.name, std::string(),
                    si.data_type, dims);
    }
    delete[] si.path;
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
void RTModel::getDims(std::vector<size_t>& dims, const struct signal_info *si,
        unsigned int dim_type)
{
    if (!si->dim[0]) {
        // Number of dimensions is >2. Actual dimensions is in si->dim[1]
        // Get the new dimensions
        size_t new_dims[si->dim[1]];
        struct rtcom_ioctldata id;

        id.model_id = model_ref;
        id.data = new_dims;
        id.data_len = sizeof(new_dims);

        // Signal index gets passed in first array element
        new_dims[0] = si->index;

        if (::ioctl(fd, dim_type, &id)) {
            throw Exception("Error on GET_SIGNAL/PARAM_DIMS.");
        }

        dims.resize(si->dim[1]);
        std::copy(new_dims, new_dims + si->dim[1], dims.begin());
    }
    else {
        dims.resize(si->dim[1] ? 2 : 1);
        std::copy(si->dim, si->dim + dims.size(), dims.begin());
    }
}

RTModel::~RTModel()
{
    for (std::vector<RTVariable*>::iterator it = variableList.begin();
            it != variableList.end(); it++)
        delete *it;
}
