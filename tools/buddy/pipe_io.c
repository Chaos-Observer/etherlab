/******************************************************************************
 *
 *           $RCSfile: msr_io.c,v $
 *           $Revision$
 *           $Author$
 *           $Date$
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
#include <string.h>		// memset()
#include <fcntl.h>		// open(), O_*, fcntl()
#include <sys/select.h>		// select(), FD_SET, FD_CLR, fd_isset...
#include <sys/stat.h>		// mkfifo()

#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>		// inet_ntop()

//#include "file_io.h"
//#include "client_io.h"
//#include "rtp_io.h"
#include "fifo_defines.h"
#include "buddy_main.h"
#include "rtp_io.h"

int open_pipe( struct file_io *filp );
int close_pipe( struct file_io *filp)
{
	clr_fd(filp);
	close(filp->rfd);
	close(filp->wfd);

	if (open_pipe(filp))
		return -1;

	return 0;
}
int open_pipe( struct file_io *filp )
{
	char *path = (char *)filp->file_data;

	printf("%s: Preparing %s\n", __FUNCTION__, filp->name);

	unlink(path);	// Ignore errors

	if (mkfifo(path, 0666)) {
		perror("Error creating pipe");
		goto pipe_out;
	}

	if( (filp->rfd = open(path, O_RDONLY | O_NONBLOCK)) < 0) {
		perror("Error opening pipe for read");
		goto pipe_out;
	}

	if( (filp->wfd = open(path, O_WRONLY | O_NONBLOCK)) < 0) {
		perror("Error opening pipe for write");
		goto pipe_out1;
	}

	init_fd(filp);

	/* Setup next routines to be called when input becomes ready */
	filp->close = close_pipe;
	filp->read = new_msr_client;
	filp->write = NULL;

	return 0;

pipe_out1:
	close(filp->rfd);
pipe_out:
	return -1;
}
int prepare_pipe( char *path )
{
	struct file_io *filp;
	char *title = "PIPE, Path = ";

	if (NULL == (filp = (struct file_io *)
				malloc(sizeof(struct file_io)))) {
		perror("Couldn't allocate space for pipe file_io");
		goto pipe_out;
	}

	if (!(filp->file_data = strdup(path))) {
		perror("Couldn't allocate space for pipe filename");
		goto pipe_out1;
	}

	if (!(filp->name = (char *)malloc(strlen(path)+strlen(title)+1))) {
		perror("Couldn't allocate space for pipe title");
		goto pipe_out2;
	}
	strcpy(filp->name, title);
	strcpy(filp->name+strlen(title), path);

	if (open_pipe(filp)) 
		goto pipe_out3;

	return 0;
	
pipe_out3:
	free(filp->name);
pipe_out2:
	free(filp->file_data);
pipe_out1:
	free(filp);
pipe_out:
	return -1;
}

