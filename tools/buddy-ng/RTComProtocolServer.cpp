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
#include "RT-Model.h"
#include "RTVariable.h"

#include "RTComVocab.h"

#include <iostream>
#include <arpa/inet.h>

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
        newApplication(m->second);
    }
}

//************************************************************************
RTComProtocolServer::~RTComProtocolServer()
//************************************************************************
{
}

//************************************************************************
void RTComProtocolServer::newApplication(const RTModel* app)
//************************************************************************
{
    LayerStack::IOBuffer *buf = new LayerStack::IOBuffer(this);
    buf->appendInt(0);
    buf->appendInt(NEW_APPLICATION);
    buf->appendInt(app->getId());
    buf->append(app->getName());
    buf->transmit();
}

//************************************************************************
void RTComProtocolServer::delApplication(const RTModel* app)
//************************************************************************
{
    LayerStack::IOBuffer *buf = new LayerStack::IOBuffer(this);
    buf->appendInt(0);
    buf->appendInt(DEL_APPLICATION);
    buf->appendInt(app->getId());
    buf->append(app->getName());
    buf->transmit();
}

//************************************************************************
void RTComProtocolServer::finished(const LayerStack::IOBuffer* buf)
//************************************************************************
{
    std::cerr << __func__ << " delete buf " << buf << std::endl;
    delete buf;
}

//************************************************************************
size_t RTComProtocolServer::receive(const char* buf, size_t data_len)
//************************************************************************
{
    std::cerr << __func__ << " called to receive " 
        << std::string(buf,data_len) << std::endl;
    uint32_t command = ntohl(*(uint32_t*)buf);
    buf += sizeof(command);

    LayerStack::IOBuffer *reply = new LayerStack::IOBuffer(this);

    std::cerr << "Command " << command << std::endl;
    switch (command) {
        case APPLICATION_INFO:
            {
                uint32_t appId = ntohl(*(uint32_t*)buf);
                const RTModel *app = rtTask->getApplication(appId);
                const RTModel::StList st = app->getSampleTimes();
                const RTModel::VariableList vl = app->getVariableList();
            std::cerr << "Sending App info for " << appId << std::endl;

                reply->appendInt(0); // Command channel

                reply->appendInt(APPLICATION_INFO);

                reply->appendInt(appId);

                reply->appendInt(st.size());
                for (unsigned int i = 0; i < st.size(); i++) {
                    reply->appendInt(st[i]);
                }
                reply->appendInt(vl.size());
                for (unsigned int i = 0; i < vl.size(); i++) {
                    const RTVariable::DimList dim = vl[i]->getDims();

                    reply->appendInt(vl[i]->getSampleTime());
                    reply->appendInt(vl[i]->getDataType());
                    reply->appendInt(vl[i]->isWriteable());

                    reply->appendInt(dim.size());
                    for (unsigned int j = 0; j < dim.size(); j++)
                        reply->appendInt(dim[i]);

                    reply->appendInt(vl[i]->getPath().length());
                    reply->appendInt(vl[i]->getName().length());
                    reply->appendInt(vl[i]->getAlias().length());

                    reply->append(vl[i]->getPath());
                    reply->append(vl[i]->getName());
                    reply->append(vl[i]->getAlias());

                    unsigned int align = reply->length()%4;
                    reply->append(align ? 4 - align : 0, '*');
                }
            }
            break;
    }

    if (reply->transmit())
        delete reply;

    return data_len;
}

