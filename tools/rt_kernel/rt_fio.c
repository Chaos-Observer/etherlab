/******************************************************************************
 *
 *           $RCSfile: rtai_module.c,v $
 *           $Revision: 1.2 $
 *           $Author: rich $
 *           $Date: 2006/02/04 11:07:15 $
 *           $State: Exp $
 *
 *           Autor: Richard Hacker
 *
 *           (C) Copyright IgH 2005
 *           Ingenieurgemeinschaft IgH
 *           Heinz-Baecker Str. 34
 *           D-45356 Essen
 *           Tel.: +49 201/36014-0
 *           Fax.: +49 201/36014-14
 *           E-mail: info@igh-essen.com
 *
 * 	This file implements the character device file operations for the
 * 	real time process
 *
 *           $Log$
 *
 *****************************************************************************/ 

#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#error "Don't have config.h"
#endif

/* Kernel header files */
#include <linux/kernel.h>       /* pr_debug() */
#include <linux/fs.h>           /* everything */
#include <linux/cdev.h>         /* that is what this section does ;-) */
#include <linux/sem.h>          /* semaphores */
#include <linux/poll.h>         /* poll_wait() */
#include <linux/mm.h>           /* mmap */
#include <asm/uaccess.h>        /* get_user(), put_user() */
#include <linux/delay.h>

/* RTAI headers */
#include <rtai_sem.h>

/* Local headers */
#include "rt_main.h"
#include "rt_kernel.h"
#include "fio_ioctl.h"

/*#########################################################################*
 * Here the file operations for the RTW Process IO are defined.
 * The buddy needs this to be able to get a stream of every time step
 * that the Real-Time process makes
 *#########################################################################*/

/* .nopage method for the mmap() functionality. If the buddy accesses a 
 * page that has not been mapped, this method gets called */
static struct page *rtp_vma_nopage(struct vm_area_struct *vma,
                                        unsigned long address, int *type)
{
    unsigned long offset;
    struct page *page = NOPAGE_SIGBUS;
    struct rt_mdl *rt_mdl = vma->vm_private_data;

    offset = (address - vma->vm_start) + (vma->vm_pgoff << PAGE_SHIFT);

    /* Check that buddy did not want to access memory out of range */
    if (offset >= rt_mdl->rtb_last - rt_mdl->rtb_buf)
        return NOPAGE_SIGBUS;

    page = vmalloc_to_page(rt_mdl->rtb_buf + offset);

    pr_debug("Nopage fault vma, address = %#lx, offset = %#lx, page = %p\n", 
            address, offset, page);

    /* got it, now increment the count */
    get_page(page);
    if (type)
        *type = VM_FAULT_MINOR;
    
    return page;
}


struct vm_operations_struct rtp_vm_ops = {
    .nopage =   rtp_vma_nopage,
};

