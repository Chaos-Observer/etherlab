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
#include <sys/stat.h>		// umask()
#include <sys/wait.h>		// wait()
#include <errno.h>		// ENOMEM, EBADF
#include <getopt.h>
#include <signal.h>

#include "buddy_main.h"

#include "modules.h"

int debug = 0;
int foreground = 0;
char *cwd;
int argc;
char * const *argv;
int file_max;

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
	void (*read)(int, void *);  /**< Callback when @c read(2) will not block */
	void (*write)(int, void *); /**< Callback when @c write(2) will not block */
	void *priv_data;      /**< Data that gets passed to callback */
};

struct sigchild_list {
    struct sigchild_list *next;

    int pid;
    void (*call)(void *);
    void *priv_data;
} *sigchild_list;

int 
reg_sigchild(int pid, void *priv_data) {
    struct sigchild_list *sc;

    if (!(sc= malloc(sizeof(struct sigchild_list)))) {
        return -errno;
    }

    sc->pid = pid;
    sc->priv_data = priv_data;
    sc->next = sigchild_list;
    sigchild_list = sc;

    return 0;
}

void
sig_handle(int signum)
{
    pid_t pid;
    int status;
    struct sigchild_list *sc;

    switch (signum) {
        case SIGCHLD:
            pid = wait(&status);

            printf("Child %i was killed\n", pid);

            for (sc = sigchild_list; sc; sc = sigchild_list->next) {
                if (sc->pid == pid)
                    (*sc->call)(sc->priv_data);
            }
            break;

        case SIGTERM:
            printf("Buddy was killed\n");
            kill(0, SIGTERM);
            exit(-SIGTERM);

        default:
            printf("Unknown signal cought\n");
            exit(-signum);
    }

}

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

int init_fd(int fd, 
        void (*read)(int, void *), 
        void (*write)(int, void *), 
        void *priv_data)
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


void init_modules(void)
/* Purpose:	go into a select() loop, calling appropriate client actions
 * 		when either data from has arrived, or data must be sent to 
 * 		client
 * scope:	private
 */
{
	/* Allocate memory for fd_client. This array must be large enough
	 * to accomodate all file descriptors a process can have. So first
	 * find out max open files */
	if (!(callback_list = calloc(file_max, sizeof(struct callback *)))) {
		perror("Error calloc");
		exit(1);
	}

	/* Initialise file descriptor sets for select() */
	FD_ZERO(&init_fd_set.rfd);
	FD_ZERO(&init_fd_set.wfd);
	select_max = 0;

	/* Open initial communication channels to enable data clients
	 * to attach to this data server
	 */
	if (0
//	    || command_module_prepare()
	    || rtp_module_prepare()
	    )
		exit(1);
}

void 
printhelp(const char *name)
{
    printf("%s: This is the user-space helper for the RT Kernel\n"
            "The main program accepts the following options:\n"
            "    -f, --foreground\n"
            "\tDon't fork; stay in foreground\n"
            "    -d, --debug level\n"
            "\tSet the debug level 0-7\n"
            "\n"
            "The following options are available for each module.\n\n",
            name);
    command_module_help();
}

void
parse_args(void)
{
    int c;
    struct option longopts[] = {
        {"help", 0, 0, 'h'},
        {"foreground", 0, 0, 'n'},
        {"debug", 1, 0, 'd'},
        {},
    };

    optind = 0;
    while ((c = getopt_long(argc, argv, ":hnd:", longopts, NULL)) != -1) {
        switch (c) {
            case 'n':
                foreground = 1;
                break;
            case 'd':
                debug += atoi(optarg);
                break;
            case ':':
                fprintf(stderr, "Argument for option -%c missing\n", optopt);
                exit(-1);
            case 'h':
                printhelp(argv[0]);
                exit(0);
        }
    }
}

/*
 * This is the main loop. Using a select() call, data is awaited from 
 * various input streams.  When data has arrived, the appropriate read()
 * function of the input stream is called to process the data.  Processing
 * the input may (or usually) causes output data to be sent to the clients.
 * Once these file descritors are ready to be written to, the appropriate
 * write() function of the stream is called.
 */
//#include <fcntl.h>
int main(int ac, char **av)
{
	int fd_count,	// Number of ready file descriptors from select()
		fd;	// Temporary fd
	struct callback *cb;
        int pid;
        struct sigaction sa;

        argc = ac;
        argv = av;

	if (0 > (file_max = sysconf(_SC_OPEN_MAX))) {
		perror("Error obtaining max open files");
		exit(1);
	}

        opterr = 0;
        if (!(cwd = getcwd(NULL, 0))) {
            perror("getcwd()");
            return -errno;
        }

        parse_args();

        sa.sa_handler = sig_handle;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_NOCLDSTOP;
        if ( sigaction(SIGCHLD, &sa, NULL)) {
            perror("sigaction()");
            return -errno;
        }

        /* Check if we can go into background */
        if (!foreground) {
            /* Close standard file descriptors */
            fclose(stdin);
            fclose(stdout);
            fclose(stderr);

            /* Fork here */
            if ((pid = fork()) < 0) {
                perror("fork()");
                return -errno;
            } else if (pid != 0) {
                /* We're the parent here, so get out */
                return 0;
            } else {
                /* We're the child */
                setsid();           // Session leader
                chdir("/");         // Don't sit around in directories
                umask(0);           // For files we create 
            }
        }

        init_modules(); // Initialise all file descriptors


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
		if (fd_count == -1) {
                    /* Interrupted system call is ok, go back to the top */
                    if (errno == EINTR) 
                        continue;

                    /* Don't know this error, bale out */
                    perror("select() error");
                    fprintf(stderr, "Aborting\n");
                    break;
                } else if (fd_count == 0) {
                    fprintf(stderr, "%s:select() returned 0, don't know why\n",
                            __FILE__);
                    break;
                }

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
				(*cb->read)(fd, cb->priv_data);
			}

			/* Is output channel to client ready? */
			if (FD_ISSET(fd, &out_fd_set.wfd)) {
				fd_count--;
				//printf("fd %i is ready for write\n",fd);

				// Find client that ownes file desriptor
				cb = callback_list[fd];

				// Call appropriate client function
				(*cb->write)(fd, cb->priv_data);
			}
			fd++;
		}
	}
        printf("Phew, finished, but don't know why!\n");

        /* Kill everything in this process group */
        kill(0, SIGTERM);

	return 0;
}

