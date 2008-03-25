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
    struct signal_info *si;
    struct rtcom_ioctldata id;
    struct mdl_properties mdl_properties;

    id.model_id = model_ref;
    id.data = &mdl_properties;
    id.data_len = sizeof(mdl_properties);

    if (::ioctl(fd, GET_MDL_PROPERTIES, &id)) {
        throw Exception("Could not get model properties.");
    }
    name = mdl_properties.name;
    version = mdl_properties.version;

    uint32_t st[mdl_properties.num_st];
    id.data = st;
    id.data_len = sizeof(st);
    if (::ioctl(fd, GET_MDL_SAMPLETIMES, &id)) {
        throw Exception("Could not get model sample times.");
    }
    sampleTime.resize(mdl_properties.num_st);
    for (unsigned int i = 0; i < mdl_properties.num_st; i++)
        sampleTime[i] = st[i];

    std::ostream_iterator<uint32_t> oo(std::cout, " ");
    std::cout << "model properties " 
        << mdl_properties.variable_path_len << " " 
        << mdl_properties.signal_count << " " 
        << mdl_properties.param_count << " " 
        << name << " " 
        << version << " ";
    copy(sampleTime.begin(), sampleTime.end(), oo);
    std::cout << std::endl;

    id.data_len = sizeof(*si) + mdl_properties.variable_path_len + 1;
    id.data = new char[id.data_len];
    si = (struct signal_info*)id.data;

    signalList.resize(mdl_properties.signal_count);
    for (si->index = 0; si->index < mdl_properties.signal_count; 
            si->index++) {
        if (::ioctl(fd, GET_SIGNAL_INFO, &id)) {

        }
        else {
            signalList[si->index] = new RTSignal(si->path, si->name, 
                    si->data_type, si->orientation, si->rnum, si->cnum,
                    sampleTime.at(si->st_index));
        }
    }

    paramList.resize(mdl_properties.param_count);
    for (si->index = 0; si->index < mdl_properties.param_count; si->index++) {
        if (::ioctl(fd, GET_PARAM_INFO, &id)) {
        }
        else {
            paramList[si->index] = new RTParameter(
                    std::string(si->path) + si->name, std::string(),
                    si->data_type, si->orientation, si->rnum, si->cnum);
        }
    }
    delete[] (char*)id.data;
}

RTModel::~RTModel()
{
    for (std::vector<RTSignal*>::iterator it = signalList.begin();
            it != signalList.end(); it++)
        delete *it;
    for (std::vector<RTParameter*>::iterator it = paramList.begin();
            it != paramList.end(); it++)
        delete *it;
}
