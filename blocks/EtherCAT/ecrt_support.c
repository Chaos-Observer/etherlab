#include <linux/list.h>
#include <linux/timex.h>
#include "rtai_sem.h"
#include "ecrt_support.h"

struct rt_ec_domain {
    struct list_head list;
    cycles_t next_hit;
    unsigned int tick;
};

struct rt_ec_dev {
    struct list_head domain_list;
    int buffer_time;
    SEM ec_dev_lock;
};

int
ext_lock_dev(void *data)
{
    struct rt_ec_dev *rt_ec_dev = data;
    cycles_t now;
    struct rt_ec_domain *ptr;

    now = get_cycles();

    list_for_each_entry(ptr,&rt_ec_dev->domain_list,list) {
        if ((int)(ptr->next_hit - now) < rt_ec_dev->buffer_time)
            return -1;
    }
    rt_sem_wait(&rt_ec_dev->ec_dev_lock);
    return 0;
}

void
ext_unlock_dev(void *data)
{
    struct rt_ec_dev *rt_ec_dev = data;

    rt_sem_signal(&rt_ec_dev->ec_dev_lock);
}

void 
ethercat_support_lock_master(struct rt_ec_dev *rt_ec_dev)
{
    rt_sem_wait(&rt_ec_dev->ec_dev_lock);
}

void 
ethercat_support_unlock_master(struct rt_ec_dev *rt_ec_dev)
{
    rt_sem_signal(&rt_ec_dev->ec_dev_lock);
}

struct rt_ec_domain *
register_ec_domain(struct rt_ec_dev *rt_ec_dev, unsigned int tick)
{
    struct rt_ec_domain *rt_ec_domain;

    rt_ec_domain = kmalloc(sizeof(struct rt_ec_domain), GFP_KERNEL);
    if (!rt_ec_domain)
        return NULL;

    list_add_tail(&rt_ec_domain->list, &rt_ec_dev->domain_list);
    rt_ec_domain->tick = tick;
    rt_ec_domain->next_hit = 0;

    return rt_ec_domain;
}

struct rt_ec_dev *
register_ec_master(unsigned int dev_no)
{
    struct rt_ec_dev *rt_ec_dev;

    rt_ec_dev = kmalloc(sizeof(struct rt_ec_dev), GFP_KERNEL);
    if (rt_ec_dev) {
        goto out_kmalloc;
    }

    INIT_LIST_HEAD(&rt_ec_dev->domain_list);
    rt_ec_dev->buffer_time = 50;
    rt_sem_init(&rt_ec_dev->ec_dev_lock,1);

    return rt_ec_dev;

out_kmalloc:
    return NULL;
}

void
ethercat_support_free_master(struct rt_ec_dev *rt_ec_dev)
{
    struct rt_ec_domain *ptr, *ptr1;
    list_for_each_entry_safe(ptr, ptr1, &rt_ec_dev->domain_list, list) {
        kfree(ptr);
    }
    kfree(rt_ec_dev);
}
