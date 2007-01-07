#include "buddy_main.h"

int close_net(int fd);
int prepare_unix( 
        const char *path,
        void *(*open_cb)(int, void *),
        void (*read_cb)(int, void*), 
        void (*write_cb)(int, void*), 
        void *priv_data
        );
int prepare_tcp( 
        int port,
        void *(*open_cb)(int, void *),
        void (*read_cb)(int, void*), 
        void (*write_cb)(int, void*), 
        void *priv_data
        );
