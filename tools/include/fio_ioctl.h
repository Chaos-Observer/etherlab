#ifndef FIO_IOCTL_H
#define FIO_IOCTL_H

#include <linux/ioctl.h>
#include <stddef.h>

#ifndef __KERNEL__
#include <unistd.h>     // ssize_t
#endif

#if defined __cplusplus
extern "C" {
#endif

#include "etl_data_info.h"

/* The maximum number of models that can be handled. The limit is due
 * to the data type that the bit operators (set_bit, clear_bit) can handle
 */
#define MAX_MODELS sizeof(unsigned long)*8

/* Arbitrary value */
#define MAX_MODEL_NAME_LEN 256
#define MAX_MODEL_VER_LEN  100

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

/* Get the model symbol file from the kernel */
#define GET_SIGNAL_INFO       _IOR(FIO_MAGIC, 15, struct signal_info *)
#define GET_PARAM_INFO        _IOR(FIO_MAGIC, 16, struct signal_info *)
// struct signal_info is defined in etl_data_info.h

/* This ioctl is used to return all the properties about the real time
 * process that the buddy needs. Pass it the address of a local 
 * struct mdl_properties */
#define GET_MDL_PROPERTIES    _IOR(FIO_MAGIC, 19, struct mdl_properties)
struct mdl_properties {
    size_t rtB_count;           // Total count of block_io structs in kernel
    size_t rtB_size;            // Size of block IO structure
    size_t rtP_size;            // Size of parameter structure
    unsigned int num_st;        // Number of sample times
    unsigned int num_tasks;     // Number of tasks
    unsigned long base_rate;    // Model's base rate in microseconds

    size_t param_count;         // Number of parameters
    size_t signal_count;        // Number of signals
    size_t variable_path_len;   // Memory requirements to store variable
                                // path and alias, including \0
    char name[MAX_MODEL_NAME_LEN];
    char version[MAX_MODEL_VER_LEN];
};

#define GET_MDL_SAMPLETIMES   _IOR(FIO_MAGIC, 20, struct rtcom_ioctldata)

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

#define SET_PRIV_DATA           _IOW(RTK_MAGIC, 3, struct rtcom_ioctldata)
struct rtcom_ioctldata {
    struct model *model_id;
    void *data;
    size_t data_len;
};

struct rtcom_event {
    enum {new_model, del_model} type;
    void *priv_data;
    union {
        struct model *model_ref;
    } data;
};

#define GET_RTK_PROPERTIES      _IOR(RTK_MAGIC, 4, struct rt_kernel_prop *)
struct rt_kernel_prop {
    size_t iomem_len;
    size_t eventq_len;
};

#if defined __cplusplus
}
#endif

#endif // FIO_IOCTL_H
