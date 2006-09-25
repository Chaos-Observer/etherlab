/**
 * @ingroup buddy
 * @file
 * Main file implementing the dispatcher
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


/*
 *  $RCSfile: rtai_main.c,v $
 *  $Revision: 1.1 $
 *  $Author: rich $
 *  $Date: 2006/02/11 00:28:57 $
 *  $State: Exp $
 *
 *  Autor: Richard Hacker
 *
 *  License: GPL
 *  (C) Copyright IgH 2005
 *  Ingenieurgemeinschaft IgH
 *  Heinz-Bäcker Str. 34
 *  D-45356 Essen
 *  Tel.: +49 201/61 99 31
 *  Fax.: +49 201/61 98 36
 *  E-mail: info@igh-essen.com
 *
 *  $Log: rtai_main.c,v $
 *  Revision 1.1  2006/02/11 00:28:57  rich
 *  Initial revision
 */ 

/** 
@defgroup buddy User Buddy Process

The @ref buddy is a process that runs in user space and forms the connection
between the real world and the Real-Time Process that runs in RTAI context. 

Essentially this is a general implementation around the @c select(2) function
call, dispatching external communication to various registered callback funtions 
when @c select(2) reports that read or write will not block on a file descriptor.
As @c select(2) is the central functionality, be sure that you fully understand 
it. Refer to the manpage if you have doubts.

This technique is a very advanced feature that does not need multitasking,
making it very efficient as it does not require task switches and interprocess
communication. Everything is contained within the context of one process.

During initialisation of the @ref buddy, various modules are initialised,
which in turn open files to perform their respective duties. During the 
initialisation of these modules, they register their functions that they wish
to be called back when the file descriptor is ready to @c read(2) or @c write(2).
For example the @ref netio module opens a tcp server socket and waits for 
incoming connections. When that happens, the file descriptor is ready to read.
The central dispatcher then calls the function that was registered when 
@c read(2) becomes ready.
*/

#include <stdio.h>		// printf
#include <stdlib.h>		// malloc, free
#include <unistd.h>		// close(), unlink(), sysconf()
#include <string.h>		// memset()
#include <sys/select.h>		// select(), FD_SET, FD_CLR, fd_isset...
#include <errno.h>		// ENOMEM, EBADF

#include "buddy_main.h"

//#include "file_io.h"
#include "rtp_io.h"
#include "net_io.h"

/**
 * \struct fd_set
 * Private data needed to be able to use @c select(2).
 */
struct fd_set {
    fd_set rfd; /**< Read file descriptors */
    fd_set wfd; /**< Write file descriptors */
};

/** Constant FD_SET used to initialise @ref out_fd_set. 
 * This is the main file descriptor set as @c select(2) requires it, and is 
 * used to initialise @ref out_fd_set just before calling @e select(2).
 */
struct fd_set init_fd_set;

/** FD_SET after call to @c select(2). 
 * This is the fd_set after the call to @c select(2)
 */
struct fd_set out_fd_set;
int select_max;	/**< Max file descriptors @c select(2) has to watch */

/**
 * Callback functions that were registered
 *
 * One such structure is allocated for every file descriptor that is watched
 * by @c select(2). When that file descriptor becomes ready, these functions
 * are called, passing @e priv_data as a parameter.
 */
struct callback {
	int (*read)(void *);  /**< Callback when @c read(2) will not block */
	int (*write)(void *); /**< Callback when @c write(2) will not block */
	void *priv_data;      /**< Data that gets passed to callback */
};

/**
 * Base pointer to the list of callbacks.
 *
 * This points to the start of an array of @ref callback pointers. During 
 * initialisation, memory is allocated for the maximum number of open file
 * descriptors a process can have.
 *
 * Upon registration of callbacks, space is allocated for @ref callback. The
 * pointer to this space is saved in @ref callback_list for the specific
 * file desriptor.
 */
struct callback **callback_list;

int init_fd(int fd, int (*read)(void *), int (*write)(void *), void *priv_data)
/* Purpose:	Sets up callback[] and struct select_vec for a new file
 * 		and also read, write callbacks, and callback private data
 */
{
	struct callback *f;

	if (!(f = callback_list[fd])) {
		// First time fd is used. Allocate a new structure
		if (!(f = (struct callback *)malloc(sizeof(struct callback)))) {
			return -ENOMEM;
		}

                // Set up the mapping from fd to callback*
		callback_list[fd] = f;

		// Make sure select_max is correct
		if (fd >= select_max) {
			select_max = fd+1;
			printf("Select max is %i\n", select_max);
		}

	}

        if (read) {
            // File is new; only setup read channels to accept commands
            FD_SET(fd, &init_fd_set.rfd );
        } else {
            FD_CLR(fd, &init_fd_set.rfd);
            FD_CLR(fd, &out_fd_set.rfd);
        }

        if (!write)
            clr_wfd(fd);

	f->read = read;
	f->write = write;
	f->priv_data = priv_data;

	return 0;
}

