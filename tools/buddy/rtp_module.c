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
#include <inttypes.h>	// uint32_t
#include <stdio.h>	// printf()
#include <unistd.h>	// close(), sleep()
#include <fcntl.h>	// open(), O_RDONLY, etc
#include <stdlib.h>	// malloc()
#include <errno.h>	// errno, strerror()
#include <string.h>	// strlen()
#include <dlfcn.h>	// Dynamic loader
#include <limits.h>	// PATH_MAX
#include <sys/ioctl.h>	// ioctl()
#include <sys/mman.h>	// mmap()
#include <syslog.h>	// syslog()
#include <errno.h>	// errno
#include <zlib.h>

//#include "defines.h"
#include "fio_ioctl.h"
#include "msr_main.h"
#include "msr_lists.h"

#include "buddy_main.h"
#include "modules.h"
#include "net_io.h"

#define CHECK_ERR(condition, position, msg, goto_label) { \
    if (condition) { \
        syslog(LOG_CRIT, "%s error: %s\n", position, msg); \
        goto goto_label; \
    } \
}
int msr_port = 2345;
uint32_t active_models = 0;

struct rtp_io {
    /* The path to the character device of the rt_kernel */
    const char *dev_path;

    unsigned int base_path_len;

    struct model {
        // File descriptors used
	int rtp_fd;             // File descriptor to the real time process
        int net_fd;             /* File descriptor of the network port
                                   where the module waits for connects 
                                   from the TestManager */

        /* Properties of the model */
        struct mdl_properties properties;

        void *blockio_start;    // Pointer to start of BlockIO

        unsigned int slice_rp;  // Read Pointer to the next time slice

        /* The following members are used to interact with the parameter
         * structure */
        void *rtP;              // Pointer to model parameters
    } model[MAX_MODELS];
} rtp_io = {.dev_path = "/dev/etl"};

/* Network client disconnected itself */
static void  __attribute__((used)) close_msr(int fd, void *p)
{

	// Make sure msr cleans up
	msr_close(p);

	close_net(fd);

	return;
}

/* This function is called from the file descriptor's read() method when
 * a client connects itself to the server */
static void *new_msr_client(int fd, void *p)
{
	// Prepare msr for the new connection. Returns a file descriptor
	// which all subsequent msr_*() calls expect as a parameter
	return msr_open(fd, fd);
}
static int param_change(void *priv_data, void *start, size_t len)
{
	struct model *model = priv_data;
        struct param_change delta;
        int rv;

        delta.rtP = model->rtP;
        delta.pos = (void *)start - model->rtP;
        delta.len = len;
        delta.count = 0;

	if ((rv = ioctl(model->rtp_fd, CHANGE_PARAM, &delta)))
            perror("params");

	return rv;
}

static int __attribute__((used)) new_params_set(void *priv_data, char *start, size_t len)
{
	struct model *model = priv_data;

	return ioctl(model->rtp_fd, NEW_PARAM, model->rtP);
}

static void update_rtB(int fd, void *priv_data)
{
	struct model *model = priv_data;
        struct data_p data_p;
	char msg_buf[4096];
	ssize_t len;
        int rv;
        int i;

        if ((rv = ioctl(model->rtp_fd, GET_WRITE_PTRS, &data_p))) {
        }

        if (data_p.wp == -ENOSPC) {
            model->slice_rp = ioctl(model->rtp_fd, RESET_BLOCKIO_RP);
            fprintf(stderr, "Kernel buffer overflowed... Resetting\n");
        } else {
            for (i = model->slice_rp; i < data_p.wp; i++) {
                /* model->blockio_start contains the base pointer to the buffer
                 * containing one block io (BlockIO) for every time slice:
                 *
                 *      +-------+---------+-------+-...---+-------+-------+
                 *      |   0   | BlockIO | 2     | ...   |  i    | last  |
                 *      +-------+---------+-------+-...---+-------+-------+
                 *      ^
                 *      | *model->blockio_start
                 *
                 * Then (*model->blockio_start)[i] is the i'th slice, and
                 * &(*model->blockio_start)[i] is its address. Call msr_update
                 * with this address
                 */
                msr_update(i);
            }

            model->slice_rp = ioctl(model->rtp_fd, SET_BLOCKIO_RP, data_p.wp);
        }

        while (data_p.msg > 0) {
            len = read(model->rtp_fd, msg_buf, sizeof(msg_buf-1));
            if (len < 0) {
                perror("Read Messages");
                rv = -errno;
            } else if (len) {
                msg_buf[sizeof(msg_buf)-1] = '\0';
                printf("%s\n", msg_buf);
                data_p.msg -= len;
            }
        }
        return;
}


