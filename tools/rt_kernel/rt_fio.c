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
    struct model *model = vma->vm_private_data;

    offset = (address - vma->vm_start) + (vma->vm_pgoff << PAGE_SHIFT);

    /* Check that buddy did not want to access memory out of range */
    if (offset >= model->rtb_last - model->rtb_buf)
        return NOPAGE_SIGBUS;

    page = vmalloc_to_page(model->rtb_buf + offset);

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
    struct model *model = filp->private_data;

    vma->vm_ops = &rtp_vm_ops;
    vma->vm_flags |= VM_RESERVED;       /* Pages will not be swapped out */

    /* Store model in private data */
    vma->vm_private_data = model;

    return 0;
}
/* File open method for Process IO */
static int rtp_open(
        struct inode *inode,
        struct file *filp
        ) {
    int err = 0;
    size_t buf_len;
    struct model *model = 
        container_of(inode->i_cdev, struct model, rtp_dev);
    const struct rtw_model *rtw_model = model->rtw_model;

    if (down_trylock(&model->buddy_lock)) {
        return -EBUSY;
    }
    filp->private_data = model;

    /* Get some memory to store snapshots of the BlockIO vector */
    model->rtB_cnt = (rtw_model->buffer_time > rtw_model->sample_period)
        ? rtw_model->buffer_time/rtw_model->sample_period : 1;
    model->rtB_len = rtw_model->rtB_size + model->task_stats_len;
    buf_len = model->rtB_cnt * model->rtB_len;
    model->rtb_buf = vmalloc(buf_len);
    if (!model->rtb_buf) {
        printk("Could not vmalloc buffer length %u "
                "for Process IO\n", buf_len);
        err = -ENOMEM;
        goto out_vmalloc;
    }
    pr_debug("Allocated BlockIO Buf size %u, addr %p\n", 
            buf_len, model->rtb_buf);

    model->msg_ptr = model->msg_buf = (char *)__get_free_page(GFP_KERNEL);
    model->msg_buf_len = PAGE_SIZE;
    if (!model->msg_ptr) {
        printk("Could not get free page for message buffer\n");
        err = -ENOMEM;
        goto out_get_free_page;
    }

    /* Setup pointer to the last image in the buffer */
    model->rtb_last = model->rtb_buf + buf_len;
    model->rp = model->rtb_buf;

    /* Setting this wp starts the process of taking images
     * of the model */
    rt_sem_wait(&model->buf_sem);
    model->wp = model->rtb_buf;
    rt_sem_signal(&model->buf_sem);

    return 0;

    free_page((unsigned long)model->msg_ptr);
out_get_free_page:
    vfree(model->rtb_buf);
out_vmalloc:
    up(&model->buddy_lock);
    return err;
}