int set_wfd(int fd)
/* Purpose:	 Enable write channel. Used if client wants to send data
 */
{
    if (!callback_list[fd]->write)
        return -EBADF;

    FD_SET(fd, &init_fd_set.wfd );
    return 0;
}

void clr_wfd(int fd)
/* Purpose: 	Disable write channel. Used if client does not have more data
 */
{
	FD_CLR(fd, &init_fd_set.wfd);
	FD_CLR(fd, &out_fd_set.wfd);
}

void clr_fd(int fd)
/* Purpose:	 Disable all io channels to a data client
 */
{
	struct callback *f = callback_list[fd];
	int i;

	if (!f)
		return;

	FD_CLR(fd, &init_fd_set.rfd);
	FD_CLR(fd, &init_fd_set.wfd);
	FD_CLR(fd, &out_fd_set.rfd);
	FD_CLR(fd, &out_fd_set.wfd);

	/* Check whether max file descriptors for select can be reduced */
	if (select_max - 1 == fd) {
		i = select_max - 2;

		// Reduce i while there are callbacks registered for a 
		// file descriptor in callback[] table
		while ( i >= 0 && !callback_list[i] )
			i--;

		select_max = i+1;

		printf("\t select max is now %i\n", select_max);
	}

	free(f);

	callback_list[fd] = NULL;
}


void file_init(void) 
/* Purpose:	go into a select() loop, calling appropriate client actions
 * 		when either data from has arrived, or data must be sent to 
 * 		client
 * scope:	private
 */
{
	int file_max;
	size_t s;

	/* Allocate memory for fd_client. This array must be large enough
	 * to accomodate all file descriptors a process can have. So first
	 * find out max open files */
	if (0 > (file_max = sysconf(_SC_OPEN_MAX))) {
		perror("Error obtaining max open files");
		exit(1);
	}
	s = file_max*sizeof(struct callback *);
	if (!(callback_list = malloc(s))) {
		perror("Error malloc");
		exit(1);
	}
	memset(callback_list, 0, s);

	/* Initialise file descriptor sets for select() */
	FD_ZERO(&init_fd_set.rfd);
	FD_ZERO(&init_fd_set.wfd);
	select_max = 0;

	/* Open initial communication channels to enable data clients
	 * to attach to this data server
	 */
	if (
	    //prepare_pipe("/tmp/p") ||
	    prepare_unix("/tmp/msr_sock") ||
	    prepare_tcp(2345) ||
	    prepare_rtp("/dev/etl") //FIFO(RTP_FIO), FIFO(RTB_FIO))
	    )
		exit(1);
}

/*
 * This is the main loop. Using a select() call, data is awaited from 
 * various input streams.  When data has arrived, the appropriate read()
 * function of the input stream is called to process the data.  Processing
 * the input may (or usually) causes output data to be sent to the clients.
 * Once these file descritors are ready to be written to, the appropriate
 * write() function of the stream is called.
 */
int main(void)
{
	int fd_count,	// Number of ready file descriptors from select()
		fd;	// Temporary fd
	struct callback *cb;

	file_init();	// Initialise all file descriptors

	// Main never ending loop around select()
	while (1) {
		/* Call select() */
		out_fd_set = init_fd_set;
		fd_count = select(
				select_max, 
				&out_fd_set.rfd,
				&out_fd_set.wfd,
				NULL,
				NULL
			  );

		/* Allways expect at least 1 fd to be ready.
		 * Otherwise a signal was sent */
		if (fd_count <= 0)
			break;

		fd = 0;
		/* There are fd_count ready file discriptors.
		 * Find them and process
		 */
		while (fd_count && fd < select_max ) {
				
			/* Has data arrived from client? */
			if (FD_ISSET(fd, &out_fd_set.rfd)) {
				fd_count--;
				//printf("fd %i is ready for read\n",fd);

				// Find client that ownes file desriptor
				cb = callback_list[fd];

				// Call appropriate client function
				cb->read(cb->priv_data);
			}

			/* Is output channel to client ready? */
			if (FD_ISSET(fd, &out_fd_set.wfd)) {
				fd_count--;
				//printf("fd %i is ready for write\n",fd);

				// Find client that ownes file desriptor
				cb = callback_list[fd];

				// Call appropriate client function
				cb->write(cb->priv_data);
			}
			fd++;
		}
	}

	return 0;
}
