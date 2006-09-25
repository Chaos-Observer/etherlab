/* This module of the buddy process connects the msr library to the real
 * time process. It openes a management channel to the rt_manager, which
 * in turn informes when a new real time process is loaded or removed.
 *
 * When a new real time process starts, this module opens the channels to
 * the RT process for communications and current value tables.
 *
 * Thereafter it informes the msr library of the new process.
 *
 * When the RT process stops, everything is reversed and allocated resources
 * are freed up.
 * 
 */
#include <stdint.h>	// uint32_t
#include <stdio.h>	// printf()
#include <unistd.h>	// close()
#include <fcntl.h>	// open(), O_RDONLY, etc
#include <stdlib.h>	// malloc()
#include <errno.h>	// errno, strerror()
#include <string.h>	// strlen()
#include <dlfcn.h>	// Dynamic loader
#include <limits.h>	// PATH_MAX
#include <sys/ioctl.h>	// ioctl()
#include <sys/mman.h>	// mmap()

#include "defines.h"
#include "fio_ioctl.h"
#include "msr_main.h"

#include "buddy_main.h"

struct rtp_io {
    char base_path[PATH_MAX];
    unsigned int base_path_len;
    int manager_fd;
    long active_models;

    struct model {
        // File descriptors used
	int comm_fd;            // bidirectional communication with RTP
	int rtB_fd;             // current value tables (RealTime BlockIO)
                                // to be processed

        /* The following members are used to manage the interaction with 
         * the BlockIO stream coming from the Real-Time Process */
        void *rtB_buf;          // Pointer to start of BlockIO
        unsigned int rtB_rp;    // Read Pointer to the next time slice
        size_t rtB_buflen;     // Total size of BlockIO buffer in the kernel
        size_t rtB_size;        // Length of one BlockIO

        /* The following members are used to interact with the parameter
         * structure */
        void *rtP;              // Pointer to model parameters
    } model[32];
} rtp_io;


int new_params(void *priv_data, char *start, size_t len)
{
	struct model *model = priv_data;

	return ioctl(model->comm_fd, CTL_NEW_PARAM, model->rtP);
}

int read_msg(void *priv_data)
{
	struct model *model = priv_data;
	char msg[4096];
	ssize_t len;

	len = read(model->comm_fd, msg, sizeof(msg));

//	if (len > 0)
//		msr_write_stdout(msg, len);

	return 0;
}

int update_rtB(void *priv_data)
{
	struct model *model = priv_data;
        int new_rp;
        int i;

        /*
        static int j = 0;
        if (j++==1000) {
            printf("tick\n");
            j = 0;
        }
        */

        new_rp = ioctl(model->rtB_fd, RTP_GET_WP);

        if (new_rp == -1 && errno == ENOSPC) {
            model->rtB_rp = ioctl(model->rtB_fd, RTP_RESET_RP);
            fprintf(stderr, "Kernel buffer overflowed... Resetting\n");
            return 0;
        }
        for (i = model->rtB_rp; i < new_rp; i++) {
            /* model->rtB_buf contains the base pointer to the buffer
             * containing one block io (BlockIO) for every time slice:
             *
             *      +-------+---------+-------+-...---+-------+-------+
             *      |   0   | BlockIO | 2     | ...   |  i    | last  |       
             *      +-------+---------+-------+-...---+-------+-------+
             *      ^
             *      | *model->rtB_buf
             *
             * Then (*model->rtB_buf)[i] is the i'th slice, and
             * &(*model->rtB_buf)[i] is its address. Call msr_update
             * with this address
             */
            msr_update(i);
            //memcpy(buf,model->rtB_buf + model->rtB_size*i,model->rtB_size);
            //printf(".");
        }
        //fflush(stdout);
        model->rtB_rp = ioctl(model->rtB_fd, RTP_SET_RP, new_rp);
        //printf("%i %i\n", model->rtB_rp, new_rp);
        //sleep(1);
        return 0;
}