static int rtp_release(
        struct inode *inode,
        struct file *filp
        ) {
    struct model *model = filp->private_data;
    int err = 0;

    rt_sem_wait(&model->buf_sem);
    model->wp = NULL;
    rt_sem_signal(&model->buf_sem);

    vfree(model->rtb_buf);
    free_page((unsigned long)model->msg_ptr);
    up(&model->buddy_lock);

    return err;
}
static long rtp_ioctl(
        struct file *filp,
        unsigned int command,
        unsigned long data
        ) {
    struct model *model = filp->private_data;
    const struct rtw_model *rtw_model = model->rtw_model;
    long rv = 0;

    switch (command) {
        case GET_MDL_PROPERTIES:
            {
                struct mdl_properties properties;

                properties.rtB_len   = model->rtB_len;
                properties.rtB_cnt   = model->rtB_cnt;
                properties.numst     = rtw_model->numst;
                properties.rtP_size  = rtw_model->rtP_size;
                properties.base_rate = rtw_model->sample_period;
                strcpy(properties.so_path, rtw_model->so_path);

                rv = copy_to_user((void *)data, &properties, 
                        sizeof(properties));
                break;
            }

        case GET_WRITE_PTRS:
            {
                struct data_p data_p;

                /* Get the current write pointer. All data between the 
                 * current read and write pointer is valid. 
                 * In case the buffer overflowed, return -ENOSPC */
                if (model->wp) {
                    /* Get pointer. If write pointer has wrapped, return 
                     * end of data area. */
                    data_p.wp = model->wp >= model->rp 
                        ? model->wp - model->rtb_buf
                        : (model->rtb_last - model->rtb_buf);

                    /* Return time slice index */
                    data_p.wp /= model->rtB_len;
                } else {
                    /* You were sleeping !! */
                    data_p.wp = -ENOSPC;
                }

                data_p.msg = model->msg_ptr - model->msg_buf;

                rv = copy_to_user((void *)data, &data_p, sizeof(data_p));

                break;
            }

        case SET_BLOCKIO_RP:
            /* Set the buddy process' current read pointer. All data up to
             * this pointer has been processed, and the space can be
             * overwritten.
             * No errors */

            /* Convert time slice to offset in buffer */
            data *= model->rtB_len;

            if (data >= model->rtb_last - model->rtb_buf) {
                /* Ideally, the buddy does not know how long the buffer is
                 * (he could know using RTP_GET_SIZE and the size of one
                 * BlockIO), and just wants to read the next segment.
                 * However, this process knows the buffer size, and just wraps
                 * the read pointer to 0 if the buddy wants to read past 
                 * the buffer's end */
                data = 0;
            }
            model->rp = model->rtb_buf + data;

            /* Since the read pointer may have wrapped, return time slice of 
             * the real read pointer. */
            rv = data/model->rtB_len;
            break;

        case RESET_BLOCKIO_RP:
            /* Reset the buddy processes read pointer to the actual write
             * pointer. This is important, because logging stops when there
             * is a collision, i.e. the buddy process is dead 
             * Return the current write pointer
             * No Errors */
            model->rp = model->wp = model->rtb_buf;
            rv = 0;
            break;
            
        case CHECK_VER_STR:
            {
                size_t usr_ver_str_len, ver_str_len;
                char *user_ver_str;

                pr_debug("Checking ver str\n");

                /* First check whether MAX_MODEL_NAME_LEN is long enough! */
                ver_str_len = strlen(rtw_model->modelVersion) + 1;

                /* Find out how long the buddy's version string is */
                if ( !(usr_ver_str_len = strnlen_user((void *)data, 
                            ver_str_len))) {
                    pr_info("Internal error: Could not get version string "
                            "length from user space\n");
                    rv = -EFAULT;
                    break;
                }
                if (usr_ver_str_len != ver_str_len) {
                    /* The lengths don't match, so the versions can't be
                     * the same */
                    pr_info("Version string lengths do not match usr: "
                            "%i, kernel %i\n",
                            usr_ver_str_len, ver_str_len);
                    rv = -EACCES;
                    break;
                }
                if (!(user_ver_str = kmalloc(ver_str_len, GFP_KERNEL))) {
                    pr_debug("no memory\n");
                    rv = -ENOMEM;
                    break;
                }

                /* Make a copy of it */
                if ((rv = copy_from_user(user_ver_str, (void *)data, 
                                ver_str_len))) {
                    pr_info("Failed to copy %li bytes from user\n", rv);
                    rv = -ENOMEM;
                } else if (strcmp(user_ver_str, rtw_model->modelVersion)) {
                    pr_info("Version check failed: user version %s, "
                            "model version %s\n", 
                            user_ver_str, rtw_model->modelVersion);
                    rv = -EACCES;
                } else {
                    pr_debug("Version check worked\n");
                    model->version_checked = 1;
                }
                kfree(user_ver_str);

                break;
            }

        case GET_PARAM:
            /* Buddy process wants to get the complete parameter
             * set */
            if (copy_to_user((void *)data, rtw_model->mdl_rtP,
                    rtw_model->rtP_size)) {
                rv = -EFAULT;
                break;
            }
            pr_debug("Sent parameters to buddy\n");
            break;

        case NEW_PARAM:
            /* Copy a completely new parameter list from user space to 
             * the pending param space */

            /* Reject if the model has no parameter set */
            if (!rtw_model->rtP_size) {
                rv = -ENODEV;
                break;
            }

            if (!model->version_checked) {
                pr_info("Model version numbers not verified.\n");
                rv = -EACCES;
                break;
            }

            /* Steal lock if it has not yet been used */
            if (model->new_rtP) {
                rt_sem_wait(&model->rtP_sem);
                model->new_rtP = 0;
                rt_sem_signal(&model->rtP_sem);
            }

            /* Put parameters into a pending area. RT task
             * will pick it up from there */
            if (copy_from_user(rtw_model->pend_rtP, (void *)data, 
                    rtw_model->rtP_size)) {
                rv = -EFAULT;
            } else {
                model->new_rtP = 1;
            }

            pr_debug("Sent rt process new parameter set\n");
            break;

        case CHANGE_PARAM:
            /* Only a part of parameter set to be changed */
            {
                struct param_change p;
                struct change_ptr change_ptr;
                int i;

                /* Reject if the model has no parameter set */
                if (!rtw_model->rtP_size) {
                    rv = -ENODEV;
                    break;
                }

                if (!model->version_checked) {
                    pr_info("Model version numbers not verified.\n");
                    rv = -EACCES;
                    break;
                }

                /* Check if access to user space is ok. */
                if (copy_from_user(&p, (void *)data, sizeof(struct param_change))
                        || !access_ok(VERIFY_READ, p.rtP, rtw_model->rtP_size)
                        || (p.count && !access_ok(VERIFY_READ, p.changes, 
                                p.count*sizeof(struct change_ptr)))
                        ) {
                    rv = -EFAULT;
                    break;
                }

                /* If there was a new parameter set pending, delete the
                 * request and set it later. We then have access and don't
                 * have to keep the real_time task waiting */
                if (model->new_rtP) {
                    rt_sem_wait(&model->rtP_sem);
                    model->new_rtP = 0;
                    rt_sem_signal(&model->rtP_sem);
                }

                /* Copy the immediate change */
                if (p.pos + p.len > rtw_model->rtP_size) {
                    rv = -ERANGE;
                    break;
                }
                __copy_from_user(rtw_model->pend_rtP + p.pos,
                        p.rtP + p.pos, p.len);

                pr_debug("Setting %i bytes at offset %i for parameters\n", 
                        p.len, p.pos);

                /* Now go through the change list, if one exists */
                for( i = 0; i < p.count; i++) {
                    __copy_from_user(&change_ptr, &p.changes[i],
                            sizeof(struct change_ptr));
                    if (change_ptr.pos + change_ptr.len 
                            > rtw_model->rtP_size) {
                        memcpy(rtw_model->pend_rtP, rtw_model->mdl_rtP,
                                rtw_model->rtP_size);
                        rv = -ERANGE;
                        break;
                    }
                    __copy_from_user(
                            rtw_model->pend_rtP + change_ptr.pos, 
                            p.rtP + change_ptr.pos,
                            change_ptr.len);
                    pr_debug("Setting %i bytes at offset %i for parameters\n", 
                            change_ptr.len, change_ptr.pos);
                }

                /* Now tell the real time process of new parameter set */
                model->new_rtP = 1;
            }

            break;

        default:
            pr_info("ioctl() number %u unknown\n", command);
            rv = -ENOTTY;
            break;
    }

    return rv;
}
static unsigned int rtp_poll(
        struct file *filp,
        poll_table *wait
        ) {
    struct model *model = filp->private_data;
    unsigned int mask = 0;

    /* Wait for data */
    poll_wait(filp, &model->waitq, wait);

    /* File has data when read and write pointers are different or a 
     * new message is waiting */
    if (model->wp != model->rp || model->msg_ptr != model->msg_buf) {
        mask = POLLIN | POLLRDNORM;
    }

    return mask;
}
static struct file_operations rtp_fops = {
    .unlocked_ioctl =    rtp_ioctl,
//    .read =     rtp_read,
    .mmap =     rtp_mmap,
    .open =     rtp_open,
    .poll =     rtp_poll,
    .release =  rtp_release,
    .owner =    THIS_MODULE,
};

