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
#include <unistd.h>	// close(), sleep()
#include <fcntl.h>	// open(), O_RDONLY, etc
#include <stdlib.h>	// malloc()
#include <errno.h>	// errno, strerror()
#include <string.h>	// strlen()
#include <dlfcn.h>	// Dynamic loader
#include <limits.h>	// PATH_MAX
#include <sys/ioctl.h>	// ioctl()
#include <sys/mman.h>	// mmap()

//#include "defines.h"
#include "fio_ioctl.h"
#include "msr_main.h"
#include "msr_lists.h"

#include "buddy_main.h"
#include "modules.h"
#include "net_io.h"

int msr_port = 2345;

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

        printf("new params\n");

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

/* Find the model description file. The description file has the name
 * <model_name>.so
 * If the real time model is loaded with a command line parameter 
 * so_path=<path-to-so.file>, the file is located using that path.
 * If so_path is not given, it is located in the current working directory.
 *
 * Parameters:
 *      filename: pointer to buffer where the filename is stored.
 *      so_path: parameter passed when loading the rt_model
 *      model_name: Model name
 */
static int find_so_file(char *filename, size_t len, 
        const char *so_path, const char *model_name) 
{
    char *path_prefix;
    int rv = 0;

    if (!strlen(so_path)) {
        /* No file name was supplied by the model, so try to locate
         * it in the working directory when the buddy was started */
        snprintf(filename, len, "%s/%s.so", cwd, model_name);

        if (access(filename, R_OK | X_OK)) {
            /* File was not found, give up */
            rv = -errno;
            if (!access(filename, R_OK))
                perror("Cannot find <mdl>.so");
            else {
                fprintf(stderr, 
                        "Could not locate model description file "
                        "%s/<mdl>.so. Either start from the working "
                        "directory, or pass the path as an argument to "
                        "insmod:\n"
                        "\tinsmod <mdl>_kmod.ko so_path=<path_to_dir>\n",
                        so_path);
            }
            return rv;
        } else {
            return 0;
        }
    }

    /* Some path was supplied.
     * First check whether an the name starts with "/" or "./"
     * dl_open searches LD_SO_PATH if the name does not start with
     * these sequences, but this is not required here */
    path_prefix = (so_path[0] == '/' || strcmp("./", so_path) != 1) 
        ? "" : "./";

    /* First try: use the name that was supplied as the path to
     * the file's directory */
    snprintf(filename, len, "%s%s/%s.so", 
            path_prefix, so_path, model_name);
    if (!access(filename, R_OK | X_OK))
        return 0;

    /* Now try whether just the .so was not supplied */
    snprintf(filename, len, "%s%s.so", path_prefix, so_path);
    if (!access(filename, R_OK | X_OK))
        return 0;

    /* Last, try if this points directly to a file */
    snprintf(filename, len, "%s%s", 
            path_prefix, so_path);
    if (!access(filename, R_OK | X_OK))
        return 0;

    fprintf(stderr, "Could not locate model description file %s/%s.so"
            " - check the path passed to insmod\n", so_path, model_name);
    return -ENOENT;
}

