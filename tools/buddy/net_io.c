/******************************************************************************
 *
 *           $RCSfile: msr_io.c,v $
 *           $Revision: 1.5 $
 *           $Author: ha $
 *           $Date: 2005/09/16 14:51:06 $
 *           $State: Exp $
 *
 *           Autor: Richard Hacker
 *
 *           (C) Copyright IgH 2005
 *           Ingenieurgemeinschaft IgH
 *           Heinz-Bäcker Str. 34
 *           D-45356 Essen
 *           Tel.: +49 201/61 99 31
 *           Fax.: +49 201/61 98 36
 *           E-mail: info@igh-essen.com
 *
 *           $Log:$
 *
 */ 
/*--includes-----------------------------------------------------------------*/

#include <stdio.h>		// printf
#include <stdlib.h>		// malloc, free
#include <unistd.h>		// close(), unlink(), sysconf()
#include <fcntl.h>		// open(), O_*, fcntl()
#include <errno.h>


#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>		// inet_ntop()

#include "net_io.h"

struct net_io {

    void *(*open)(int, void *);
    void (*read)(int, void *);
    void (*write)(int, void *);

    void *priv_data;
};

/* Network client disconnected itself */
int close_net(int fd)
{
	// Remove all references of this file descriptor from the 
	// file descriptor dispatcher
	clr_fd(fd);

	close(fd);

	return 0;
}

/* This function is called by the read() method of the main loop
 * when a new network connection arrives
 */
void new_connection( int net_fd, void *p)
{
	struct net_io *net_io = (struct net_io *)p;
        void *priv_data;
        int fd;
	union {
		struct sockaddr_un un_addr;
		struct sockaddr_in in_addr;
	} sockaddr;
	socklen_t size = sizeof(sockaddr);

	printf("%s: Network connection has new client; client: fd %i\n",
			__FUNCTION__, net_fd);

	/* accept() the connection.
	 * if fd < 0, there was a problem in the network layer*/
	fd = accept(net_fd, (struct sockaddr *)&sockaddr, &size);
	if (fd < 0) {
		perror("Error accepting from net socket");
		goto net_out1;
	}

	/* Check whether accept() even put anything into sockaddr at all!
	 * This should never happen, but you never know...
	 */
	if (!size) {
		printf("Unknown AF in %s:%s():%i\n", 
				__FILE__, __FUNCTION__, __LINE__);
		goto net_out2;
	}

	/* Make sure file does not block */
	if (fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK)) {
		perror("Error setting filedescriptor flags");
		goto net_out2;
	}

	/* Setup function pointers for actions to perform when input becomes 
	 * ready, output is ok or network connection is closed
	 */
	priv_data = net_io->open(fd, net_io->priv_data);

	// Tell dispatcher which funtions to call when a read() or write()
	// becomes ready
	init_fd(fd, net_io->read, net_io->write, priv_data);

	return;

net_out2:
	close(fd);
net_out1:
	return;
}

/* Now that the address family dependent network preparation is finished, 
 * complete the independent stuff */
int prepare_post(
        int fd,
        struct sockaddr *addr,
        socklen_t sun_len,
        void *(*open_cb)(int, void *),
        void (*read_cb)(int, void*), 
        void (*write_cb)(int, void*), 
        void *client_priv_data
        ) 
{
    
    struct net_io *net_io;

    if (bind(fd, addr, sun_len)) {
            perror("Error binding socket");
            return -1;
    }

    if (listen(fd, 5)) {
            perror("Error listening socket");
            return -1;
    }

    if (fcntl(fd, F_SETFL, O_RDONLY | O_NONBLOCK)) {
            perror("Error setting socket flags");
            return -1;
    }

    if (!(net_io = (struct net_io *)malloc(sizeof(struct net_io)))) {
            perror("malloc");
            return -1;
    }

    net_io->open = open_cb;
    net_io->read = read_cb;
    net_io->write = write_cb;
    net_io->priv_data = client_priv_data;

    /* Setup next routines to be called when input becomes ready */
    init_fd(fd, new_connection, NULL, net_io);

    return 0;
}

int prepare_unix(const char *path, 
        void *(*open_cb)(int, void *),
        void (*read_cb)(int, void*), 
        void (*write_cb)(int, void*), 
        void *client_priv_data
        )
{
	struct sockaddr_un addr;
        int fd;

	printf("%s: Preparing unix socket path: %s\n", __FUNCTION__, 
                path);

	if (unlink(path) && errno != ENOENT) {
                fprintf(stderr, "Error while removing %s: ", path);
                perror(NULL);
		goto unix_out1;
        }
	if ((fd = socket(PF_UNIX, SOCK_STREAM, 0)) < 0) {
		perror("Error creating unix socket");
		goto unix_out2;
	}

	addr.sun_family = AF_UNIX;
	if (!memccpy(addr.sun_path, path, '\0', 
                    sizeof(addr.sun_path))) {
		fprintf(stderr, "Error: Socket path (%s) too long\n", 
                        path);
		goto unix_out2;
        }

        if (prepare_post(fd, (struct sockaddr *)&addr, SUN_LEN(&addr),
                open_cb, read_cb, write_cb, client_priv_data)) {
            goto unix_out2;
        }

	return 0;

unix_out2:
	close(fd);
unix_out1:
	return -1;
}

int prepare_tcp( 
        int port,
        void *(*open_cb)(int, void *),
        void (*read_cb)(int, void*), 
        void (*write_cb)(int, void*), 
        void *client_priv_data
        )
{
	struct sockaddr_in addr;
        int fd;
	int sockopt;

	printf("%s: Preparing tcp socket port: %i\n", __FUNCTION__, port);

	if (0 > (fd = socket(PF_INET, SOCK_STREAM, 0))) {
		perror("Error creating tcp socket");
		goto tcp_out1;
	}

	sockopt = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, 
				&sockopt, sizeof(sockopt))) {
		perror("Error setting socket options");
		goto tcp_out2;
	}

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

        if ( prepare_post(fd, (struct sockaddr *)&addr, sizeof(addr),
                open_cb, read_cb, write_cb, client_priv_data)) {
            goto tcp_out2;
        }

	return fd;

tcp_out2:
	close(fd);
tcp_out1:
        return -1;
}
