/* **********************************************************************
 *  $Id$
 * **********************************************************************/

/*
 * Copyright (C) 2008 Richard Hacker
 * <lerichi@gmx.net>
 *
 *  License: GPL
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include "RTComProtocolServer.h"
#include "IOBuffer.h"
#include "RT-Task.h"

#include "RTComVocab.h"

#include <iostream>

#undef DEBUG
//#define DEBUG 1

//************************************************************************
RTComProtocolServer::RTComProtocolServer(Layer *below, RTTask *_rtTask):
    Layer(below,"RTComProtocolServer", 0), RTAppClient(_rtTask),
    rtTask(_rtTask)
//************************************************************************
{
    std::cerr << "XXXXXXXXXXXXX 2 " << __func__ << std::endl;

    RTTask::ModelMap mm = rtTask->getModelMap();
    for (RTTask::ModelMap::iterator m = mm.begin(); m != mm.end(); m++) {
        newApplication(m->first);
    }
}

//************************************************************************
RTComProtocolServer::~RTComProtocolServer()
//************************************************************************
{
}

//************************************************************************
void RTComProtocolServer::newApplication(const std::string& name)
//************************************************************************
{
    LayerStack::IOBuffer *buf = new LayerStack::IOBuffer(this);
    buf->appendInt(0);
    buf->appendInt(NEW_APPLICATION);
    buf->append(name);
    buf->transmit();
}

//************************************************************************
void RTComProtocolServer::delApplication(const std::string&)
//************************************************************************
{
}

//************************************************************************
void RTComProtocolServer::finished(const LayerStack::IOBuffer* buf)
//************************************************************************
{
    std::cerr << __func__ << " delete buf " << buf << std::endl;
    delete buf;
}