/* This funtion fetches the compressed model symbol file from the
 * real time process, decompresses it and writes it to a file. 
 * The caller should unlink() the file to prevent polluting /tmp
 * Returns: filename of the decompressed file - the caller must free() it
 *          NULL on error
 */
static char *get_model_symbol( struct model *model)
{
    Bytef dst[4096];
    void *in_buf;
    char so_filename[] = "/tmp/msf_so_XXXXXX";
    char *ret_filename;
    int fd_so;
    int err;
    z_stream d_stream;
    size_t len;

    /* Open a temporary file where the uncompressed file will be stored */
    fd_so = mkstemp(so_filename);
    CHECK_ERR(fd_so < 0, "mkstemp()", strerror(errno), out_mkstemp_so_file);
    syslog(LOG_DEBUG,
            "Created temporary file for model symbol file: %s\n",
            so_filename);

    /* Make a copy of the filename, the caller has to free() it */
    ret_filename = strdup(so_filename);
    CHECK_ERR(!ret_filename, "strdup()", "No memory", out_strdup);

#undef TEST

#ifdef TEST
    {
        int fd;
        in_buf = malloc(10000);
        CHECK_ERR(!in_buf, "malloc()", "No memory", out_inflate_malloc);

        fd = open("/home/rich/matlab/osc.so.c", O_RDONLY);
        len = read(fd, in_buf, 10000);
        close(fd);

        printf("Read %i bytes\n", len);
    }
#else
    /* Make some space for the model symbol file */
    len = model->properties.symbol_len;
    in_buf = malloc(len);
    CHECK_ERR(!in_buf, "malloc()", "No memory", out_inflate_malloc);
    syslog(LOG_DEBUG, "Compressed model symbol file is %i bytes", len);

    /* Fetch the compressed model symbol file into in_buf */
    err = ioctl(model->rtp_fd, GET_MDL_DESCRIPTION, in_buf);
    CHECK_ERR(err, "ioctl()", strerror(errno), out_ioctl);
#endif

    /* Setup data structure for inflateInit() function */
    d_stream.zalloc = (alloc_func)0;
    d_stream.zfree = (free_func)0;
    d_stream.opaque = (voidpf)0;
    err = inflateInit(&d_stream);
    CHECK_ERR(err != Z_OK, "inflateInit()", d_stream.msg, out_inflate_init);

    /* Make d_stream pointers point to the compressed data */
    d_stream.next_in  = in_buf;
    d_stream.avail_in = len;
 
    /* Decompress the data in chunks */
    while (d_stream.avail_in && err != Z_STREAM_END) {
        d_stream.next_out = dst;
        d_stream.avail_out = sizeof(dst);

        err = inflate(&d_stream, Z_NO_FLUSH);
        len = sizeof(dst) - d_stream.avail_out;

        /* Filter out Z_STREAM_END from the errors. This is not a real error
         * and this case is handled in the while() statement */
        CHECK_ERR(err != Z_STREAM_END && err != Z_OK, 
                "inflate()", d_stream.msg, out_inflate);

        /* write() the data to the output file */
        CHECK_ERR(len != write(fd_so, dst, len), "write()", strerror(errno), 
                out_write);
    }
    syslog(LOG_INFO, "Decompressed model symbol file: %s, length %lu",
            ret_filename, d_stream.total_out);

    close(fd_so);

    err = inflateEnd(&d_stream);
    CHECK_ERR(err != Z_OK, "inflateInit()", d_stream.msg, out_inflate_end);

    return ret_filename;

out_inflate_end:
out_write:
out_inflate:
    inflateEnd(&d_stream);
out_inflate_init:
out_ioctl:
    free(in_buf);
out_inflate_malloc:
    free(ret_filename);
out_strdup:
    close(fd_so);
    unlink(so_filename);
out_mkstemp_so_file:
    return NULL;
}

