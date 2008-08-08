/******************************************************************************
 *
 * $Id$
 *
 * This is the header file for the RTCom character device interface
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
 *
 *****************************************************************************/ 

#include "rt_main.h"

/* Called when a new app is registered */
int rtcom_new_app(struct app *app);

/* Called when a app is removed */
void rtcom_del_app(struct app *app);

/* Clear the Real-Time AppCore file handles */
void rtcom_fio_clear(void);

/* Set up the Real-Time AppCore file handles. This is called once when
 * the rt_appcore is loaded, and opens up the char device for 
 * communication between buddy and rt_appcore. */
int rtcom_fio_init(void);
