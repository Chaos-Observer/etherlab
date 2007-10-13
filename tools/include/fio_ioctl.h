#include <linux/ioctl.h>
#include <stddef.h>

/* The maximum number of models that can be handled. The limit is due
 * to the data type that the bit operators (set_bit, clear_bit) can handle
 */
#define MAX_MODELS sizeof(unsigned long)*8

/* Arbitrary value */
#define MAX_MODEL_NAME_LEN 256
#define MAX_MODEL_VER_LEN  100

struct task_stats {
    double time;
    unsigned int exec_time;
    unsigned int time_step;
    unsigned int overrun;
};


/* Here are the ioctl() commands to control the Process IO data stream 
 * between the Real-Time Process and the user space Buddy */

#define FIO_MAGIC  '2'

/* Get the current write pointers in the blockIO and message buffers.
 * For the blockIO vector, data between this and the previous call to 
 * RTP_SET_RP is valid data.
 * For the message buffer, data between buffer start and msg pointer is 
 * valid.
 * Can return -ENOSPC when the buffer overflowed */
#define GET_WRITE_PTRS   _IOR(FIO_MAGIC, 1, struct data_p *)
struct data_p {
    int wp;             // Current data vector slice
    ssize_t msg;        // Message pointer
};

/* Set the buddy processes' read pointer. Data before this pointer has been
 * processed and can be overwritten with new data.
 * Returns actual position. This can be different from that intended, when
 * it wraps
 * No error */
#define SET_BLOCKIO_RP   _IOWR(FIO_MAGIC, 2, int)

/* Reset the read pointer to the current write pointer and return this
 * pointer.
 * No error */
#define RESET_BLOCKIO_RP _IO(FIO_MAGIC, 3)

/* Pass a whole new parameter set to the real time process. */
#define NEW_PARAM             _IOW(FIO_MAGIC, 10, void *)

/* This command compares the version string of the model symbol file
 * with that of the running model. Only if these version strings match
 * are parameter changes allowed. */
//#define CHECK_VER_STR         _IOW(FIO_MAGIC, 13, char *)

/* Write the models current parameter set to the data pointer */
#define GET_PARAM             _IOR(FIO_MAGIC, 14, void *)

/* Get the model symbol file from the kernel */
#define GET_MDL_DESCRIPTION   _IOR(FIO_MAGIC, 15, void *)

/* The following structure is used by the CHANGE_PARAM ioctl to change the 
 * models parameter set by the buddy selectively */
#define CHANGE_PARAM          _IOW(FIO_MAGIC, 16, struct param_change *)
struct param_change {
    void *rtP;                  // Pointer from where the data is copied

    unsigned int pos;           // Start of change
    unsigned int len;           // Length of change

    unsigned int count;         /* If there is more than one change than
                                   the one above, use this value to indicate
                                   how many structures of data changes are
                                   still coming in the next structure */
    struct change_ptr {         /* [count] number of changes */
        unsigned int pos;       // start of change
        unsigned int len;       // Length of change
    } changes[];
};

/* This ioctl is used to return all the properties about the real time
 * process that the buddy needs. Pass it the address of a local 
 * struct mdl_properties */
#define GET_MDL_PROPERTIES    _IOR(FIO_MAGIC, 19, struct mdl_properties *)
struct mdl_properties {
    size_t rtB_len;             // Total size of BlockIO buffer in the kernel
    size_t rtB_cnt;             // Total count of block_io structs in kernel
    unsigned int numst;         // Number of sample times
    size_t rtP_size;            // Size of model parameter structure
    unsigned long base_rate;    // Model's base rate in microseconds
    size_t symbol_len;          // Length of model symbol file
};


/***************************************************
 * The following ioctl commands are used by the 
 * rt_kernel itself.
 ***************************************************/
#define RTK_MAGIC       'K'
/* Return a bit mask in a uint32_t indicating which models are active */
#define RTK_GET_ACTIVE_MODELS   _IOR(RTK_MAGIC, 0, unsigned long *)

/* Pass the model number to the rt_kernel, and it replies with the model
 * name */
struct rtp_model_name {
    unsigned int number;
    char name[MAX_MODEL_NAME_LEN];
};
#define RTK_MODEL_NAME          _IOR(RTK_MAGIC, 2, struct rtp_model_name *)