int start_model(char *name, unsigned int num)
{
    int rv = 0;
    size_t len;
    char *so_file;
    void *dlhandle;
    struct model *model;
    unsigned long base_rate;
    int (*model_init)(void *);
    size_t rtP_size;

    /* Get a pointer to model, going to need it often */
    model = &rtp_io.model[num];

    /* Modify rtp_io.base_path to point to the character device file. 
     * It has been checked that the original rtp_io.base_path is 10 
     * chars less than PATH_MAX, so there is enough space to add a few
     * numbers. */
    sprintf(rtp_io.base_path + rtp_io.base_path_len, "%u", num*2 + 1);
    printf("Open control channel (%s) to the Real-Time Process\n", 
            rtp_io.base_path);
    if ((model->comm_fd = 
                open(rtp_io.base_path, O_RDONLY | O_NONBLOCK)) < 0) {
        perror("Cannot open communication char device to Real-Time Process");
        rv = -errno;
        goto out_open_comm;
    }

    /* Open data cdev that provides a stream of BlockIO */
    sprintf(rtp_io.base_path + rtp_io.base_path_len, "%u", num*2 + 2);
    printf("Open data channel (%s) to the Real-Time Process\n", 
            rtp_io.base_path);
    if ((model->rtB_fd = 
                open(rtp_io.base_path, O_RDONLY | O_NONBLOCK)) < 0) {
        perror("Cannot open block IO char device to Real-Time Process");
        rv = -errno;
        goto out_open_rtb;
    }
    rtp_io.base_path[rtp_io.base_path_len] = '\0'; /* Restore cdev name */

    /*======================================================================
     * Initialise the Real-Time Process parameters in user space
     *======================================================================*/
    /* Find out the size of the parameter structure */
    rtP_size = ioctl(model->comm_fd, CTL_GET_PARAM_SIZE);
    /* Get memory for parameter structure */
    if (!(model->rtP = malloc(rtP_size))) {
        perror("Error getting memory for parameters");
        rv = -errno;
        goto out_malloc_rtP;
    }
    /* Initialise it with the current Real-Time Process parameters. Note
     * that the model may already have been running and that these parameters
     * are not necessarily the defaults */
    if ((rv = ioctl(model->comm_fd, CTL_GET_PARAM, model->rtP)) < 0) {
        fprintf(stderr, "Failed to collect initial parameter set %i\n", rv);
        goto out_ioctl_get_param;
    }

    /* Find out the base rate of the model */
    base_rate = ioctl(model->comm_fd, CTL_GET_SAMPLEPERIOD);

    /*======================================================================
     * Initialise pointers to the BlockIO stream from the Real-Time Process
     *======================================================================*/
    model->rtB_size = ioctl(model->rtB_fd, RTP_BLOCKIO_SIZE);
    model->rtB_buflen = ioctl(model->rtB_fd, RTP_CREATE_BLOCKIO_BUF);
    if (model->rtB_buflen <= 0) {
        fprintf(stderr, "Failed to initialise BlockIO buffer of %i seconds\n",
                model->rtB_buflen);
        goto out_ioctl_get_param;
    }
    model->rtB_buflen *= model->rtB_size;
    model->rtB_buf = 
        mmap(0, model->rtB_buflen, PROT_READ, MAP_PRIVATE, model->rtB_fd, 0);
    if (model->rtB_buf == MAP_FAILED) {
        perror("mmap() failed");
        goto out_mmap;
    }
    model->rtB_rp = ioctl(model->rtB_fd, RTP_RESET_RP);

    /*======================================================================
     * Load the Model specific shared object, which contains all path 
     * and parameter names.
     *======================================================================*/
    /* Build up the path name to the shared object, which in turn has
     * all the model specific details. It has the form ./<model>.so 
     * The ./ is needed otherwise the dynamic loader only searches in
     * the library paths */
    len = PATH_MAX;
    if (!(so_file = malloc(PATH_MAX))) {
        perror("malloc");
        rv = -errno;
        goto out_malloc;
    }
    if ((rv = ioctl(model->comm_fd, CTL_GET_SO_PATH, so_file)) < 0) {
        fprintf(stderr, "Failed to get path to the shared object\n");
        goto out_get_so_path;
    }
    if (!strlen(so_file)) {
        strcpy(so_file,".");
    }
    if (so_file[strlen(so_file)-1] != '/')
        strcpy(so_file + strlen(so_file), "/");
    strcpy(so_file + strlen(so_file), name);
    strcpy(so_file + strlen(so_file), ".so");

    /* Now load the shared object. This also additionally executes the function
     * having __attribute__((constructor)) */
    printf("Loading model symbol file %s\n", so_file);
    dlhandle = dlopen(so_file, RTLD_LAZY);
    if (!dlhandle) {
        fprintf(stderr, "%s\n", dlerror());
        rv = -errno;
        goto out_dlopen;
    }
    if (!(model_init = dlsym(dlhandle, "model_init"))) {
        rv = -errno;
        fprintf(stderr,"%s\n", dlerror());
        goto out_init_sym;
    }

    /*======================================================================
     * Find out the model's tick period, and initialise MSR and model
     *======================================================================*/
    if (msr_init(model, new_params, base_rate, model->rtB_buf, model->rtB_size,
                model->rtB_buflen/model->rtB_size)) {
            fprintf(stderr, "Could not initialise msr software\n");
            goto out_msr_init;
    }
    /* Initialise the model with the pointer obtained from the shared
     * object above */
    model_init(model->rtP);


    /* Now register the model with the main dispatcher. When the file handle
     * becomes readable (using select) the callback is executed */
    //init_fd(model->comm_fd, read_msg, NULL, model);
    init_fd(model->rtB_fd, update_rtB, NULL, model);

    /* Free unused memory */
    dlclose(dlhandle);
    free(so_file);
    return rv;


    msr_cleanup();
out_msr_init:
out_init_sym:
    dlclose(dlhandle);
out_get_so_path:
out_dlopen:
    free(so_file);
out_malloc:
    munmap(model->rtB_buf, model->rtB_buflen);
out_mmap:
out_ioctl_get_param:
    free(model->rtP);
out_malloc_rtP:
    close(model->rtB_fd);
out_open_rtb:
    close(model->comm_fd);
out_open_comm:
    rtp_io.base_path[rtp_io.base_path_len] = '\0';
    return rv;
}

