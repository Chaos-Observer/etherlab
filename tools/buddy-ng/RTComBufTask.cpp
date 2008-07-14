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

#undef DEBUG
#define DEBUG 1

#include <iostream>
#include <arpa/inet.h>

using namespace std;

//************************************************************************
RTComIOTask::RTComIOTask(Task* parent, int _fd, 
        unsigned int _buflen, unsigned int _maxbuf): 
    Task(parent), streambuf(), fd(_fd), buflen(_buflen), max_buffers(_maxbuf)
//************************************************************************
{
    reset();
}

//************************************************************************
RTComIOTask::~RTComIOTask()
//************************************************************************
{
}

//************************************************************************
void RTComIOTask::reset()
//************************************************************************
{
    for (list<char*>::iterator it = buf.begin(); it != buf.end(); it++)
        delete[] *it;
    buf.clear();
    wptr = wbuf = ibuf = 0;
    bufSize = 0;
    setp(0,0);
    disableWrite();
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

void RTComIOTask::send(const std::string& s)
{
    char padchars[4];
    streamsize pad = 4 - s.length() % 4;
    uint32_t len = htonl(s.length() + pad);
    xsputn((char*)&len, sizeof(len));
    xsputn(s.c_str(), s.length());
    xsputn(padchars, pad);
    sync();
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
                bufSize -= len;
#if DEBUG
                cerr << "decreasing 1 bufSize " << len;
#endif
            }
        }
    } else {
        /* Writing and reading on different buffers */
#if DEBUG
        cerr << "Sending " << wbuf + buflen - wptr
            << " Bytes from different buffers " << (void*)wbuf << endl;
#endif
        len = ::write(fd, wptr, wbuf + buflen - wptr);
        if (len > 0) {
            wptr += len;
            bufSize -= len;
#if DEBUG
            cerr << "decreasing 2 bufSize " << len;
#endif
            if (wbuf + buflen == wptr) {
                /* Finished sending wbuf */
#if DEBUG
                cerr << "deleting buffer " << (void*)wbuf << endl;
#endif
                delete wbuf;
                buf.pop_front();
                wptr = wbuf = buf.front();
            }
        }
    }

    if (!bufSize)
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

        // If max_buffers != 0, check for max buffers
        if (max_buffers && buf.size() == max_buffers) {
            // Buffer is really full
            return EOF;
        }

        ibuf = new char[buflen];
#if DEBUG
        cerr << "got new buffer " << (void*)ibuf << endl;
#endif
        buf.push_back(ibuf);
        setp(ibuf, ibuf + buflen);
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
    cerr << "pbump " << n << " " << bufSize << " " << pptr() - lastPptr << endl;
#endif
    if (lastPptr != pptr()) {
        bufSize += pptr() - lastPptr;
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
