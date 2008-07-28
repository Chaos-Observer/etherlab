/* **********************************************************************
 *
 * $Id$
 *
 * This defines the class used to interact with the real-time kernel. Only
 * one instance of this class exists. This class keeps track of all real-time
 * applications that are running, as well as all network clients that are
 * being served with data.
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

#ifndef APPCTLTASK_H
#define APPCTLTASK_H

#include "Task.h"

#include <map>

class Application;

class AppCtlTask: public Task {
    public:
        AppCtlTask();
        ~AppCtlTask();

    private:
        int etl_fd;

        typedef std::map<unsigned int,Application*> ApplicationMap;
        ApplicationMap appMap;

        // Reimplemented from class Task
        int read(int fd);
        void kill(Task*, int);
};
#endif // APPCTLTASK_H
