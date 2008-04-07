/*****************************************************************************
 *
 *   It is possible to control the buddy from the command line. To this 
 *   purpose, a unix_domain and a network socket is opened with which the 
 *   buddy, when called as a command processor, can communicate with
 *   the main buddy.
 *
 *   To use etherlab_buddy to contact the server:
 *     etherlab_buddy <host:port> <command> <options>
 *               - Contacts the server via TCP/IP
 *     etherlab_buddy [-s </path/to/socket>] <command> <options>:
 *               - Contacts the server using unix domain socket
 *
 *   To use etherlab_buddy as server:
 *     etherlab_buddy [-s </path/to/socket>] [-p port]
 *
 *   The difference is subtle: etherlab_buddy is started as a server
 *   when there are no commands left after parsing all options.
 *
 *           Autor: Richard Hacker
 *           License: GPL
 *
 *           (C) Copyright Richard Hacker
 *           IgH 2006
 *           Ingenieurgemeinschaft IgH
 *           Heinz-Bäcker Str. 34
 *           D-45356 Essen
 *           Tel.: +49 201/61 99 31
 *           Fax.: +49 201/61 98 36
 *           E-mail: info@igh-essen.com
 *
 */ 
/*--includes----------------------------------------------------------------*/

#include <stdio.h>		// printf
#include <stdlib.h>		// malloc, free
#include <unistd.h>		// close(), unlink(), sysconf()
#include <errno.h>
#include <getopt.h>
#include <string.h>

static const char *command_path = "/tmp/buddy_sock";
static int port = 5000;

#include "buddy_main.h"
#include "modules.h"
#include "net_io.h"

static char buf[4096];

/* Network client disconnected itself */
static void __attribute__((unused)) close_command_chan(int fd, void *p)
{
    struct sock_data __attribute__((unused)) *sock_data = p;

    close_net(fd);

    return;
}

/* Data has arrived from socket to be processed */
static void read_command(int fd, void *p)
{
	ssize_t s = 0;

        s = read(fd, buf, sizeof(buf));
        set_wfd(fd);

	return;
}

/* The file descriptor is ready to send the response */ 
static void send_response(int fd, void *p)
{
	ssize_t s = 0;

        s = write(fd, buf, strlen(buf));
        clr_wfd(fd);
	return;
}

/* This function is called from the file descriptor's read() method when
 * a client connects itself to the server */
static void *new_cmd_client(int fd, void *p)
{

	return p;
}

static void parse_options (void)
{
    int c;
    struct option longopts[] = {
        {"command_socket", 1, NULL, 's'},
        {"command_port", 1, NULL, 'p'},
        {},
    };

    optind = 0;
    while (1) {
        c = getopt_long(argc, argv, ":c:p:d", longopts, NULL);
        switch (c) {
            case -1:
                return;
            case ':':
                fprintf(stderr, "Argument for option -%c missing\n", optopt);
                exit(-1);
            case 'c':
                command_path = optarg;
                break;
            case 'p':
                port = atoi(optarg);
                break;
        }
    }
    return;
}
void
command_module_help(void)
{
    printf("Options for command module:\n"
            "    -s\t--socket\tPath to socket for buddy<->buddy communication\n"
            "\t\t\t[%s]\n", command_path);
}
int command_module_prepare(void)
{
        parse_options();

	printf("%s: Preparing command module: socket path %s, port %i\n", 
                __FUNCTION__, command_path, port);

	/* Setup next routines to be called when input becomes ready */
        if (command_path && 
                prepare_unix(command_path, new_cmd_client, read_command, 
                    send_response, NULL)) {
            fprintf(stderr, "Could not initialise %s\n", __FILE__);
            return -1;
        }

	/* Setup next routines to be called when input becomes ready */
        if (port && 
                prepare_tcp(port, new_cmd_client, read_command, 
                    send_response, NULL)) {
            fprintf(stderr, "Could not initialise %s\n", __FILE__);
            return -1;
        }

	return 0;
}
