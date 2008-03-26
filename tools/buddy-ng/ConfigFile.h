/* **********************************************************************
 *  $Id$
 *
 * **********************************************************************/

/**
 *
 * @note Copyright (C) 2008 Richard Hacker
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

#ifndef CONFIGFILE_H
#define CONFIGFILE_H

#include <string>

extern "C" {
#include "iniparser.h"
}

class ConfigFile {
    public:
        ConfigFile(const char* filename);

        static std::string getString(const std::string& section, 
                const std::string& entry, const std::string& def = "")
            throw(std::bad_alloc);
        static double getDouble(const std::string& section, 
                const std::string& entry, double def = 0) 
            throw(std::bad_alloc);
        static bool getBool(const std::string& section, 
                const std::string& entry, bool def = false);
        static int getInt(const std::string& section, 
                const std::string& entry, int def = 0);

    private:
        static dictionary* dict;
        static std::string makeKey(const std::string& section, 
                const std::string& entry);
};

#endif // CONFIGFILE_H