int close_net( struct file_io *filp)
{
	clr_fd(filp);
	close(filp->rfd); // Don't need to close filp->wfd, it is equal to rfd
	free(filp->name);
	if(filp->file_data)
		free(filp->file_data);
	free(filp);
	return 0;
}
int open_net( struct file_io *filp )
{
	int fd;
	union {
		struct sockaddr_un un_addr;
		struct sockaddr_in in_addr;
	} sockaddr;
	int sockaddr_len;
	socklen_t size = sizeof(sockaddr);
	char in_name[sizeof(sockaddr.un_addr)+100];
	char inetaddr[INET_ADDRSTRLEN];
	char *name;
	void *file_data;

	printf("%s: Network connection has new client; "
			"client: rfd %i, wfd %i, name %s\n",
			__FUNCTION__, filp->rfd, filp->wfd, filp->name);

	/* accept() the connection.
	 * if fd < 0, there was a problem in the network layer*/
	if (0 > (fd = accept(filp->rfd, (struct sockaddr *)&sockaddr, &size))) {
		perror("Error accepting from net socket");
		goto net_out;
	}

	/* Give the remote client a name, and setup file_data:
	 * For UNIX sockets, it is the path to the file
	 * for TCP sockets, it contains the remote IPADDR:PORT
	 */
	switch (((struct sockaddr *)&sockaddr)->sa_family) {
	case AF_LOCAL:
		name = filp->name;
		sockaddr_len = SUN_LEN((struct sockaddr_un *)filp->file_data); 

		if (!(file_data = malloc(sockaddr_len))) 
			goto net_out1;
		memcpy(file_data, filp->file_data, sockaddr_len);

		break;

	case AF_INET: 
		if (!inet_ntop(AF_INET, &sockaddr.in_addr.sin_addr, inetaddr, 
				  sizeof(inetaddr))) {
			/* For whatever reason, accept did not return correct
			 * address
			 * FIXME: report this 
			 */
			strncpy(inetaddr, "xxx.xxx.xxx.xxx", sizeof(inetaddr));
		}
		snprintf(in_name, sizeof(in_name), "TCP: Remote client %s:%i", 
			inetaddr, ntohs(sockaddr.in_addr.sin_port));
		name = in_name;

		sockaddr_len = sizeof(struct sockaddr_in);
		if (!(file_data= malloc(sockaddr_len))) {
			perror("malloc()");
			goto net_out1;
		}
		memcpy(file_data, &sockaddr.in_addr, sockaddr_len);

		break;

	default:
		name = strncpy(in_name, "Unknown AF", sizeof(in_name));
		file_data = NULL;
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

	if (!(filp = (struct file_io *)malloc(sizeof(struct file_io)))) {
		perror("malloc");
		goto net_out2;
	}

	/* Setup function pointers for actions to perform when input becomes 
	 * ready, output is ok or network connection is closed
	 */
	filp->rfd = filp->wfd = fd;
	filp->close = close_net;
	filp->read = new_msr_client;
	filp->write = NULL;
	filp->file_data = file_data;
	if (!(filp->name = strdup(name)))
		goto net_out3;
	init_fd(filp);

	return 0;

net_out3:
	free(filp);
net_out2:
	// file_data may be null, see the switch() statement above
	if (file_data)
		free(file_data);
net_out1:
	close(fd);
net_out:
	return -1;
}
int prepare_unix(char *path)
{
	struct file_io *filp;
	struct sockaddr_un addr;
	char name[sizeof(addr.sun_path)+20];

        parse_options (argv, argc);

	printf("%s: Preparing unix socket path: %s\n", __FUNCTION__, path);

	if (strlen(path) > sizeof(addr.sun_path)) {
		printf("Error: Unix domain path too long\n");
		goto unix_out;
	}

	if (!(filp = (struct file_io *)malloc(sizeof(struct file_io)))) {
		perror("malloc");
		goto unix_out;
	}

	unlink(path);	// Ignore system errors
	if (0 > (filp->wfd = filp->rfd = socket(PF_UNIX, SOCK_STREAM, 0))) {
		perror("Error creating unix socket");
		goto unix_out1;
	}

	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, path); 	// Length was checked above
	if (bind(filp->rfd, (struct sockaddr *)&addr, SUN_LEN(&addr))) {
		perror("Error binding socket");
		goto unix_out2;
	}
	if (listen(filp->rfd, 5)) {
		perror("Error listening socket");
		goto unix_out2;
	}
	if (fcntl(filp->rfd, F_SETFL, O_RDONLY | O_NONBLOCK)) {
		perror("Error setting socket flags");
		goto unix_out2;
	}

	sprintf(name,"Unix sock, path: %s", path); 

	if (!(filp->file_data = malloc(SUN_LEN(&addr)))) {
		printf("Could not allocate priv_data for unix socket\n");
		goto unix_out2;
	}
	memcpy(filp->file_data, &addr, SUN_LEN(&addr));

	if (!(filp->name = strdup(name))) {
		perror("strdup");
		goto unix_out3;
	}

	/* Setup next routines to be called when input becomes ready */
	filp->close = NULL;	// Never gets closed
	filp->read = open_net;
	filp->write = NULL;
	init_fd(filp);

	return 0;

unix_out3:
	free(filp->file_data);
unix_out2:
	close(filp->rfd);
unix_out1:
	free(filp);
unix_out:
	return -1;
}
int prepare_tcp( int port )
{
	struct file_io *filp;
	struct sockaddr_in *addr;
	int sockopt;
	char name[30];

	printf("%s: Preparing tcp socket port: %i\n", __FUNCTION__, port);

	if (!(filp = (struct file_io *)malloc(sizeof(struct file_io)))) {
		perror("malloc");
		goto tcp_out;
	}

	if (!(filp->file_data = malloc(sizeof(struct sockaddr_in)))) {
		perror("malloc");
		goto tcp_out1;
	}
	addr = (struct sockaddr_in *)filp->file_data;

	if (0 > (filp->rfd = filp->wfd = socket(PF_INET, SOCK_STREAM, 0))) {
		perror("Error creating tcp socket");
		goto tcp_out2;
	}

	sockopt = 1;
	if (setsockopt(filp->rfd, SOL_SOCKET, SO_REUSEADDR, 
				&sockopt, sizeof(sockopt))) {
		perror("Error setting socket options");
		goto tcp_out3;
	}

	addr->sin_family = AF_INET;
	addr->sin_port = htons(port);
	addr->sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(filp->rfd, (struct sockaddr *)addr, 
				sizeof(struct sockaddr_in))) {
		perror("Error binding socket");
		goto tcp_out3;
	}
	if (listen(filp->rfd, 5)) {
		perror("Error listening socket");
		goto tcp_out3;
	}

	if (fcntl(filp->rfd, F_SETFL, O_RDONLY | O_NONBLOCK)) {
		perror("Error setting socket flags");
		goto tcp_out3;
	}

	sprintf(name,"TCP sock, port: %i", port); 
	if (!(filp->name = strdup(name))) {
		perror("malloc");
		goto tcp_out3;
	}

	/* Setup next routines to be called when input becomes ready */
	filp->close = NULL;	// Never gets closed
	filp->read = open_net;
	filp->write = NULL;
	init_fd(filp);

	return 0;

