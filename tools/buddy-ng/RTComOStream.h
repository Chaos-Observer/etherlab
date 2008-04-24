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

#ifndef RTCOMOSTREAM_H
#define RTCOMOSTREAM_H

#include "Task.h"

#include <ostream>
#include <vector>

class RTComOStream: public std::ostream {
    public:
        RTComOStream(std::streambuf*);

        void stdOut(const std::string& s);
        void stdErr(const std::string& s);
        void processLog(const std::string& s);
        void eventStream(const std::string& s);
        void dataStream(unsigned int decimation, const char* s, size_t n);

        void stdOutListStart(const std::string& title);
        void stdOutListElement(const std::vector<std::string>& key,
                const std::vector<std::string>& value,
                bool first);
        void stdOutListEnd();

        void hello();
        std::ostream& write( const char* s, std::streamsize n);

};

#endif // RTCOMOSTREAM_H