/* mmap() method for Process IO */
static int rtp_mmap(
        struct file *filp,
        struct vm_area_struct *vma
        ) {
    struct rt_mdl *rt_mdl = filp->private_data;

    if (!rt_mdl->rtb_buf)
        return -ENODEV;

    vma->vm_ops = &rtp_vm_ops;
    vma->vm_flags |= VM_RESERVED;       /* Pages will not be swapped out */
    vma->vm_private_data = filp->private_data;

    return 0;
}
/* File open method for Process IO */
static int rtp_open(
        struct inode *inode,
        struct file *filp
        ) {
    int err = 0;
    struct rt_mdl *rt_mdl = 
        container_of(inode->i_cdev, struct rt_mdl, rtp_dev);

    down(&rt_manager.sem);
    if (test_and_set_bit(RTB_BIT, &rt_mdl->buddy_there)) {
        err = -EBUSY;
    } else if (!test_bit(rt_mdl->model_id, &rt_manager.loaded_models) ||
            test_bit(rt_mdl->model_id, &rt_manager.stopped_models)) {
        err = -ENODEV;
        clear_bit(RTB_BIT, &rt_mdl->buddy_there);
    } else {
        filp->private_data = rt_mdl;
    }
    up(&rt_manager.sem);

    return err;
}
static int rtp_release(
        struct inode *inode,
        struct file *filp
        ) {
    struct rt_mdl *rt_mdl = filp->private_data;
    int err = 0;

    down(&rt_manager.sem);
    clear_bit(RTB_BIT, &rt_mdl->buddy_there);
    rt_sem_wait(&rt_mdl->buf_sem);
    if (rt_mdl->rtb_buf)
        vfree(rt_mdl->rtb_buf);
    rt_mdl->rtb_buf = NULL;
    rt_mdl->wp = NULL;
    rt_sem_signal(&rt_mdl->buf_sem);
    up(&rt_manager.sem);
    wake_up_interruptible(&rt_mdl->data_q);

    return err;
}
static long rtp_ioctl(
        struct file *filp,
        unsigned int command,
        unsigned long data
        ) {
    struct rt_mdl *rt_mdl = filp->private_data;
    struct rtw_model *rtw_model = rt_mdl->rtw_model;
    long rv = 0;

    down(&rt_manager.sem);
    switch (command) {
        case RTP_CREATE_BLOCKIO_BUF:
            {
                unsigned int buf_size;

                /* Work out how long the buffer for all BlockIO must be */
                buf_size = rtw_model->buffer_time > rtw_model->sample_period
                    ? rtw_model->buffer_time/rtw_model->sample_period : 1;
                rv = buf_size;  /* User buddy process wants to know how
                                   many images are taken, not the 
                                   buffer length */
                buf_size *= rtw_model->rtB_size;

                /* Get some memory */
                rt_mdl->rtb_buf = vmalloc(buf_size);
                if (!rt_mdl->rtb_buf) {
                    printk("Could not vmalloc buffer length %ul "
                            "for Process IO\n", buf_size);
                    rv = -ENOMEM;
                    break;
                }
                pr_debug("Allocated BlockIO Buf size %u, addr %p\n", 
                        buf_size, rt_mdl->rtb_buf);

                rt_sem_wait(&rt_mdl->buf_sem);

                /* Setup pointer to the last image in the buffer */
                rt_mdl->rtb_last = rt_mdl->rtb_buf + buf_size;

                /* Setting this wp starts the process of taking images
                 * of the model */
                rt_mdl->wp = rt_mdl->rtb_buf;
                    
                rt_sem_signal(&rt_mdl->buf_sem);
            }
            break;

        case RTP_GET_WP:
            /* Get the current write pointer. All data between the current read
             * and write pointer is valid. 
             * In case the buffer overflowed, return -ENOSPC */
            if (rt_mdl->wp) {
                /* Get pointer. If write pointer has wrapped, return end of
                 * data area. */
                rv = rt_mdl->wp >= rt_mdl->rp 
                    ? rt_mdl->wp - rt_mdl->rtb_buf
                    : (rt_mdl->rtb_last - rt_mdl->rtb_buf);

                /* Return time slice index */
                rv /= rtw_model->rtB_size;
            } else {
                /* You were sleeping !! */
                rv = -ENOSPC;
            }
            break;

        case RTP_SET_RP:
            /* Set the buddy process' current read pointer. All data up to
             * this pointer has been processed, and the space can be
             * overwritten.
             * No errors */

            /* Convert time slice to offset in buffer */
            data *= rtw_model->rtB_size;

            if (data >= rt_mdl->rtb_last - rt_mdl->rtb_buf) {
                /* Ideally, the buddy does not know how long the buffer is
                 * (he could know using RTP_GET_SIZE and the size of one
                 * BlockIO), and just wants to read the next segment.
                 * However, this process knows the buffer size, and just wraps
                 * the read pointer to 0 if the buddy wants to read past 
                 * the buffer's end */
                data = 0;
            }
            rt_mdl->rp = rt_mdl->rtb_buf + data;

            /* Return time slice of actual read pointer. */
            rv = data/rtw_model->rtB_size;
            break;

        case RTP_RESET_RP:
            /* Reset the buddy processes read pointer to the actual write
             * pointer. This is important, because logging stops when there
             * is a collision, i.e. the buddy process is dead 
             * Return the current write pointer
             * No Errors */
            rt_mdl->rp = rt_mdl->wp = rt_mdl->rtb_buf;
            rv = 0;
            break;
            
        case RTP_BLOCKIO_SIZE:
            rv = rtw_model->rtB_size;
            break;

        default:
            pr_info("ioctl() number %u unknown\n", command);
            rv = -ENOTTY;
            break;
    }
    up(&rt_manager.sem);

    return rv;
}
static unsigned int rtp_poll(
        struct file *filp,
        poll_table *wait
        ) {
    struct rt_mdl *rt_mdl = filp->private_data;
    unsigned int mask = 0;

    /* Wait for data */
    poll_wait(filp, &rt_mdl->data_q, wait);

    /* File has data when read and write pointers are different */
    if (rt_mdl->wp != rt_mdl->rp) {
        mask = POLLIN | POLLRDNORM;
    }

    return mask;
}
/* This is the Linux Kernel side helper for the poll function, because 
 * wake_up() cannot be called from inside the rt-task 
 */
