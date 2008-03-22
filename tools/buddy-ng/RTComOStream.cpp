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

#undef DEBUG
#define DEBUG 0

using namespace std;

//************************************************************************
RTComOStream::RTComOStream(streambuf* sb): ostream(sb)
//************************************************************************
{
}

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
void RTComOStream::eventStream(const char* s, size_t n)
//************************************************************************
{
    *this << "+EVENT" << endl;
}

//************************************************************************
void RTComOStream::dataStream(unsigned int decimation, 
        const char* s, size_t n)
//************************************************************************
{
}
