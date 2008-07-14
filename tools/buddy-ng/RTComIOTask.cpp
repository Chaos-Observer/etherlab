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

#include "RTComIOTask.h"
#include "SocketExcept.h"
#include "ConfigFile.h"
#include "RT-Task.h"
#include "RT-Model.h"
#include "RTVariable.h"

#undef DEBUG
#define DEBUG 1

#include <iostream>
#include <arpa/inet.h>
//#include <sstream>
//#include <iterator>
#include <string>
//#include <cstring>
#include <cerrno>

using namespace std;

//************************************************************************
RTComIOTask::RTComIOTask(Task* parent, RTTask* _rtTask, int _fd, 
        unsigned int _buflen, unsigned int _maxbuf): 
    Task(parent), streambuf(), fd(_fd), rtTask(_rtTask), outBuflen(_buflen), 
    maxOutBuffers(_maxbuf)
//************************************************************************
{
    wptr = wbuf = ibuf = 0;
    outBufSize = 0;
    setp(0,0);

    inBufLen = 4096;
    inBufPtr = inBuf = new char[inBufLen];
    inBufEnd = inBuf + inBufLen;

    parserState = Init;
//    loggedIn = false;

    rtTask->setComTask(this);

    enableRead(fd);

    // Say hello to the client.
    hello();
}

//************************************************************************
RTComIOTask::~RTComIOTask()
//************************************************************************
{
    rtTask->clrComTask(this);
    disableRead();
    disableWrite();
    delete[] inBuf;
    for (list<char*>::iterator it = outBuf.begin(); it != outBuf.end(); it++)
        delete[] *it;
    cerr << "Deleting RTComIOTask" << this << endl;
    close(fd);
}

//************************************************************************
void RTComIOTask::hello()
// The hello message is sent to the client as soon as the connection is
// established. This is used to identify the protocol being used.
//
// The message is composed as follows:
// First 6 bytes: string "RTCom\0"
// Bytes 8-11: Major version as unsigned int
// Bytes 12-15: Minor version as unsigned int
//************************************************************************
{
    unsigned char s[16] = "RTCom";
    uint32_t len = htonl(16);

    // Set version to 1.0
    *(uint32_t *)&s[8] = htonl(1);
    *(uint32_t *)&s[12] = htonl(0);

    xsputn((char*)&len, sizeof(len));
    xsputn((char*)s, sizeof(s));
    sync();
}

//************************************************************************
void RTComIOTask::newApplication(const std::string& name)
// The newApplication() message informs the network client of a new running 
// application
//
// Telegram is composed as:
// Bytes
// 0-3  Telegram Length
// 4    'N' -> for new application
// 5... Application name
//************************************************************************
{
    uint32_t len = htonl(name.length()+1);
    xsputn((char*)&len, sizeof(len));
    sputc('N');
    xsputn(name.c_str(), name.size());
    sync();
    std::cout << "Sendign model " << name << std::endl;
}

//************************************************************************
void RTComIOTask::delApplication(const std::string& name)
// The newApplication() message informs the network client of a new running 
// application
//
// Telegram is composed as:
// Bytes
// 0-3  Telegram Length
// 4    'D' -> for deleted application
// 5... Application name
//************************************************************************
{
    uint32_t len = htonl(name.length()+1);
    xsputn((char*)&len, sizeof(len));
    sputc('D');
    xsputn(name.c_str(), name.size()+1);
    sync();
    std::cout << "Deleting model " << name << std::endl;
}