void rtp_data_avail_handler(void)
{
    unsigned int model_id;

    while ((model_id = ffs(rt_manager.srq_mask))) {
        model_id--;
        clear_bit(model_id, &rt_manager.srq_mask);
        //pr_debug("TID %u has data for buddy\n", model_id);

        //if (waitqueue_active(&rt_manager.rt_mdl[model_id].data_q))
            wake_up_interruptible(&rt_manager.rt_mdl[model_id]->data_q);
    }
}

static struct file_operations rtp_fops = {
    .unlocked_ioctl =    rtp_ioctl,
    .mmap =     rtp_mmap,
    .open =     rtp_open,
    .poll =     rtp_poll,
    .release =  rtp_release,
    .owner =    THIS_MODULE,
};

/* This makes a snapshot of the current process and places this slice
 * in rtb_buf after the last one */
void rtp_make_photo(struct rt_mdl *rt_mdl)
{
    struct rtw_model *rtw_model = rt_mdl->rtw_model;

    rt_sem_wait(&rt_mdl->buf_sem);

    /* Only take a photo when wp is valid. To stop taking photos (e.g.
     * when not initialised or when the buffer is full), set wp to NULL */
    if (rt_mdl->wp) {
        memcpy(rt_mdl->wp, rtw_model->mdl_rtB, rtw_model->rtB_size);

        /* Update write pointer, wrapping if necessary */
        rt_mdl->wp += rtw_model->rtB_size;
        if (rt_mdl->wp == rt_mdl->rtb_last) {
            rt_mdl->wp = rt_mdl->rtb_buf;
        }

        /* Stop if the write pointer caught up to the read pointer */
        if (rt_mdl->wp == rt_mdl->rp) {
            rt_mdl->wp = NULL;
            pr_info("rtp_buf full\n");
        }
    }
    rt_sem_signal(&rt_mdl->buf_sem);

    /* Send a signal that data is available */
    set_bit(rt_mdl->model_id, &rt_manager.srq_mask);
    rt_pend_linux_srq(rt_manager.srq);
}


/*#########################################################################*
 * Here the file operations to control the RTW Process are defined.
 * The buddy needs this to be able to get various information from the 
 * Real-Time Process
 *#########################################################################*/