/* This is the Linux Kernel side helper for the poll function, because 
 * wake_up() cannot be called from inside the rt-task 
 */
void rtp_data_avail_handler(void)
{
    unsigned int model_id;

    down(&rt_kernel.lock);
    while ((model_id = ffs(rt_kernel.data_mask))) {
        model_id--;
        clear_bit(model_id, &rt_kernel.data_mask);
        //pr_debug("TID %u has data for buddy\n", model_id);

        wake_up_interruptible( &rt_kernel.model[model_id]->waitq);
    }
    up(&rt_kernel.lock);
}

/* This makes a snapshot of the current process and places this slice
 * in rtb_buf after the last one */
void rtp_make_photo(struct model *model)
{
    const struct rtw_model *rtw_model = model->rtw_model;

    rt_sem_wait(&model->buf_sem);

    /* Only take a photo when wp is valid. To stop taking photos (e.g.
     * when not initialised or when the buffer is full), set wp to NULL */
    if (model->wp) {
        memcpy(model->wp, rtw_model->mdl_rtB, rtw_model->rtB_size);
        memcpy(model->wp + rtw_model->rtB_size, model->task_stats,
                model->task_stats_len);

        /* Update write pointer, wrapping if necessary */
        model->wp += model->rtB_len;
        if (model->wp == model->rtb_last) {
            model->wp = model->rtb_buf;
        }

        /* Stop if the write pointer caught up to the read pointer */
        if (model->wp == model->rp) {
            model->wp = NULL;
            pr_info("rtp_buf full\n");
        }
    }
    rt_sem_signal(&model->buf_sem);

    /* Send a signal that data is available */
    set_bit(model->id, &rt_kernel.data_mask);
}


