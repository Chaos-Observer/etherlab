/** 
 * @ingroup buddy
 * @file 
 *
 * Header to include when using the file dispatcher.
 * @author Richard Hacker
 *
 * @note Copyright (C) 2006 Richard Hacker
 * <lerichi@gmx.net>
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

/* $RCSfile: rtai_main.h,v $
 * $Revision: 1.1 $
 * $Author: rich $
 * $Date: 2006/02/13 23:05:45 $
 * $State: Exp $
 *
 * $Log: rtai_main.h,v $
 * Revision 1.1  2006/02/13 23:05:45  rich
 * Initial revision
 *
 */

#ifndef _BUDDY_MAIN_H
#define _BUDDY_MAIN_H

/* Command line variables */
extern int argc;
extern char * const *argv;

/* Maximum number of open files */
extern int file_max;

/* Run in foreground */
extern int foreground;

/* Directory from where buddy was started */
extern char *cwd;

/** Register a file handle with the dispatcher. 
 *
 * This function is used to register callbacks with the main dispatcher
 * in case a file handle becomes ready to read (\c read(2) will not block) or
 * ready to write (\c write(2) will not block). These callbacks will be called,
 * passing \e priv_data as a parameter. In case a NULL is registered for
 * \e read or \e write, the appropriate file desriptor is not considered in the 
 * \c select(2) call.
 *
 * Calling this with \e read not NULL will immediately enable the read callback.
 * If \e write is not NULL, it still has to be enabled with
 * \ref set_wfd.
 *
 * Recalling this with the same \e fd is allowed, thereby redefining the callback
 * functions.
 *
 * \return 
 *   \arg \c 0: No error
 *   \arg \c -ENOMEM: No memory is available for \c malloc()
 *
 * \sa set_wfd clr_wfd clr_fd
 */
int init_fd(
        int fd,                 /**< The file discriptor for which the 
                                  * callbacks are registered. */
        void (*read)(int, void *),/**< Method to be called when \a fd becomes 
                                  * readable. Can be NULL*/
        void (*write)(int, void *),/**< Method to be called when \a fd becomes 
                                  * writeable Can be NULL*/
        void *priv_data         /**< Argument that is passed when calling 
                                  * registered callback */
        );

/**
 * Enable write callback.
 *
 * Calling this function will consider the \e write callback of \e fd during 
 * the next call to \c select(2) call. 
 *
 * Setting a write callback with \ref init_fd does not immediately enable it
 * when calling \c select(2). This is because usually during
 * initialisation of a server, output data is not yet available. When data
 * does become available, the application has to use this function to tell 
 * the dispatcher that it has data to be sent.
 *
 * \return
 *   \arg \c 0: No error
 *   \arg \c -EBADF: \e write callback was not defined in \ref init_fd
 */
int set_wfd(
        int fd      /**< Descriptor for which the \e write callback is 
                      * activated */
        );

/**
 * Disable write callback.
 *
 * Calling this function will disable the \e write callback of \e fd during 
 * future calls to \c select(2).
 *
 * This is used when the application has no more data to send.
 */
void clr_wfd(
        int fd      /**< Descriptor for which the \e write callback is 
                      * deactivated */
        );

/**
 * Disable and free all callbacks.
 *
 * Calling this function will disable all callbacks for \e fd. Additionally
 * all internal stuctures are removed.
 */
void clr_fd(
        int fd  /**< Descriptor to free */
        );

#endif