static int ctl_open(
        struct inode *inode,
        struct file *filp
        ) {
    int err = 0;
    struct rt_mdl *rt_mdl = 
        container_of(inode->i_cdev, struct rt_mdl, ctl_dev);

    down(&rt_manager.sem);
    /* Only one is allowed */
    if (test_and_set_bit(CTL_BIT, &rt_mdl->buddy_there)) {
        pr_info("Another process has already aquired the Control Channel\n");
        err = -EBUSY;
    } else if (!test_bit(rt_mdl->model_id, &rt_manager.loaded_models) ||
            test_bit(rt_mdl->model_id, &rt_manager.stopped_models)) {
        clear_bit(CTL_BIT, &rt_mdl->buddy_there);
        err = -ENODEV;
    } else {
        filp->private_data = rt_mdl;
    }
    up(&rt_manager.sem);

    return err;
}
static int ctl_release(
        struct inode *inode,
        struct file *filp
        ) {
    int err = 0;
    struct rt_mdl *rt_mdl = filp->private_data;

    clear_bit(CTL_BIT, &rt_mdl->buddy_there);
    wake_up_interruptible(&rt_mdl->data_q);

    return err;
}
static long ctl_ioctl(
        struct file *filp,
        unsigned int command,
        unsigned long data
        ) {
    long rv = 0;
    struct rt_mdl *rt_mdl = filp->private_data;
    struct rtw_model *rtw_model = rt_mdl->rtw_model;

    down(&rt_manager.sem);
    switch (command) {
        case CTL_NEW_PARAM:

            /* Steal lock if it has not yet been used */
            rt_sem_wait(&rt_mdl->io_rtP_sem);

            /* Put parameters into a pending area. RT task
             * will pick it up from there */
            if (copy_from_user(rtw_model->pend_rtP, (void *)data, 
                    rtw_model->rtP_size)) {
                rv = -EFAULT;
            } else {
                rtw_model->new_rtP = 1;
            }

            /* Signal RT task of new parameters */
            rt_sem_signal(&rt_mdl->io_rtP_sem);

            rt_printk("Sent rt process new parameter set\n");
            break;

        case CTL_IP_FORCE:
            /* Input forcing vector was received */
            rt_printk("IP_FORCE not yet implemented\n");
            rv = -EINVAL;
            break;

        case CTL_OP_FORCE:
            /* Output forcing vector was received */
            rt_printk("IP_FORCE not yet implemented\n");
            rv = -EINVAL;
            break;

        case CTL_VER_STR:
            /* Buddy process wants to get out version string */
            rv = strlen(rtw_model->modelVersion);
            if (copy_to_user((void *)data, rtw_model->modelVersion, rv)) {
                rv = -EFAULT;
                break;
            }
            rt_printk("Sent version string %s\n", rtw_model->modelVersion);
            break;

        case CTL_GET_PARAM:
            /* Buddy process wants to get the complete parameter
             * set */
            if (copy_to_user((void *)data, rtw_model->mdl_rtP,
                    rtw_model->rtP_size)) {
                rv = -EFAULT;
                break;
            }
            rt_printk("Sent parameters to buddy\n");
            break;

        case CTL_GET_SAMPLEPERIOD:
            /* Buddy process wants to get the complete parameter
             * set */
            rv = rtw_model->sample_period;
            rt_printk("Sent base rate (%lu) to buddy\n", 
                    rtw_model->sample_period);
            break;

        case CTL_SET_PARAM:
            /* Only a part of parameter set to be changed */
            {
                struct param_change p;
                struct change_ptr change_ptr;
                int i;

                /* Reject if there is no parameter set */
                if (!rtw_model->rtP_size) {
                    rv = -ENODEV;
                    break;
                }

                if (copy_from_user(&p, (void *)data, sizeof(struct param_change))
                        || !access_ok(VERIFY_READ, p.rtP, rtw_model->rtP_size)
                        || !access_ok(VERIFY_READ, p.changes, 
                                p.count*sizeof(struct change_ptr))) {
                    rv = -EFAULT;
                    break;
                }

                rt_sem_wait(&rt_mdl->io_rtP_sem);

                for( i = 0; i < p.count; i++) {
                    __copy_from_user(&change_ptr, &p.changes[i],
                            sizeof(struct change_ptr));
                    __copy_from_user(
                            rtw_model->pend_rtP + change_ptr.pos, 
                            p.rtP + change_ptr.pos,
                            change_ptr.len);
                    rt_printk("Setting %i bytes at offset %i for parameters\n", 
                            change_ptr.len, change_ptr.pos);
                }

                rtw_model->new_rtP = 1;
                rt_sem_signal(&rt_mdl->io_rtP_sem);
            }

            break;

        case CTL_GET_PARAM_SIZE:
            rv = rtw_model->rtP_size;
            break;

        case CTL_GET_SO_PATH:
            rv = strlen(rtw_model->so_path);
            if (copy_to_user((void *)data, rtw_model->so_path, rv)){
                rv = -EFAULT;
                break;
            };
            break;

        default:
            rv = -ENOTTY;
            break;
    }
    up(&rt_manager.sem);

    return rv;
}
ssize_t ctl_read(
        struct file *filp,
        char __user *dest,
        size_t buflen,
        loff_t *loff
        ) {
    size_t size = 0;
    return size;
}
unsigned int ctl_poll(
        struct file *filp,
        struct poll_table_struct *pstruct
        ) {
    return 0;
}

