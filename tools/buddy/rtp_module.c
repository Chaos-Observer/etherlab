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
#include "include/fio_ioctl.h"
#include "include/taskstats.h"
#include "msr_main.h"
#include "msr_lists.h"
#include "msr_reg.h"

#include "buddy_main.h"
#include "modules.h"
#include "net_io.h"

#define CHECK_ERR(condition, errno, goto_label, msg, args...) { \
    if (condition) { \
        syslog(LOG_CRIT, msg, ##args); \
        rv = errno; \
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

        /* Properties of the model. Defined in fio_ioctl.h */
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


static int start_model(const char *model_name, unsigned int model_num)
{
    int rv = 0;
    char filename[PATH_MAX];
    struct model *model;
    struct task_stats *task_stats;
    unsigned int st;
    enum si_orientation_t orientation;
    char *path_buf;
    unsigned int idx;

    /* Get a pointer to model, going to need it often */
    model = &rtp_io.model[model_num];

    /* Modify rtp_io.dev_path to point to the character device file. 
     * It has been checked that the original rtp_io.dev_path is 10 
     * chars less than PATH_MAX, so there is enough space to add a few
     * numbers. */
    snprintf(filename, PATH_MAX, "%s%u", rtp_io.dev_path, model_num + 1);
    syslog(LOG_DEBUG, "Open data channel (%s) to the Real-Time Process\n",
            filename);
    model->rtp_fd = open(filename, O_RDONLY | O_NONBLOCK);
    CHECK_ERR(model->rtp_fd < 0, errno, out_open_rtb,
            "Error: Cannot open char device %s to rt_process: %s",
            filename, strerror(errno));

    /*======================================================================
     * Initialise the Real-Time Process parameters in user space
     *======================================================================*/
    /* Find out the size of the parameter structure */
    rv = ioctl(model->rtp_fd, GET_MDL_PROPERTIES, &model->properties);
    CHECK_ERR(rv, errno, out_get_mdl_properties,
            "Error: Could not fetch model properties: %s", strerror(errno));

    /* Get memory for parameter structure */
    model->rtP = malloc(model->properties.rtP_size);
    CHECK_ERR(!model->rtP, errno, out_malloc_rtP,
            "Could not allocate memory: %s", strerror(errno));

    /* Initialise it with the current Real-Time Process parameters. Note
     * that the model may already have been running and that these parameters
     * are not necessarily the defaults */
    rv = ioctl(model->rtp_fd, GET_PARAM, model->rtP);
    CHECK_ERR(rv < 0, errno, out_ioctl_get_param,
            "Could not fetch model's current parameter set: %s",
                strerror(errno));

    /*======================================================================
     * Initialise pointers to the BlockIO stream from the Real-Time Process
     *======================================================================*/

    model->blockio_start = 
        mmap(0, model->properties.rtB_size * model->properties.rtB_count, 
                PROT_READ, MAP_PRIVATE, model->rtp_fd, 0);
    CHECK_ERR(model->blockio_start == MAP_FAILED, errno, out_mmap,
            "Could not memory map model's IO space: %s", strerror(errno));
    model->slice_rp = ioctl(model->rtp_fd, RESET_BLOCKIO_RP);

    /*======================================================================
     * Find out the model's tick period, and initialise MSR and model
     *======================================================================*/
    CHECK_ERR (msr_init(model, param_change, 
                model->properties.sample_period, model->blockio_start, 
                model->properties.rtB_size, model->properties.rtB_count),
            -1, out_msr_init, "Could not initialise msr software");

    /* Initialise the model with the pointer obtained from the shared
     * object above */
    path_buf = (char*) malloc( model->properties.variable_path_len + 1);
    CHECK_ERR (!path_buf, errno, out_malloc_si,
            "Could not allocate memory: %s", strerror(errno));

    for (idx = 0; idx < model->properties.signal_count; idx++) {
        struct signal_info si = {
            .index = idx,
            .path = path_buf,
            .path_buf_len = model->properties.variable_path_len + 1,
        };

        rv = ioctl(model->rtp_fd, GET_SIGNAL_INFO, &si);
        CHECK_ERR (rv, errno, out_get_signal_info,
                "Error: could not get Signal Info for signal %u: %s", 
                idx, strerror(errno));
        if (!si.dim[0]) {
            syslog(LOG_NOTICE, "msr cannot handle multidimensional arrays.");
            continue;
        }
        else if (si.dim[1]) {
            orientation = si_matrix;
        }
        else {
            orientation = si.dim[0] > 1 ? si_vector : si_scalar;
            si.dim[1] = 1;
        }
        CHECK_ERR (!msr_reg_rtw_signal( model->properties.name,
                    si.path, si.name, si.alias,
                    "", si.offset, 
                    si.dim[0], si.dim[1], si.data_type, orientation,
                    si_data_width[si.data_type]), 
                -1, out_reg_signal, "MSR Error: could not register signal");

    }

    for (idx = 0; idx < model->properties.param_count; idx++) {
        struct signal_info si = {
            .index = idx,
            .path = path_buf,
            .path_buf_len = model->properties.variable_path_len + 1,
        };

        rv = ioctl(model->rtp_fd, GET_PARAM_INFO, &si);
        CHECK_ERR (rv, errno, out_get_param_info,
                "Error: could not get Parameter Info %s", strerror(errno));
        if (!si.dim[0]) {
            syslog(LOG_NOTICE, "msr cannot handle multidimensional arrays.");
            continue;
        }
        else if (si.dim[1]) {
            orientation = si_matrix;
        }
        else {
            orientation = si.dim[0] > 1 ? si_vector : si_scalar;
            si.dim[1] = 1;
        }
        CHECK_ERR (!msr_reg_rtw_param( model->properties.name,
                    si.path, si.name, si.alias, "", 
                    model->rtP + si.offset, 
                    si.dim[0], si.dim[1], si.data_type, orientation,
                    si_data_width[si.data_type]), 
                -1, out_reg_param, "MSR Error: could not register parameter");
    }

    /* Statistics of Tasks are stored as a (struct task_stats *) just
     * after the rtB vector. Register these separately */
    task_stats = (struct task_stats *) (model->properties.rtB_size - 
         model->properties.num_tasks*sizeof(struct task_stats));
    msr_reg_time(&task_stats[0].time);
    for (st = 0; st < model->properties.num_tasks; st++) {
        msr_reg_task_stats(st, &task_stats[st].time, &task_stats[st].exec_time,
                &task_stats[st].time_step, &task_stats[st].overrun);
    }

    syslog(LOG_DEBUG, "Preparing msr module socket port: %i\n", 
            msr_port + model_num);
    model->net_fd = prepare_tcp(msr_port + model_num, new_msr_client, msr_read, 
                msr_write, NULL);
    if (model->net_fd < 0) {
        syslog(LOG_ERR, "Could not register network port %i", 
                msr_port + model_num);
        goto out_prepare_tcp;
    }

    /* Now register the model with the main dispatcher. When the file handle
     * becomes readable (using select) the callback is executed */
    init_fd(model->rtp_fd, update_rtB, NULL, model);

    /* Free unused memory */
    free(path_buf);

    syslog(LOG_DEBUG, "Finished registering model with buddy");
    return 0;

out_prepare_tcp:
out_reg_param:
out_get_param_info:
out_reg_signal:
out_get_signal_info:
    free(path_buf);
out_malloc_si:
    msr_cleanup();
out_msr_init:
    munmap(model->blockio_start, 
            model->properties.rtB_size * model->properties.rtB_count);
out_mmap:
out_ioctl_get_param:
    free(model->rtP);
out_malloc_rtP:
out_get_mdl_properties:
    close(model->rtp_fd);
out_open_rtb:
    return rv;
}

static void open_model(int fd, void *d)
{
    int rv, i, j, pid;
    uint32_t new_models, deleted_models, running_models; 
    struct rt_app_name rt_app_name;

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

        rt_app_name.number = i;
        rv = ioctl(fd, RTK_MODEL_NAME, &rt_app_name);
        if (rv) {
            syslog(LOG_ERR, "ioctl() errorgetting model name: %s",
                    strerror(errno));
            continue;
        }
        syslog(LOG_DEBUG, "Starting new model number %i, name: %s", 
                i, rt_app_name.name);

        if (!(pid = fork())) { 
            // child here

            /* Close all open file handles */
            for (j = foreground ? 3 : 0; j < file_max; j++) {
                if (!close(j)) {
                    clr_fd(j);
                }
            }
            if (start_model(rt_app_name.name, i))
                exit(-1);
            return;
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
                i, rt_app_name.name);
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