void stop_model(unsigned int num)
{
    struct model *model;
    model = &rtp_io.model[num];

    clr_fd(model->rtB_fd);
    msr_disconnect();
    munmap(model->rtB_buf, model->rtB_buflen);
    free(model->rtP);
    close(model->rtB_fd);
    close(model->comm_fd);
}

int open_model(void *d)
{
    int rv, i;
    uint32_t model_list, new_models, del_models;
    struct rtp_model rtp_model;
    struct rtp_io *rtp_io = d;

    rv = ioctl(rtp_io->manager_fd, RTM_GET_ACTIVE_MODELS, &model_list);
    if (rv) {
        perror("Could not get names of active models");
        return rv;
    }

    new_models = ~rtp_io->active_models & model_list;

    for( i = 0; i < 32; i++) {
        if (!(new_models & (0x01<<i))) {
            continue;
        }

        rtp_model.number = i;
        rv = ioctl(rtp_io->manager_fd, RTM_MODEL_NAME, &rtp_model);
        if (rv) {
            perror("Error getting model name");
            continue;
        }
        printf("Starting model %s\n", rtp_model.name);

        start_model(rtp_model.name, i);

    }

    del_models = rtp_io->active_models & ~model_list;
    for( i = 0; i < 32; i++) {
        if (!(del_models & (0x01<<i))) {
            continue;
        }

        printf("Stopping model %i\n", i);

        stop_model(i);

    }
    rtp_io->active_models = model_list;

    return rv;
}

int prepare_rtp(char *base_path)
{
    int rv = 0;

    rtp_io.base_path_len = strlen(base_path);
    if (rtp_io.base_path_len > sizeof(rtp_io.base_path)-10) {
        fprintf(stderr, "Base path too long (> %i)\n", 
                sizeof(rtp_io.base_path)-10);
        rv = -ENOMEM;
        goto out_size_check;
    }

    strcpy(rtp_io.base_path, base_path);
    strcpy( rtp_io.base_path + rtp_io.base_path_len, "0");

    printf("Opening %s\n", rtp_io.base_path);
    if ((rtp_io.manager_fd = 
                open(rtp_io.base_path, O_RDWR | O_NONBLOCK)) < 0) {
            perror("Error opening manager channel");
            rv = -errno;
            goto out_open_manager;
    }
    rtp_io.base_path[rtp_io.base_path_len] = '\0';

    rtp_io.active_models = 0;
    open_model(&rtp_io);
    init_fd(rtp_io.manager_fd, open_model, NULL, &rtp_io);
    return 0;

out_open_manager:
    rtp_io.base_path[rtp_io.base_path_len] = '\0';
out_size_check:
    return rv;
}
