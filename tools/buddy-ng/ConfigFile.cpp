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


#include "ConfigFile.h"

#include <cstring>
#include <iostream>

using namespace std;

//************************************************************************
ConfigFile::ConfigFile(const char* filename)
//************************************************************************
{
    dict = iniparser_load(filename);
    cerr << "loaded file " << filename << dict << endl;
}

//************************************************************************
ConfigFile::~ConfigFile()
//************************************************************************
{
    iniparser_freedict(dict);
}

//************************************************************************
bool ConfigFile::getBool(const string& section, const string& entry, 
        bool def)
//************************************************************************
{
    string key(section);
    key.append(":").append(entry);
    return iniparser_getboolean(dict, key.c_str(), def);
}

//************************************************************************
double ConfigFile::getDouble(const string& section, const string& entry, 
        double def) throw(bad_alloc)
//************************************************************************
{
    char* nkey;
    double value;

    string key(section);
    key.append(":").append(entry);

    nkey = strdup(key.c_str());
    if (!nkey)
        throw bad_alloc();

    value = iniparser_getdouble(dict, nkey, def);
    free(nkey);

    return value;
}

//************************************************************************
int ConfigFile::getInt(const string& section, const string& entry, 
        int def)
//************************************************************************
{
    string key(section);
    key.append(":").append(entry);
    return iniparser_getint(dict, key.c_str(), def);
}

//************************************************************************
string ConfigFile::getString(const string& section, const string& entry, 
        const string& def) throw(bad_alloc)
//************************************************************************
{
    char* def_str;

    def_str = strdup(def.c_str());
    if (!def_str)
        throw bad_alloc();

    string key(section);
    key.append(":").append(entry);
    string value = iniparser_getstring(dict, key.c_str(), def_str);
    free(def_str);
    return value;
}