static int start_model(int fd, const char *model_name, unsigned int model_num)
{
    int rv = 0;
    char filename[PATH_MAX];
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
    printf("Open data channel (%s) to the Real-Time Process\n", filename);
    if ((model->rtp_fd = open(filename, O_RDONLY | O_NONBLOCK)) < 0) {
        perror("Cannot open block IO char device to Real-Time Process");
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
        perror("Error getting memory for parameters");
        rv = -errno;
        goto out_malloc_rtP;
    }
    /* Initialise it with the current Real-Time Process parameters. Note
     * that the model may already have been running and that these parameters
     * are not necessarily the defaults */
    if ((rv = ioctl(model->rtp_fd, GET_PARAM, model->rtP)) < 0) {
        fprintf(stderr, "Failed to collect initial parameter set %i\n", rv);
        goto out_ioctl_get_param;
    }

    /*======================================================================
     * Initialise pointers to the BlockIO stream from the Real-Time Process
     *======================================================================*/

    model->blockio_start = 
        mmap(0, model->properties.rtB_len * model->properties.rtB_cnt, 
                PROT_READ, MAP_PRIVATE, model->rtp_fd, 0);
    if (model->blockio_start == MAP_FAILED) {
        perror("mmap() failed");
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
    if ((rv = find_so_file(filename, sizeof(filename), 
                    model->properties.so_path, model_name))) {
        goto out_get_mdl_so_path;
    }

    /* Now load the shared object. This also additionally executes the function
     * having __attribute__((constructor)) */
    printf("Loading model symbol file %s\n", filename);
    dlhandle = dlopen(filename, RTLD_LAZY);
    if (!dlhandle) {
        fprintf(stderr, "%s\n", dlerror());
        rv = -errno;
        goto out_dlopen;
    }
    if (!(version_string = dlsym(dlhandle, "version_string"))) {
        rv = -errno;
        fprintf(stderr,"%s\n", dlerror());
        goto out_init_sym;
    }
    if (!(model_init = dlsym(dlhandle, "model_init"))) {
        rv = -errno;
        fprintf(stderr,"%s\n", dlerror());
        goto out_init_sym;
    }

    /* Make sure that the versions are the same! */
    if ((rv = ioctl(model->rtp_fd, CHECK_VER_STR, version_string)) < 0) {
        switch (errno) {
            case EFAULT:
                fprintf(stderr, "ERROR: Could not verify model and "
                        "description file versions due to an internal "
                        "error\n");
                break;
            case EACCES:
                fprintf(stderr, "Error loading model description file %s: "
                        "Real Time Module and description files do not "
                        "match. Recompile and retry\n", version_string);
                break;
            case ENOMEM:
                fprintf(stderr, "ERROR: Low on memory\n");
                break;
            default:
                fprintf(stderr, "ERROR: An unforseen error occurred.\n");
                break;
        }
        rv = -errno;
        goto out_wrong_version;
    }

    /*======================================================================
     * Find out the model's tick period, and initialise MSR and model
     *======================================================================*/
    if (msr_init(model, param_change, 
                model->properties.base_rate, model->blockio_start, 
                model->properties.rtB_len, model->properties.rtB_cnt)) {
            fprintf(stderr, "Could not initialise msr software\n");
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

    printf("%s: Preparing msr module socket port: %i\n", 
            __FUNCTION__, msr_port);
    model->net_fd = prepare_tcp(msr_port, new_msr_client, msr_read, 
                msr_write, NULL);
    if (model->net_fd < 0) {
        fprintf(stderr, "Could not initialise %s\n", __FILE__);
        return -1;
    }

    /* Now register the model with the main dispatcher. When the file handle
     * becomes readable (using select) the callback is executed */
    init_fd(model->rtp_fd, update_rtB, NULL, model);

    /* Free unused memory */
    dlclose(dlhandle);
    return 0;


    msr_cleanup();
out_msr_init:
out_wrong_version:
out_init_sym:
    dlclose(dlhandle);
out_dlopen:
out_get_mdl_so_path:
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
//    struct rtp_io *rtp_io = d;
    int rv, i, j, pid;
    uint32_t new_models;
    struct rtp_model_name rtp_model_name;

    rv = ioctl(fd, RTK_GET_NEW_MODELS, &new_models);
    if (rv) {
        perror("Could not get names of active models");
        return;
    }

    /* Get a list of models that have started since last time 
     * it was checked */
//    new_models = ~rtp_io->active_models & model_list;
    for( i = 0; i < 32; i++) {
        if (!(new_models & (0x01<<i))) {
            continue;
        }

        rtp_model_name.number = i;
        rv = ioctl(fd, RTK_MODEL_NAME, &rtp_model_name);
        if (rv) {
            perror("Error getting model name");
            continue;
        }
        printf("Starting model %s\n", rtp_model_name.name);
        if (ioctl(fd, RTK_SET_MODEL_WATCHED, i)) {
            perror("Cannot mark model watched");
            continue;
        }

        if (!(pid = fork())) { 
            // child
            for (j = foreground ? 3 : 0; j < file_max; j++) {
                if (!close(j))
                    clr_fd(j);
            }
            if (start_model(fd, rtp_model_name.name, i))
                exit(-1);
        } else if (pid > 0) {
            // parent

            //reg_sigchild(pid, &rtp_io->model[i]);
        } else { 
            // fork() failed
            perror("fork()");
        }
    }

    return;
}

int rtp_module_prepare(void)
{
    int fd;
    int rv = 0;
    char filename[PATH_MAX];

    /* Open a file descriptor to the rt_kernel. It is allways the first
     * one */
    snprintf(filename, sizeof(filename), "%s0", rtp_io.dev_path);

    printf("Opening %s\n", filename);
    if ((fd = open(filename, O_RDWR | O_NONBLOCK)) < 0) {
            perror("Error opening manager channel");
            rv = -errno;
            goto out_open_manager;
    }

//    rtp_io.active_models = 0;
    init_fd(fd, open_model, NULL, &rtp_io);
    return 0;

    close(fd);

out_open_manager:
    return rv;
}