tcp_out3:
	close(filp->rfd);
tcp_out2:
	free(filp->file_data);
tcp_out1:
	free(filp);
tcp_out:
	return -1;
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
	s = file_max*sizeof(struct file_io *);
	if (!(file_io = malloc(s))) {
		perror("Error malloc");
		exit(1);
	}
	memset(file_io, 0, s);

	/* Initialise file descriptor sets for select() */
	FD_ZERO(&select_vec.rfd);
	FD_ZERO(&select_vec.wfd);
	select_max = 0;

	/* Open initial communication channels to enable data clients
	 * to attach to this data server
	 */
	if (prepare_pipe("/tmp/p") ||
	    prepare_unix("/tmp/logger_sock") ||
	    prepare_tcp(2345) ||
	    prepare_rtp(RTP_FIO, RTB_FIO, RTM_FIO))
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
	struct file_io *filp;
	struct fd_set out_vec;	// Return vector with ready file descriptors
				// from select()

	file_init();	// Initialise all file descriptors

	// Main never ending loop around select()
	while (1) {
		/* Call select() */
		memcpy(&out_vec, &select_vec, sizeof(select_vec));
		fd_count = select(
				select_max, 
				&out_vec.rfd,
				&out_vec.wfd,
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
			if (FD_ISSET(fd, &out_vec.rfd)) {
				fd_count--;
				printf("fd %i is ready for read\n",fd);

				// Find client that ownes file desriptor
				filp = file_io[fd];

				// Call appropriate client function
				filp->read(filp->priv_data);
			}

			/* Is output channel to client ready? */
			if (FD_ISSET(fd, &out_vec.wfd)) {
				fd_count--;
				printf("fd %i is ready for write\n",fd);

				// Find client that ownes file desriptor
				filp = file_io[fd];

				// Call appropriate client function
				filp->write(filp->priv_data);
			}
			fd++;
		}
	}

	return 0;
}