struct file_operations ctl_fops = {
    .unlocked_ioctl =     ctl_ioctl,
    .owner =     THIS_MODULE,
    .open =      ctl_open,
    .release =   ctl_release,
    .read =      ctl_read,
    .poll =      ctl_poll,
};


/*#########################################################################*
 * Here the file operations to control the Real-Time Kernel are defined.
 * The buddy needs this to be able to change things in the Kernel.
 *#########################################################################*/
static int rtp_main_open(
        struct inode *inode,
        struct file *filp
        ) {
    int err = 0;

    if (down_trylock(&rt_manager.file_lock)) {
        err = -EBUSY;
    }

    return err;
}
static int rtp_main_release(
        struct inode *inode,
        struct file *filp
        ) {

    up(&rt_manager.file_lock);
    return 0;
}
static unsigned int rtp_main_poll(
        struct file *filp,
        poll_table *wait
        ) {
    unsigned int mask = 0;

    /* Wait for data */
    poll_wait(filp, &rt_manager.event_q, wait);

    /* Check whether there was a change in the states of a Real-Time Process */
    if (rt_manager.notify_buddy) {
        down(&rt_manager.sem);
        rt_manager.notify_buddy = 0;
        up(&rt_manager.sem);
        mask = POLLIN | POLLRDNORM;
    }

    return mask;
}

static long rtp_main_ioctl(
        struct file *filp,
        unsigned int command,
        unsigned long data
        ) {
    long rv = 0;
    pr_debug("ioctl command %#x, data %lu\n", command, data);
    down(&rt_manager.sem);
    switch (command) {
        case RTM_GET_ACTIVE_MODELS:
            rv = put_user(rt_manager.loaded_models ^ rt_manager.stopped_models, 
                    (uint32_t *)data);
            break;

        case RTM_MODEL_NAME:
            {
                struct rtp_model rtp_model;
                const char *name;
                rv = copy_from_user(&rtp_model, (void *)data,
                        sizeof(rtp_model.number));
                if (rv)
                    break;
                if (rtp_model.number >= MAX_MODELS || 
                        !test_bit(rtp_model.number, &rt_manager.loaded_models)
                        ) {
                    pr_debug("Requested model number %i does not exist\n", 
                            rtp_model.number);
                    rv = -ENODEV;
                    break;
                }

                name = rt_manager.rt_mdl[rtp_model.number]->rtw_model->modelName;

                // Model name length was checked to be less than 
                // MAX_MODEL_NAME_LEN when it was registered
                rv = copy_to_user(&(((struct rtp_model *)data)->name), 
                    name, strlen(name)+1);
                break;
            }

        default:
            rv = -ENOTTY;
            break;
    }
    up(&rt_manager.sem);

    return rv;
}

static struct file_operations main_fops = {
    .unlocked_ioctl = rtp_main_ioctl,
    .poll           = rtp_main_poll,
    .open           = rtp_main_open,
    .release        = rtp_main_release,
    .owner          = THIS_MODULE,
};


/*#########################################################################*
 * Here is general management code to initialise a new Real-Time Workshop
 * model that is inserted into the Kernel.
 *#########################################################################*/

/* Free file handles of a specific RTW Model */
void rtp_fio_clear_mdl(struct rt_mdl *rt_mdl)
{

    pr_debug("Tearing down FIO for model_id %u\n", rt_mdl->model_id);

    cdev_del(&rt_mdl->ctl_dev);
    cdev_del(&rt_mdl->rtp_dev);
}

