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

#include "RTComOStream.h"

#include <iostream>

#undef DEBUG
#define DEBUG 0

using namespace std;

//************************************************************************
RTComOStream::RTComOStream(streambuf* sb): ostream(sb)
//************************************************************************
{
}

//************************************************************************
void RTComOStream::hello()
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

    // Set version to 1.0
    *(unsigned int *)&s[8] = 1;
    *(unsigned int *)&s[12] = 0;

    std::ostream::write((char*)s, sizeof(s)).flush();
}

//************************************************************************
std::ostream& RTComOStream::write(const char* s, std::streamsize n)
//************************************************************************
{
    uint32_t len = n;
    return std::ostream::write((const char*)&len, 4).write(s, n).flush();
}

////************************************************************************
//void RTComOStream::sendInt(unsigned int n)
////************************************************************************
//{
//    uint32_t i = htonl(n);
//    this->write(&i, 4);
//}

//************************************************************************
void RTComOStream::stdOut(const std::string& s)
//************************************************************************
{
    *this << "+OK " << s << endl;
}

//************************************************************************
void RTComOStream::stdErr(const std::string& s)
//************************************************************************
{
    *this << "-ERR " << s << endl;
}

//************************************************************************
void RTComOStream::processLog(const std::string& s)
//************************************************************************
{
}

//************************************************************************
void RTComOStream::eventStream(const std::string& s)
//************************************************************************
{
    *this << "=EVENT " << s << endl;
}

//************************************************************************
void RTComOStream::dataStream(unsigned int decimation, 
        const char* s, size_t n)
//************************************************************************
{
}

//************************************************************************
void RTComOStream::stdOutListStart(const std::string& title)
//************************************************************************
{
    *this << "+OK " << title << ":\n";
}

//************************************************************************
void RTComOStream::stdOutListElement(const std::vector<std::string>& key,
        const std::vector<std::string>& value, bool first)
//************************************************************************
{
    if (!first)
        *this << '\n';
    for (unsigned int i = 0; i < key.size(); i++)
        *this << key[i] << ": " << value[i] << '\n';
}

//************************************************************************
void RTComOStream::stdOutListEnd()
//************************************************************************
{
    *this << ".\n";
    this->flush();
}