static int start_model(const char *model_name, unsigned int model_num)
{
    int rv = 0;
    char filename[PATH_MAX];
    char *model_symbol_file;
    void *dlhandle;
    struct model *model;
    int (*model_init)(void *);
    struct task_stats *task_stats;
    unsigned int st;
    char *version_string;

    /* Get a pointer to model, going to need it often */
    model = &rtp_io.model[model_num];

    /* Modify rtp_io.dev_path to point to the character device file. 
     * It has been checked that the original rtp_io.dev_path is 10 
     * chars less than PATH_MAX, so there is enough space to add a few
     * numbers. */
    snprintf(filename, PATH_MAX, "%s%u", rtp_io.dev_path, model_num + 1);
    syslog(LOG_DEBUG, "Open data channel (%s) to the Real-Time Process\n",
            filename);
    if ((model->rtp_fd = open(filename, O_RDONLY | O_NONBLOCK)) < 0) {
        syslog(LOG_ERR, "Cannot open char device %s to rt_process: %s",
                filename, strerror(errno));
        rv = -errno;
        goto out_open_rtb;
    }

    /*======================================================================
     * Initialise the Real-Time Process parameters in user space
     *======================================================================*/
    /* Find out the size of the parameter structure */
    if ((rv = ioctl(model->rtp_fd, GET_MDL_PROPERTIES, &model->properties))) {
    }

    /* Get memory for parameter structure */
    if (!(model->rtP = malloc(model->properties.rtP_size))) {
        syslog(LOG_ERR, "Could not allocate memory: %s", strerror(errno));
        rv = -errno;
        goto out_malloc_rtP;
    }
    /* Initialise it with the current Real-Time Process parameters. Note
     * that the model may already have been running and that these parameters
     * are not necessarily the defaults */
    if ((rv = ioctl(model->rtp_fd, GET_PARAM, model->rtP)) < 0) {
        syslog(LOG_ERR, "Could not fetch model's current parameter set: %s",
                strerror(errno));
        goto out_ioctl_get_param;
    }

    /*======================================================================
     * Initialise pointers to the BlockIO stream from the Real-Time Process
     *======================================================================*/

    model->blockio_start = 
        mmap(0, model->properties.rtB_len * model->properties.rtB_cnt, 
                PROT_READ, MAP_PRIVATE, model->rtp_fd, 0);
    if (model->blockio_start == MAP_FAILED) {
        syslog(LOG_ERR, "Could not memory map model's IO space: %s",
                strerror(errno));
        goto out_mmap;
    }
    model->slice_rp = ioctl(model->rtp_fd, RESET_BLOCKIO_RP);

    /*======================================================================
     * Load the Model specific shared object, which contains all path 
     * and parameter names.
     *======================================================================*/
    /* Build up the path name to the shared object, which in turn has
     * all the model specific details. It has the form ./<model>.so 
     * The ./ is needed otherwise the dynamic loader only searches in
     * the library paths */
    if (!(model_symbol_file = get_model_symbol(model))) {
        goto out_get_model_symbol_file;
    }

    /* Now load the shared object. This also additionally executes the function
     * having __attribute__((constructor)) */
    syslog(LOG_DEBUG, "Loading model symbol file %s\n", model_symbol_file);
    dlhandle = dlopen(model_symbol_file, RTLD_LAZY);
    if (!dlhandle) {
        syslog(LOG_ERR, "Could not open model symbol file %s: %s",
                model_symbol_file, dlerror());
        rv = -errno;
        goto out_dlopen;
    }
    if (!(version_string = dlsym(dlhandle, "version_string"))) {
        syslog(LOG_ERR, "Symbol \"version_string\" is not exported in %s: %s",
                model_symbol_file, dlerror());
        rv = -errno;
        goto out_init_sym;
    }
    if (!(model_init = dlsym(dlhandle, "model_init"))) {
        syslog(LOG_ERR, "Symbol \"model_init\" is not exported in %s: %s",
                model_symbol_file, dlerror());
        rv = -errno;
        goto out_init_sym;
    }

    /*======================================================================
     * Find out the model's tick period, and initialise MSR and model
     *======================================================================*/
    if (msr_init(model, param_change, 
                model->properties.base_rate, model->blockio_start, 
                model->properties.rtB_len, model->properties.rtB_cnt)) {
        syslog(LOG_ERR, "Could not initialise msr software\n");
        goto out_msr_init;
    }
    /* Initialise the model with the pointer obtained from the shared
     * object above */
    model_init(model->rtP);

    /* Statistics of Tasks are stored as a (struct task_stats *) just
     * after the rtB vector. Register these separately */
    task_stats = (struct task_stats *) (model->properties.rtB_len - 
         model->properties.numst*sizeof(struct task_stats));
    msr_reg_time(&task_stats[0].time);
    for (st = 0; st < model->properties.numst; st++) {
        msr_reg_task_stats(st, &task_stats[st].time, &task_stats[st].exec_time,
                &task_stats[st].time_step, &task_stats[st].overrun);
    }

    syslog(LOG_DEBUG, "Preparing msr module socket port: %i\n", msr_port);
    model->net_fd = prepare_tcp(msr_port, new_msr_client, msr_read, 
                msr_write, NULL);
    if (model->net_fd < 0) {
        syslog(LOG_ERR, "Could not register network port %i", msr_port);
        goto out_prepare_tcp;
    }

    /* Now register the model with the main dispatcher. When the file handle
     * becomes readable (using select) the callback is executed */
    init_fd(model->rtp_fd, update_rtB, NULL, model);

    /* Free unused memory */
    dlclose(dlhandle);
    unlink(model_symbol_file);
    free(model_symbol_file);

    syslog(LOG_DEBUG, "Finished registering model with buddy");
    return 0;

out_prepare_tcp:
    msr_cleanup();
out_msr_init:
//out_wrong_version:
out_init_sym:
    dlclose(dlhandle);
out_dlopen:
    unlink(model_symbol_file);
    free(model_symbol_file);
out_get_model_symbol_file:
    munmap(model->blockio_start, 
            model->properties.rtB_len * model->properties.rtB_cnt);
out_mmap:
out_ioctl_get_param:
    free(model->rtP);
out_malloc_rtP:
    close(model->rtp_fd);
out_open_rtb:
    return rv;
}