/* Create file handles to insert another RTW Model into the Real-Time Kernel */
int rtp_fio_init_mdl( struct rt_mdl *rt_mdl, struct rtw_model *rtw_model)
{
    dev_t devno;
    int major = MAJOR(rt_manager.dev), minor = MINOR(rt_manager.dev);
    int err = -1;

    pr_debug("Initialising FIO for model_id %u, model %s\n", 
            rt_mdl->model_id, rtw_model->modelName);

    /* Normal initialisation of semaphores */
    init_waitqueue_head(&rt_mdl->data_q);
    init_waitqueue_head(&rt_mdl->ctl_q);
    rt_sem_init(&rt_mdl->io_rtP_sem,1);
    rt_sem_init(&rt_mdl->buf_sem,1); /* Lock the BlockIO buffer semaphore */
    rt_mdl->wp = NULL;
    rt_mdl->buddy_there = 0;

    /* Character device to control process */
    cdev_init(&rt_mdl->ctl_dev, &ctl_fops);
    rt_mdl->ctl_dev.owner = THIS_MODULE;
    devno = MKDEV(major, minor + 2*rt_mdl->model_id + 1);
    if ((err = cdev_add(&rt_mdl->ctl_dev, devno, 1))) {
        printk("Could not add Control FOPS to cdev\n");
        goto out_add_ctl;
    }
    pr_debug("Added char dev for Controls, minor %u\n", MINOR(devno));

    /* Character device for BlockIO stream */
    cdev_init(&rt_mdl->rtp_dev, &rtp_fops);
    rt_mdl->rtp_dev.owner = THIS_MODULE;
    devno = MKDEV(major, minor + 2*rt_mdl->model_id + 2);
    if ((err = cdev_add(&rt_mdl->rtp_dev, devno, 1))) {
        printk("Could not add Process IO FOPS to cdev\n");
        goto out_add_rtp;
    }
    pr_debug("Added char dev for BlockIO, minor %u\n", MINOR(devno));

    return 0;

out_add_rtp:
    cdev_del(&rt_mdl->ctl_dev);
out_add_ctl:
    return err;
}

/*#########################################################################*
 * Here is general management code to initialise file handles for the 
 * Real-Time Kernel.
 *#########################################################################*/

/* Clear the Real-Time Kernel file handles */
void rtp_fio_clear(void)
{
    pr_debug("Tearing down FIO for rt_kernel\n");

    if(rt_manager.srq > 0)
        rt_free_srq(rt_manager.srq);

    cdev_del(&rt_manager.main_ctrl);
    unregister_chrdev_region(rt_manager.dev,2*MAX_MODELS+1);
}

/* Set up the Real-Time Kernel file handles */
int rtp_fio_init(void)
{
    int err = -1;

    pr_debug("Initialising FIO for rt_kernel\n");

    /* Create character devices for this process. Each RT model needs 2
     * char devices, and rt_manager needs 1 */
    if ((err = alloc_chrdev_region(&rt_manager.dev, 0, 
                    2*MAX_MODELS+1, "rt_kernel"))) {
        printk("Could not allocate character device\n");
        goto out_chrdev;
    }
    pr_debug("Reserved char dev for rt_kernel, major %u, minor start %u\n",
            MAJOR(rt_manager.dev), MINOR(rt_manager.dev));

    cdev_init(&rt_manager.main_ctrl, &main_fops);
    rt_manager.main_ctrl.owner = THIS_MODULE;
    if ((err = cdev_add(&rt_manager.main_ctrl, rt_manager.dev, 1))) {
        printk("Could not add Process IO FOPS to cdev\n");
        goto out_add_rtp;
    }
    pr_debug("Started char dev for rt_kernel\n");

    /* Get a service request handler. This is the only way to call linux 
     * kernel functions reliably from within an RTAI thread */
    if ((rt_manager.srq = 
                rt_request_srq(0, rtp_data_avail_handler, NULL)) < 0) {
        printk("Error: could not get a linux service request handler\n");
        err = rt_manager.srq;
        goto out_req_srq;
    }


    return 0;

    rt_free_srq(rt_manager.srq);
out_req_srq:
    cdev_del(&rt_manager.main_ctrl);
out_add_rtp:
    unregister_chrdev_region(rt_manager.dev,2*MAX_MODELS+1);
out_chrdev:
    return err;
}

