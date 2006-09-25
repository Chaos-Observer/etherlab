#include <linux/ioctl.h>

#define MAX_MODEL_NAME_LEN 256

/* Here are the ioctl() commands to control the Process IO data stream 
 * between the Real-Time Process and the user space Buddy */

#define FIO_MAGIC  'R'

/* Get the current write pointer in the buffer. Data between this and the
 * previous call to RTP_SET_RP is valid data.
 * Can return -ENOSPC when the buffer overflowed */
#define RTP_GET_WP   _IO(FIO_MAGIC, 1)

/* Set the buddy processes read pointer. Data before this pointer has been
 * processed and can be overwritten with new data.
 * Returns actual position. This can be different from that intended, when
 * it wraps
 * No error */
#define RTP_SET_RP   _IOWR(FIO_MAGIC, 2, int)

/* Reset the read pointer to the current write pointer and return this
 * pointer.
 * No error */
#define RTP_RESET_RP _IO(FIO_MAGIC, 3)

/* Setup RT-Process BlockIO buffer */
#define RTP_CREATE_BLOCKIO_BUF _IO(FIO_MAGIC, 4)

/* Get the size of one BlockIO */
#define RTP_BLOCKIO_SIZE  _IO(FIO_MAGIC, 5)


struct param_change {
    unsigned int count;
    void *rtP;
    struct change_ptr {
        unsigned int pos;
        unsigned int len;
    } changes[];
};

#define CTL_NEW_PARAM         _IOW(FIO_MAGIC, 10, struct param_change *)
#define CTL_IP_FORCE          _IOW(FIO_MAGIC, 11, void *)
#define CTL_OP_FORCE          _IOW(FIO_MAGIC, 12, void *)
#define CTL_VER_STR           _IOR(FIO_MAGIC, 13, void *)
#define CTL_GET_PARAM         _IOR(FIO_MAGIC, 14, void *)
#define CTL_GET_SAMPLEPERIOD  _IOR(FIO_MAGIC, 15, int)
#define CTL_SET_PARAM         _IOW(FIO_MAGIC, 16, void *)
#define CTL_GET_PARAM_SIZE    _IOR(FIO_MAGIC, 17, size_t)
#define CTL_GET_SO_PATH       _IOR(FIO_MAGIC, 18, void *)


/* ioctl commands for the Real-Time Manager */
struct rtp_model {
    unsigned int number;
    char name[MAX_MODEL_NAME_LEN];
};
#define MAN_MAGIC       'M'
#define RTM_GET_ACTIVE_MODELS   _IOR(MAN_MAGIC, 0, unsigned long *)
#define RTM_MODEL_NAME          _IOR(MAN_MAGIC, 1, struct model_name *)