//************************************************************************
int RTComIOTask::write(int fd)
//************************************************************************
{
#if DEBUG
    cerr << "RTComIOTask::writeReady()" << endl;
#endif

    int len;

    if (!wptr || pptr() == wptr)
        return 0;

    if (wbuf == ibuf) {
        /* Writing and reading on the same buffer */
#if DEBUG
        cerr << "Sending " << pptr() - wptr 
            << " Bytes from same buf" << (void*)wbuf << endl;
#endif
        len = ::write(fd, wptr, pptr() - wptr);
#if DEBUG
        cerr << "Sent " << len << " bytes from " << (void*)wptr << endl;
        cerr << "<<<< Data: " << string(wptr, len) << endl;
#endif

        if (len > 0) {
            wptr += len;
#if DEBUG
            cerr << "Checking pointers: " 
                << "pptr() = " << (void*)pptr()
                << " wptr = " << (void*)wptr 
                << " bumping " << wbuf - pptr() << endl;
#endif
            if (pptr() == wptr) {
                wptr = wbuf;
                pbump(wbuf - pptr());
#if DEBUG
                cerr << "bumped all, new pptr() " << (void*)pptr() 
                    << " " << (void*)wbuf << endl;
#endif
            }
            else {
                outBufSize -= len;
#if DEBUG
                cerr << "decreasing 1 outBufSize " << len;
#endif
            }
        }
    } else {
        /* Writing and reading on different buffers */
#if DEBUG
        cerr << "Sending " << wbuf + outBuflen - wptr
            << " Bytes from different buffers " << (void*)wbuf << endl;
#endif
        len = ::write(fd, wptr, wbuf + outBuflen - wptr);
        if (len > 0) {
            wptr += len;
            outBufSize -= len;
#if DEBUG
            cerr << "decreasing 2 outBufSize " << len;
#endif
            if (wbuf + outBuflen == wptr) {
                /* Finished sending wbuf */
#if DEBUG
                cerr << "deleting buffer " << (void*)wbuf << endl;
#endif
                delete wbuf;
                outBuf.pop_front();
                wptr = wbuf = outBuf.front();
            }
        }
    }

    if (!outBufSize)
        disableWrite();

    return len;
}

//************************************************************************
int RTComIOTask::sync()
//************************************************************************
{
    enableWrite(fd);
    return 0;
}

//************************************************************************
std::streamsize RTComIOTask::xsputn(const char *s, std::streamsize len)
//************************************************************************
{
    streamsize n, count = 0;

#if DEBUG
    cerr << "RTComIOTask::xsputn(" << string(s,len) << ") len:" << len << endl;
#endif

    while (count != len) {
        n = min(epptr() - pptr(), len - count);

        if (!n) {
            // Current buffer is full
            if (new_page() == EOF)
                return count;
            continue;
        }

        memcpy(pptr(), s + count, n);
        pbump(n);
        count += n;
    }

    return count;
}

//************************************************************************
int RTComIOTask::overflow(int c) 
//************************************************************************
{ 
#if DEBUG
    cerr << "RTComIOTask::overflow(" << c << ")" << endl;
#endif
    if (new_page() == EOF)
        return EOF;

    *pptr() = c;
    pbump(1);

    return 0;
}

//************************************************************************
int RTComIOTask::new_page()
//************************************************************************
{
#if DEBUG
    cerr << "RTComIOTask::new_page()" << endl;
#endif

    // Move data to beginning of buffer
    if (ibuf && wbuf == ibuf && wptr != wbuf) {
        // Only one buffer is active and whats more, the write pointer
        // does not point to buffer start, meaning that we can make space
        // by moving the data forward. Try this first
        streamsize len = pptr() - wptr;
#if DEBUG
        cerr << "moving " << len << " bytes to buffer start" << endl;
#endif
        memmove(wbuf, wptr, len);
        pbump(wbuf - wptr);
        wptr = wbuf;
    }
    else {
        // Have to allocate new data space

        // If maxOutBuffers != 0, check for max buffers
        if (maxOutBuffers && outBuf.size() == maxOutBuffers) {
            // Buffer is really full
            return EOF;
        }

        ibuf = new char[outBuflen];
#if DEBUG
        cerr << "got new buffer " << (void*)ibuf << endl;
#endif
        outBuf.push_back(ibuf);
        setp(ibuf, ibuf + outBuflen);
        if (!wbuf) {
            wptr = wbuf = ibuf;
        }
    }

    return 0;
}

//************************************************************************
void RTComIOTask::pbump(int n)
//************************************************************************
{
    streambuf::pbump(n);
#if DEBUG
    cerr << "pbump " << n << " " << outBufSize << " " << pptr() - lastPptr << endl;
#endif
    if (lastPptr != pptr()) {
        outBufSize += pptr() - lastPptr;
        lastPptr = pptr();
    }
}

//************************************************************************
void RTComIOTask::setp ( char* pbeg, char* pend )
//************************************************************************
{
    lastPptr = pbeg;
    streambuf::setp(pbeg,pend);
}