/*#########################################################################*
 * Here is general management code to initialise a new Real-Time Workshop
 * model that is inserted into the Kernel.
 *#########################################################################*/

/* Free file handles of a specific RTW Model */
void rtp_fio_clear_mdl(struct model *model)
{
    pr_debug("Tearing down FIO for model_id %u\n", model->id);

    clear_bit(model->id, &rt_kernel.data_mask);
    cdev_del(&model->rtp_dev);

    /* Wake the buddy to tell that there is a new process */
    rt_kernel.model_state_changed = 1;
    wake_up_interruptible(&rt_kernel.event_q);
}

/** 
 * Here all the file operations for the communication with the buddy 
 * are initialised for the new RTW Model */
int rtp_fio_init_mdl(struct model *model, struct module *owner)
{
    dev_t devno;
    int major = MAJOR(rt_kernel.dev), minor = MINOR(rt_kernel.dev);
    int err = -1;

    pr_debug("Initialising FIO for model_id %u\n", model->id);

    rt_sem_init(&model->buf_sem,1); /* Lock the BlockIO buffer semaphore */
    init_MUTEX(&model->buddy_lock);
    init_waitqueue_head(&model->waitq);
    model->wp = NULL;

    /* Character device for BlockIO stream */
    cdev_init(&model->rtp_dev, &rtp_fops);
    model->rtp_dev.owner = owner;
    devno = MKDEV(major, minor + model->id + 1);
    if ((err = cdev_add(&model->rtp_dev, devno, 1))) {
        printk("Could not add Process IO FOPS to cdev\n");
        goto out_add_rtp;
    }
    pr_debug("Added char dev for BlockIO, minor %u\n", MINOR(devno));

    /* Wake the buddy to tell that there is a new process */
    rt_kernel.model_state_changed = 1;
    wake_up_interruptible(&rt_kernel.event_q);

    return 0;

    cdev_del(&model->rtp_dev);
out_add_rtp:
    return err;
}


/*#########################################################################*
 * Here the file operations to control the Real-Time Kernel are defined.
 * The buddy needs this to be able to change things in the Kernel.
 *#########################################################################*/