static void open_model(int fd, void *d)
{
    int rv, i, j, pid;
    uint32_t new_models, deleted_models, running_models; 
    struct rtp_model_name rtp_model_name;

    /* Ask the rt_kernel for the bitmask of running models. The bits in
     * running_models represent whether the model is running */
    rv = ioctl(fd, RTK_GET_ACTIVE_MODELS, &running_models);
    if (rv) {
        syslog(LOG_ERR, "ioctl() error: Could not get the bit mask of "
                "active models: %s", strerror(errno));
        return;
    }

    /* Get a list of models that have started since last time 
     * it was checked */
    new_models = running_models & ~active_models;
    for( i = 0; i < 32; i++) {
        if (!(new_models & (0x01<<i))) {
            continue;
        }

        rtp_model_name.number = i;
        rv = ioctl(fd, RTK_MODEL_NAME, &rtp_model_name);
        if (rv) {
            syslog(LOG_ERR, "ioctl() errorgetting model name: %s",
                    strerror(errno));
            continue;
        }
        syslog(LOG_DEBUG, "Starting new model number %i, name: %s", 
                i, rtp_model_name.name);

        if (!(pid = fork())) { 
            // child here

            /* Close all open file handles */
            for (j = foreground ? 3 : 0; j < file_max; j++) {
                if (!close(j)) {
                    clr_fd(j);
                }
            }
            if (start_model(rtp_model_name.name, i))
                exit(-1);
        } else if (pid > 0) {
            // parent

            //reg_sigchild(pid, &rtp_io->model[i]);
        } else { 
            // fork() failed
            syslog(LOG_CRIT, "Forking failed: %s", strerror(errno));
        }
    }

    /* Get a list of models that have started since last time 
     * it was checked */
    deleted_models = ~running_models & active_models;
    for( i = 0; i < 32; i++) {
        if (!(deleted_models & (0x01<<i))) {
            continue;
        }

        syslog(LOG_DEBUG, "Stopping deleted model number %i, name: %s", 
                i, rtp_model_name.name);
    }

    active_models = running_models;

    return;
}

int rtp_module_prepare(void)
{
    int fd;
    int rv = 0;
    char filename[PATH_MAX];

    syslog(LOG_DEBUG, "%s(%s): Initialising RT-Kernel comms channel",
            __FILE__, __func__);

    /* Open a file descriptor to the rt_kernel. It is allways the first
     * one */
    snprintf(filename, sizeof(filename), "%s0", rtp_io.dev_path);
    syslog(LOG_INFO, "    Opening character device %s", filename);

    if ((fd = open(filename, O_RDWR | O_NONBLOCK)) < 0) {
        syslog(LOG_EMERG, "Could not open control channel to rt_kernel: %s",
                strerror(errno));
        rv = -errno;
        goto out_open_manager;
    }

    init_fd(fd, open_model, NULL, &rtp_io);

    open_model(fd, &rtp_io);

    return 0;

    close(fd);

out_open_manager:
    return rv;
}
