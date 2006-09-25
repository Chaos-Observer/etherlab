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

#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>		// inet_ntop()

#include "buddy_main.h"
#include "net_io.h"
#include "msr_lists.h"

struct net_io {
	int fd;		// File descriptor after accept()
	void *msr_privdata;	// Descriptor returned by msr_open()
			// all subsequent msr_*() calls must pass on this fd
};

/* Network client disconnected itself */
int close_net( void *p)
{
	struct net_io *net_io = (struct net_io *)p;

	// Make sure msr cleans up
	msr_close(net_io->msr_privdata);

	// Remove all references of this file descriptor from the 
	// file descriptor dispatcher
	clr_fd(net_io->fd);

	close(net_io->fd);
	free(net_io);

	return 0;
}

/* Data has arrived from client for msr_*(), i.e. fd is read()able */
int data_for_msr(void *p)
{
	struct net_io *net_io = (struct net_io *)p;
	ssize_t s;

	// msr_read() should do the read() on net_io->fd. msr_read() must
	// return the return value of read() itself
	s = msr_read(net_io->msr_privdata);

	// Check for errors reported by read. Especially, if 0 bytes
	// were received, it means that the client has closed() the connection
	if (s <= 0) {
		close_net(p);
		return (int)s;
	}

	return 0;
}

/* msr_*() has signalised earlier on that it has data for the client.
 * Now the fd is write()able and msr_*() can send its data */
int data_for_client(void *p)
{
	struct net_io *net_io = (struct net_io *)p;
	ssize_t s;

	// msr_write must return the return value of its write() call
	s = msr_write(net_io->msr_privdata);

	// Check whether the write() call reported errors
	if (s <= 0) {
		close_net(p);
		return (int)s;
	}

	return 0;
}

/* This function is called from the file descriptor's read() method when
 * a client connects itself to the server */
int new_msr_client(void *p)
{
	struct net_io *net_io = (struct net_io *)p;

	// Prepare msr for the new connection. Returns a file descriptor
	// which all subsequent msr_*() calls expect as a parameter
	net_io->msr_privdata = msr_open(net_io->fd, net_io->fd);

	// Tell dispatcher which funtions to call when a read() or write()
	// becomes ready
	init_fd(net_io->fd, data_for_msr, data_for_client, p);

	// When this function is called, data has arrived already, so call it
	// immediately without doing another cycle through the dispatcher
	data_for_msr(p);

	return 0;
}

/* This function is called by the read() method of the main loop
 * when a new network connection arrives
 */
int open_net( void *p)
{
	struct net_io *net_io = (struct net_io *)p;
	int fd;
	union {
		struct sockaddr_un un_addr;
		struct sockaddr_in in_addr;
	} sockaddr;
	socklen_t size = sizeof(sockaddr);

	printf("%s: Network connection has new client; client: fd %i\n",
			__FUNCTION__, net_io->fd);

	/* accept() the connection.
	 * if fd < 0, there was a problem in the network layer*/
	if (0 > (fd = accept(net_io->fd, (struct sockaddr *)&sockaddr, 
					&size))) {
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
	/*
	if (fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK)) {
		perror("Error setting filedescriptor flags");
		goto net_out2;
	}
	*/

	if (!(net_io = (struct net_io *)malloc(sizeof(struct net_io)))) {
		perror("malloc");
		goto net_out2;
	}

	/* Setup function pointers for actions to perform when input becomes 
	 * ready, output is ok or network connection is closed
	 */
	net_io->fd = fd;
	init_fd(fd, new_msr_client, close_net, net_io);

	return 0;

net_out2:
	close(fd);
net_out1:
	return -1;
}
int prepare_unix( char *path )
{
	struct net_io *net_io;
	struct sockaddr_un addr;

	printf("%s: Preparing unix socket path: %s\n", __FUNCTION__, path);

	if (strlen(path) > sizeof(addr.sun_path)) {
		printf("Error: Unix domain path too long\n");
		goto unix_out1;
	}

	if (!(net_io = (struct net_io *)malloc(sizeof(struct net_io)))) {
		perror("malloc");
		goto unix_out1;
	}

	unlink(path);	// Ignore system errors
	if (0 > (net_io->fd = socket(PF_UNIX, SOCK_STREAM, 0))) {
		perror("Error creating unix socket");
		goto unix_out2;
	}

	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, path); 	// Length was checked above
	if (bind(net_io->fd, (struct sockaddr *)&addr, SUN_LEN(&addr))) {
		perror("Error binding socket");
		goto unix_out3;
	}
	if (listen(net_io->fd, 5)) {
		perror("Error listening socket");
		goto unix_out3;
	}
	if (fcntl(net_io->fd, F_SETFL, O_RDONLY | O_NONBLOCK)) {
		perror("Error setting socket flags");
		goto unix_out3;
	}

	/* Setup next routines to be called when input becomes ready */
	init_fd(net_io->fd, open_net, NULL, net_io);

	return 0;

unix_out3:
	close(net_io->fd);
unix_out2:
	free(net_io);
unix_out1:
	return -1;
}
int prepare_tcp( int port )
{
	struct net_io *net_io;
	struct sockaddr_in addr;
	int sockopt;

	printf("%s: Preparing tcp socket port: %i\n", __FUNCTION__, port);

	if (!(net_io = (struct net_io *)malloc(sizeof(struct net_io)))) {
		perror("malloc");
		goto tcp_out1;
	}

	if (0 > (net_io->fd = socket(PF_INET, SOCK_STREAM, 0))) {
		perror("Error creating tcp socket");
		goto tcp_out2;
	}

	sockopt = 1;
	if (setsockopt(net_io->fd, SOL_SOCKET, SO_REUSEADDR, 
				&sockopt, sizeof(sockopt))) {
		perror("Error setting socket options");
		goto tcp_out3;
	}

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(net_io->fd, (struct sockaddr *)&addr, sizeof(addr))) {
		perror("Error binding socket");
		goto tcp_out3;
	}
	if (listen(net_io->fd, 5)) {
		perror("Error listening socket");
		goto tcp_out3;
	}

	if (fcntl(net_io->fd, F_SETFL, O_RDONLY | O_NONBLOCK)) {
		perror("Error setting socket flags");
		goto tcp_out3;
	}

	init_fd(net_io->fd, open_net, NULL, net_io);

	return 0;

tcp_out3:
	close(net_io->fd);
tcp_out2:
	free(net_io);
tcp_out1:
	return -1;
}