static int rtp_main_open(
        struct inode *inode,
        struct file *filp
        ) {
    int err = 0;

    if (down_trylock(&rt_kernel.file_lock)) {
        err = -EBUSY;
    }
    rt_kernel.model_state_changed = 0;

    return err;
}
static int rtp_main_release(
        struct inode *inode,
        struct file *filp
        ) {
    up(&rt_kernel.file_lock);
    return 0;
}
static unsigned int rtp_main_poll(
        struct file *filp,
        poll_table *wait
        ) {
    unsigned int mask = 0;

    /* Wait for data */
    poll_wait(filp, &rt_kernel.event_q, wait);

    /* Check whether there was a change in the states of a Real-Time Process */
    if (rt_kernel.model_state_changed) {
        mask = POLLIN | POLLRDNORM;
        rt_kernel.model_state_changed = 0;
    }

    return mask;
}

static long rtp_main_ioctl(
        struct file *filp,
        unsigned int command,
        unsigned long data
        ) {
    long rv = 0;

    pr_debug("rt_kernel ioctl command %#x, data %lu\n", command, data);

    down(&rt_kernel.lock);
    switch (command) {
        case RTK_GET_ACTIVE_MODELS:
            rv = put_user(rt_kernel.loaded_models, (uint32_t *)data);
            break;

        case RTK_MODEL_NAME:
            {
                struct rtp_model_name *rtp_model_name = 
                    (struct rtp_model_name *)data;
                typeof(rtp_model_name->number) mdl_number;
                const char *name;

                if ((rv = get_user(mdl_number, &rtp_model_name->number)))
                    break;
                if (mdl_number >= MAX_MODELS || 
                        !test_bit(mdl_number, &rt_kernel.loaded_models)) {
                    pr_debug("Requested model number %i does not exist\n", 
                            mdl_number);
                    rv = -ENODEV;
                    break;
                }

                name = rt_kernel.model[mdl_number]->rtw_model->modelName;

                // Model name length was checked to be less than 
                // MAX_MODEL_NAME_LEN when it was registered
                rv = copy_to_user(rtp_model_name->name, name, strlen(name)+1);
                break;
            }

        default:
            rv = -ENOTTY;
            break;
    }
    up(&rt_kernel.lock);

    return rv;
}

static struct file_operations kernel_fops = {
    .unlocked_ioctl = rtp_main_ioctl,
    .poll           = rtp_main_poll,
    .open           = rtp_main_open,
    .release        = rtp_main_release,
    .owner          = THIS_MODULE,
};


/*#########################################################################*
 * Here is general management code to initialise file handles for the 
 * Real-Time Kernel.
 *#########################################################################*/

/* Clear the Real-Time Kernel file handles */
void rtp_fio_clear(void)
{
    pr_debug("Tearing down FIO for rt_kernel\n");

    cdev_del(&rt_kernel.buddy_dev);
    unregister_chrdev_region(rt_kernel.dev, rt_kernel.chrdev_cnt);
}

/* Set up the Real-Time Kernel file handles. This is called once when
 * the rt_kernel is loaded, and opens up the char device for 
 * communication between buddy and rt_kernel */
int rtp_fio_init(void)
{
    int err = -1;

    pr_debug("Initialising FIO for rt_kernel\n");

    rt_kernel.model_state_changed = 0;

    /* Create character devices for this process. Each RT model needs 1
     * char device, and rt_kernel needs 1 */
    rt_kernel.chrdev_cnt = MAX_MODELS+1;
    if ((err = alloc_chrdev_region(&rt_kernel.dev, 0, 
                    rt_kernel.chrdev_cnt, "rt_kernel"))) {
        printk("Could not allocate character device\n");
        goto out_chrdev;
    }
    pr_debug("Reserved char dev for rt_kernel, major %u, minor start %u\n",
            MAJOR(rt_kernel.dev), MINOR(rt_kernel.dev));

    cdev_init(&rt_kernel.buddy_dev, &kernel_fops);
    rt_kernel.buddy_dev.owner = THIS_MODULE;
    if ((err = cdev_add(&rt_kernel.buddy_dev, rt_kernel.dev, 1))) {
        printk("Could not add Process IO FOPS to cdev\n");
        goto out_add_rtp;
    }
    pr_debug("Started char dev for rt_kernel\n");

    return 0;

    cdev_del(&rt_kernel.buddy_dev);
out_add_rtp:
    unregister_chrdev_region(rt_kernel.dev, rt_kernel.chrdev_cnt);
out_chrdev:
    return err;
}

