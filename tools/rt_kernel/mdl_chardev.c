/******************************************************************************
 *
 * $Id$
 *
 * Every Real-Time model opens a character device to communicates with the 
 * user space buddy. The code here implements this char dev
 * 
 * Copyright (C) 2008  Richard Hacker
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
#include <linux/device.h>       /* class_create(), etc. */
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
    const struct rt_model *rt_model = model->rt_model;

    if (down_trylock(&model->buddy_lock)) {
        return -EBUSY;
    }
    filp->private_data = model;

    /* Get some memory to store snapshots of the BlockIO vector */
    model->rtB_cnt = (rt_model->buffer_time > rt_model->sample_period)
        ? rt_model->buffer_time/rt_model->sample_period : 1;
    model->rtB_len = rt_model->rtB_size + model->task_stats_len;
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
    const struct rt_model *rt_model = model->rt_model;
    long rv = 0;

    switch (command) {
        case GET_MDL_PROPERTIES:
            {
                struct mdl_properties properties;

                properties.signal_count  = rt_model->signal_count;
                properties.param_count   = rt_model->param_count;
                properties.variable_path_len  = rt_model->variable_path_len;
                properties.sample_period = rt_model->sample_period;
                properties.num_tasks     = rt_model->num_tasks;

                properties.rtB_count     = rt_model->rtB_count;
                properties.rtB_size      = rt_model->rtB_size
                    + rt_model->num_tasks * sizeof(struct task_stats);
                properties.rtP_size      = rt_model->rtP_size;

                strncpy(properties.name, rt_model->modelName, 
                        sizeof(properties.name));
                properties.name[sizeof(properties.name)-1] = '\0';

                strncpy(properties.version, rt_model->modelVersion, 
                        sizeof(properties.version));
                properties.name[sizeof(properties.version)-1] = '\0';

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
            
        case GET_SIGNAL_INFO:
        case GET_PARAM_INFO:
            /* Get properties of signal or parameter */
            {
                struct signal_info si;
                char *user_path;
                char *local_path;
                const char *err;

                // Get the index the user is interested in
                if (copy_from_user(&si, (void*)data, sizeof(si))) {
                    rv = -EFAULT;
                    break;
                }

                /* Save the pointer to the user's path string, and make
                 * si.path point to our own memory, since we cannot write
                 * to user space directly*/
                user_path = si.path;
                si.path = local_path = kmalloc(si.path_buf_len, GFP_KERNEL);
                if (!local_path) {
                    rv = -ENOMEM;
                    break;
                }

                /* Get the information from the model */
                err = (command == GET_SIGNAL_INFO)
                    ? rt_model->get_signal_info(&si)
                    : rt_model->get_param_info(&si);
                if (err) {
                    printk("Error: %s\n", err);
                    rv = -ERANGE;
                }
                else if (copy_to_user(user_path, local_path, si.path_buf_len)) {
                        rv = -EFAULT;
                }
                else {
                    /* Replace user's path, otherwise he will be surprised */
                    si.path = user_path;
                    if (copy_to_user((void*)data, &si, sizeof(si))) {
                        rv = -EFAULT;
                    }
                }
                kfree(local_path);
            }
            break;

        case GET_PARAM:
            /* Buddy process wants to get the complete parameter
             * set */
            if (copy_to_user((void *)data, rt_model->mdl_rtP,
                    rt_model->rtP_size)) {
                rv = -EFAULT;
                break;
            }
            pr_debug("Sent parameters to buddy\n");
            break;

        case NEW_PARAM:
            /* Copy a completely new parameter list from user space to 
             * the pending param space */

            /* Reject if the model has no parameter set */
            if (!rt_model->rtP_size) {
                rv = -ENODEV;
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
            if (copy_from_user(rt_model->pend_rtP, (void *)data, 
                    rt_model->rtP_size)) {
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
                if (!rt_model->rtP_size) {
                    rv = -ENODEV;
                    break;
                }

                /* Check if access to user space is ok. */
                if (copy_from_user(&p, (void *)data, sizeof(struct param_change))
                        || !access_ok(VERIFY_READ, p.rtP, rt_model->rtP_size)
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
                if (p.pos + p.len > rt_model->rtP_size) {
                    rv = -ERANGE;
                    break;
                }
                __copy_from_user(rt_model->pend_rtP + p.pos,
                        p.rtP + p.pos, p.len);

                pr_debug("Setting %i bytes at offset %i for parameters\n", 
                        p.len, p.pos);

                /* Now go through the change list, if one exists */
                for( i = 0; i < p.count; i++) {
                    __copy_from_user(&change_ptr, &p.changes[i],
                            sizeof(struct change_ptr));
                    if (change_ptr.pos + change_ptr.len 
                            > rt_model->rtP_size) {
                        memcpy(rt_model->pend_rtP, rt_model->mdl_rtP,
                                rt_model->rtP_size);
                        rv = -ERANGE;
                        break;
                    }
                    __copy_from_user(
                            rt_model->pend_rtP + change_ptr.pos, 
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
    const struct rt_model *rt_model = model->rt_model;

    rt_sem_wait(&model->buf_sem);

    /* Only take a photo when wp is valid. To stop taking photos (e.g.
     * when not initialised or when the buffer is full), set wp to NULL */
    if (model->wp) {
        memcpy(model->wp, rt_model->mdl_rtB, rt_model->rtB_size);
        memcpy(model->wp + rt_model->rtB_size, model->task_stats,
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
    class_device_destroy(rt_kernel.sysfs_class, 
            rt_kernel.dev + model->id + 1);
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
    int err = -1;

    pr_debug("Initialising FIO for model_id %u\n", model->id);

    rt_sem_init(&model->buf_sem,1); /* Lock the BlockIO buffer semaphore */
    init_MUTEX(&model->buddy_lock);
    init_waitqueue_head(&model->waitq);
    model->wp = NULL;

    /* Character device for BlockIO stream */
    cdev_init(&model->rtp_dev, &rtp_fops);
    model->rtp_dev.owner = owner;
    devno = rt_kernel.dev + model->id + 1;
    if ((err = cdev_add(&model->rtp_dev, devno, 1))) {
        printk("Could not add Process IO FOPS to cdev\n");
        goto out_add_rtp;
    }
    pr_debug("Added char dev for BlockIO, minor %u\n", MINOR(devno));

    model->sysfs_dev = class_device_create(rt_kernel.sysfs_class, NULL, 
            devno, NULL, "etl%d", model->id + 1);
    if (IS_ERR(model->sysfs_dev)) {
        printk("Could not create device etl0.\n");
        err = PTR_ERR(model->sysfs_dev);
        goto out_class_device_create;
    }

    /* Wake the buddy to tell that there is a new process */
    rt_kernel.model_state_changed = 1;
    wake_up_interruptible(&rt_kernel.event_q);

    return 0;

    class_device_destroy(rt_kernel.sysfs_class, devno);
out_class_device_create:
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

                name = rt_kernel.model[mdl_number]->rt_model->modelName;

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

    class_device_destroy(rt_kernel.sysfs_class, rt_kernel.dev);
    cdev_del(&rt_kernel.buddy_dev);
    unregister_chrdev_region(rt_kernel.dev, rt_kernel.chrdev_cnt);
    class_destroy(rt_kernel.sysfs_class);
}

/* Set up the Real-Time Kernel file handles. This is called once when
 * the rt_kernel is loaded, and opens up the char device for 
 * communication between buddy and rt_kernel */
int rtp_fio_init(void)
{
    int err = -1;

    pr_debug("Initialising FIO for rt_kernel\n");

    rt_kernel.model_state_changed = 0;

    rt_kernel.sysfs_class = class_create(THIS_MODULE, "rt_kernel");
    if (IS_ERR(rt_kernel.sysfs_class)) {
        printk("Could not create SysFS class structure.\n");
        err = PTR_ERR(rt_kernel.sysfs_class);
        goto out_class_create;
    }

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

    rt_kernel.sysfs_dev = class_device_create(rt_kernel.sysfs_class, NULL, 
            rt_kernel.dev, NULL, "etl0");
    if (IS_ERR(rt_kernel.sysfs_dev)) {
        printk("Could not create device etl0.\n");
        err = PTR_ERR(rt_kernel.sysfs_dev);
        goto out_class_device_create;
    }

    return 0;

    class_device_destroy(rt_kernel.sysfs_class, rt_kernel.dev);
out_class_device_create:
    cdev_del(&rt_kernel.buddy_dev);
out_add_rtp:
    unregister_chrdev_region(rt_kernel.dev, rt_kernel.chrdev_cnt);
out_chrdev:
    class_destroy(rt_kernel.sysfs_class);
out_class_create:
    return err;
}