//************************************************************************
int RTComIOTask::read(int)
//************************************************************************
{
    int n;
    const char *in = inBuf;
    std::string model;
    bool finished = false;
    size_t left;

    if (inBufPtr == inBufEnd)
        throw Exception("Input buffer full");

    n = ::read(fd, inBufPtr, inBufEnd - inBufPtr);

    if (n <= 0) {
        return n ? errno : 0;
    }

    inBufPtr += n;

    std::cout << "Read " << n << "bytes: " << inBuf << std::endl;

    while (!finished && (left = inBufPtr - in)) {
        switch (parserState) {
            case Init:
                if (in[0] == 'R') {
                    // User requested to enter run state

                    // Is there enough data?
                    if (left < 4) {
                        finished = true;
                        break;
                    }
                    in += 4;

                    // Send the list of currently running models
                    const RTTask::ModelMap& modelMap = rtTask->getModelMap();

                    for( RTTask::ModelMap::const_iterator it = modelMap.begin();
                            it != modelMap.end(); it++) {
                        newApplication(it->first);
                    }

                    parserState = Idle;
                }
                else {
                    in = inBufPtr;
                }
            case Idle:
                if (in[0] == 'Q') {
                    unsigned int nameLen = 0;
                    uint32_t len;
                    const char* querySignature = in;

                    if (left < 12 ||
                            left < 12 + (nameLen = *(uint32_t*)(in + 8))) {
                        finished = true;
                    }

                    std::string modelName(in+12, nameLen);
                    in += 12 + (nameLen/4+1)*4;

                    std::cout << "REply model query " << modelName << 
                        modelName.size() << std::endl;


                    const RTTask::ModelMap& modelMap = rtTask->getModelMap();
                    RTTask::ModelMap::const_iterator it = 
                        modelMap.find(modelName);
                    if (it == modelMap.end()) {
                        len = 8;
                        xsputn((char*)&len, 4);
                        // Send back the first 8 chars which make up the query
                        // signature
                        xsputn(in, 8);
                        break;
                    }

                    const std::vector<uint32_t> sampleTime = 
                        it->second->getSampleTimes();
                    const std::string version =
                        it->second->getVersion();
                    const std::vector<RTVariable*> variables =
                        it->second->getVariableList();
                    uint32_t x;

                    // Packet Len
                    len = 8                     // Query signature
                        + 1                     // 'G'
                        + 4                     // SampleTimeCount
                        + 4*sampleTime.size()   // Individual sample times
                        + 4                     // Variable Count
                        + version.size() + 1;   // Version string incl \0
                    len = htonl(len);
                    xsputn((char*)&len, 4);

                    // Send back the first 8 chars which make up the query
                    // signature
                    xsputn(querySignature, 8);

                    // General parameters
                    sputc('G');

                    // Sample Time Count
                    x = sampleTime.size();
                    xsputn((char*)&x, 4);

                    // Sample Times
                    for (std::vector<uint32_t>::const_iterator it = 
                            sampleTime.begin(); 
                            it != sampleTime.end(); it++) {
                        x = *it;
                        xsputn((char*)&x, 4);
                    }

                    // Variable Count
                    x = variables.size();
                    xsputn((char*)&x, 4);

                    // Version, including trailing \0
                    xsputn(version.c_str(), version.size() + 1);
                    sync();
                }
                else {
                    in = inBufPtr;
                }
                break;
            default:
                finished = true;
                in = inBufPtr;
        }
    }

    memmove(inBuf, in, inBufPtr - in);
    inBufPtr -= in - inBuf;

    return n;
} 
//            else if (listSignals.FullMatch(inBuf, &model)) {
//                unsigned int idx = 0;
//                std::ostringstream oss;
//                std::ostream_iterator<size_t> oo(oss, " ");
//                const RTTask::ModelMap& modelMap = rtTask->getModelMap();
//                RTTask::ModelMap::const_iterator it = modelMap.find(model);
//                if (it == modelMap.end()) {
//                    os.stdErr("Model unknown");
//                    break;
//                }
//                const std::vector<RTVariable*>& variableList =
//                    it->second->getVariableList();
//                std::vector<RTVariable*>::const_iterator vit;
//                os.stdOutListStart("Variable List");
//
//                std::vector<string> key(1);
//                std::vector<string> value(1);
//                key[0] = "VariableCount";
//                oss << variableList.size();
//                value[0] = oss.str();
//                os.stdOutListElement(key, value, true);
//
//                key.resize(7);
//                value.resize(7);
//                key[0] = "Path";
//                key[1] = "Name";
//                key[2] = "Index";
//                key[3] = "Datatype";
//                key[4] = "Dimensions";
//                key[5] = "Flags";
//                key[6] = "SampleTime";
//                for( vit = variableList.begin(); vit != variableList.end(); 
//                        vit++) {
//                    value[0] = (*vit)->getPath();
//                    value[1] = (*vit)->getName();
//                    oss.str("");
//                    oss << idx++;
//                    value[2] = oss.str();
//                    switch ((*vit)->getDataType()) {
//                        case si_double_T:
//                            value[3] = "double_T";
//                            break;
//                        case si_single_T:
//                            value[3] = "single_T";
//                            break;
//                        case si_boolean_T:
//                            value[3] = "boolean_T";
//                            break;
//                        case si_uint8_T:
//                            value[3] = "uint8_T";
//                            break;
//                        case si_sint8_T:
//                            value[3] = "sint8_T";
//                            break;
//                        case si_uint16_T:
//                            value[3] = "uint16_T";
//                            break;
//                        case si_sint16_T:
//                            value[3] = "sint16_T";
//                            break;
//                        case si_uint32_T:
//                            value[3] = "uint32_T";
//                            break;
//                        case si_sint32_T:
//                            value[3] = "sint32_T";
//                            break;
//                        default:
//                            break;
//                    }
//                    const std::vector<size_t> dims = 
//                        (*vit)->getDims();
//                    oss.str("");
//                    copy(dims.begin(), dims.end(), oo);
//                    value[4] = oss.str();
//
//                    value[5] = (*vit)->isWriteable() ? "rw" : "r-";
//
//                    oss.str("");
//                    oss << (*vit)->getSampleTime();
//                    value[6] = oss.str();
//                    os.stdOutListElement(key, value, false);
//                }
//                os.stdOutListEnd();
//            }
//        case LoginInit:
//            {
//                string client_mech;
//                string rest;
//
//                if (auth.PartialMatch(inBuf, &client_mech, &rest)) {
//                    const char* reply;
//                    unsigned int reply_len;
//                    int retval;
//
//                    retval = sasl_server_start(sasl_connection,
//                            client_mech.c_str(), 
//                            rest.length() ? rest.c_str() : NULL, 
//                            rest.length(), 
//                            &reply, &reply_len);
//
//                    switch (retval) {
//                        case SASL_CONTINUE:
//                            cerr << "SASL_CONTINUE" << endl;
//                            parserState = LoginContinue;
//                            break;
//                        case SASL_OK:
//                            cerr << "SASL_OK" << endl;
//                            parserState = Idle;
//                            loggedIn = true;
//                            break;
//                        default:
//                            os.stdErr("Login failure. Reconnect.");
//                            parserState = LoginFail;
//                            inBufEnd = 0;
//                            break;
//                    }
//                }
//                else {
//                    os.stdErr("Did not find AUTH command.");
//                    parserState = Idle;
//                    inBufEnd = 0;
//                }
//                break;
//            }
//        case LoginContinue:
//            {
//                const char* p = inBuf;
//
//                const char* dataStart;
//
//                while ((dataStart = strchr(p,'}'))) {
//                        unsigned int len;
//
//                        if (!dataStart)
//                        break;
//
//                        // Point to location after right brace
//                        dataStart++;
//
//                        if (length.PartialMatch(p, &len)) {
//                            int retval;
//                            const char* reply;
//                            unsigned int reply_len;
//
//                            if (inBufEnd - (dataStart - p) < len) {
//                                inBufEnd -= p - inBuf;
//                                memmove(inBuf, p, inBufEnd);
//                                break;
//                            }
//
//                            string s(dataStart, len);
//
//                            retval = sasl_server_step(sasl_connection,
//                                    s.c_str(), s.length(),
//                                    &reply, &reply_len);
//                            if (reply_len)
//                                os.stdOut(string(reply,reply_len));
//                            break;
//                        }
//                        else {
//                        }
//                }
//                break;
//            }
//        case LoginFail:
//            return -EPERM;
//            break;
//                /*
//                if (!memcmp(inBuf, "SET ", min(4U, inBufPos))) {
//                    parserState = Write;
//                }
//                else if (!memcmp(inBuf, "SUBSCRIBE ", min(10U, inBufPos))) {
//                    parserState = Subscribe;
//                }
//                else if (!memcmp(inBuf, "POLL ", min(5U, inBufPos))) {
//                    parserState = Poll;
//                }
//                else if (!memcmp(inBuf, "USER ", min(5U, inBufPos))) {
//                    parserState = SetUser;
//                }
//                else if (!memcmp(inBuf, "PASS ", min(5U, inBufPos))) {
//                    parserState = SetPass;
//                }
//                else if (!memcmp(inBuf, "LIST", min(4U, inBufPos))) {
//                    parserState = List;
//                }
//                else if (*inBuf == '\n') {
//                    consumed = 1;
//                }
//                else {
//                    consumed = inBufPos;
//                    os.stdErr("Unknown command.");
//                }
//                break;
//                */

