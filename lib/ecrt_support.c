/* This is the EtherCAT support layer for EtherLab. It is linked to every 
 * model and deals with the complexity of registering EtherCAT terminals 
 * on different EtherCAT masters for various sample times.
 * 
 * The code is used as follows:
 *      - Initialise structures using ecs_init()
 *      - Register EtherCAT slaves using ecs_reg_pdo() or ecs_reg_pdo_range()
 *      - If a slave requires SDO configuration objects, register them 
 *        using ecs_reg_sdo()
 *      - Before going into the cyclic real time mode, call ecs_start().
 *      - In the cyclic mode, call ecs_process() to input new values
 *        and ecs_send() to write new values to the EtherCAT terminals.
 *      - When finished, call ecs_end()
 *
 * See the individual functions for more details.
 * */

#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#error "Don't have config.h"
#endif

#define DEBUG 1

#include <linux/types.h>
#include <linux/list.h>
#include <asm/timex.h>
#include "rtai_sem.h"
#include "ecrt_support.h"

/* The following message gets repeated quite frequently. */
char *no_mem_msg = "Could not allocate memory";
char errbuf[256];

/* Structure to temporarily store SDO objects prior to registration */
struct sdo_init {
    /* Linked list */
    struct list_head list;

    /* The data type of the sdo */
    enum {sdo8, sdo16, sdo32} type;

    /* SDO values. Used by EtherCAT functions */
    uint16_t sdo_index;
    uint8_t sdo_subindex;
    union {
        uint32_t u32;
        uint16_t u16;
        uint8_t u8;
    } value;
};

/* Structure to temporarily store PDO objects prior to registration */
struct ecat_pdo {
    struct list_head list;      /* Linked list of pdo's */

    enum {pdo_typed, pdo_range} type;

    /* This structure is used with both pdo types */
    const char *slave_address; /**< slave address string
                                 \see ec_master_parse_slave_address() */
    uint32_t vendor_id; /**< vendor ID */
    uint32_t product_code; /**< product code */

    uint16_t pdo_entry_index;
    uint8_t pdo_entry_subindex;

    /* This data is used only with pdo_range */
    ec_direction_t pdo_direction;
    uint16_t pdo_offset;
    uint16_t pdo_length;

    void **data_ptr;

    /* Set to 1 if this is an output */
    unsigned int output;
};


/* Structure to temporarily store PDO objects prior to registration */
struct ecat_slave_block {
    struct list_head list;      /* Linked list of slave blocks */

    struct ecat_domain *domain;

    const char *slave_address; /**< slave address string
                                 \see ec_master_parse_slave_address() */
    uint32_t vendor_id; /**< vendor ID */
    uint32_t product_code; /**< product code */

    struct list_head sdo_list;
    struct list_head pdo_entry_list;

    /* Used to remap PDO's */
    uint16_t *RxPDO;
    uint16_t *TxPDO;

    /* Set to 1 if this is an output */
    unsigned int output;
};

/* Every domain has one of these structures. There can exist exactly one
 * domain for a sample time on an EtherCAT Master. */
struct ecat_domain {
    struct list_head list;      /* Linked list of domains. This list
                                 * is only used prior to ecs_start(), where
                                 * it is reworked into an array for faster
                                 * access */

    ec_domain_t *handle;        /* used when calling EtherCAT functions */

    struct ecat_master *master; /* The master this domain is registered
                                 * in. Every domain has exactly 1 master */

    /* The following list is used for most cases where a slave is not configurable
     * and has just one input or output */
    struct list_head simple_pdo_list;

    /* For enhanced slaves with configuration and/or multiple inputs and outputs
     * the matter becomes a little bit trickier. These slaves are parked here */
    struct list_head slave_block_list;

    unsigned int tid;           /* Id of the corresponding RealTime Workshop
                                 * task */
};

/* For every EtherCAT Master, one such structure exists */
struct ecat_master {
    struct list_head list;      /* Linked list of masters. This list
                                 * is only used prior to ecs_start(), where
                                 * it is reworked into an array for faster
                                 * access */

    struct list_head domain_list; /* The list to temprarily store domains 
                                   * that are registered on this master. This
                                   * list is reworked during ecs_start() */

    unsigned int fastest_tid;   /* Fastest RTW task id that uses this 
                                 * master */

    unsigned int id;            /* Id of the EtherCAT master */
    ec_master_t *handle;        /* Handle retured by EtherCAT code */

    SEM lock;                   /* Lock the ecat master */
    cycles_t last_call;     /* TSC of last call. Need this to decide
                                 * whether it is allowable to let others
                                 * use the ecat card */
    cycles_t grace_cycles;  /* Grant the use of the network card if the
                                 * call comes within this grace period 
                                 * since last call */

    unsigned int slave_count;  /* Roughly keep track how many slaves are 
                                * registered for this master. The value here
                                * can be too high when the same slave is 
                                * registered more than once. */
};

/* Data used by the EtherCAT support layer */
struct ecat_data {
    unsigned int nst;           /* Number of unique RTW sample times */

    /* This list is used to store all masters during the registration
     * process. Thereafter this list is reworked in ecs_start(), moving 
     * the masters and domains into the st[] struct below */
    struct list_head master_list;

    struct {
        unsigned int master_count;
        unsigned int domain_count;

        /* Array of masters and domains in this sample time. The data area
         * are slices out of st_data */
        struct ecat_master *master;
        struct ecat_domain *domain;

        /* The tick period in microseconds for this sample time */
        unsigned int period;
    } st[];
} *ecat_data;

/////////////////////////////////////////////////
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,17)
void *
kzalloc(size_t len, int type)
{
    void *p;
    if ((p = kmalloc(len,type))) {
        memset(p, 0, len);
    }
    return p;
}
#endif
/////////////////////////////////////////////////

/* Do input processing for a RTW task. It does the following:
 *      - calls ecrt_master_receive() for every master whose fastest 
 *        domain is in this task. 
 *      - calls ecrt_domain_process() for every domain in this task
 */
void
ecs_receive(int tid)
{
    struct ecat_master *master;
    struct ecat_domain *domain;
    unsigned int i;
        
    for (i = 0; i < ecat_data->st[tid].master_count; i++) {
        master = &ecat_data->st[tid].master[i];
        if (tid != master->fastest_tid) 
            continue;
        rt_sem_wait(&master->lock);
        ecrt_master_receive(master->handle);
//        pr_debug("\ttid %i: ecrt_master_receive(%p)\n", 
//                tid, master->handle);
        rt_sem_signal(&master->lock);
    }

    for (i = 0; i < ecat_data->st[tid].domain_count; i++) {
        domain = &ecat_data->st[tid].domain[i];
        rt_sem_wait(&domain->master->lock);
        ecrt_domain_process(domain->handle);
//        pr_debug("\ttid %i: ecrt_domain_process(%p)\n", 
//                tid, domain->handle);
        rt_sem_signal(&domain->master->lock);
    }
}

/* Do EtherCAT output processing for a RTW task. It does the following:
 *      - calls ecrt_master_run() for every master whose fastest task
 *        domain is in this task
 *      - calls ecrt_master_send() for every domain in this task
 */
void
ecs_send(int tid) 
{
    struct ecat_master *master;
    struct ecat_domain *domain;
    unsigned int i;

    for (i = 0; i < ecat_data->st[tid].domain_count; i++) {
        domain = &ecat_data->st[tid].domain[i];
        rt_sem_wait(&domain->master->lock);
        ecrt_domain_queue(domain->handle);
//        pr_debug("\ttid %i: ecrt_domain_queue(%p)\n", 
//                tid, domain->handle);
        rt_sem_signal(&domain->master->lock);
    }

    for (i = 0; i < ecat_data->st[tid].master_count; i++) {
        master = &ecat_data->st[tid].master[i];
        master->last_call = get_cycles();
        rt_sem_wait(&master->lock);
        ecrt_master_send(master->handle);
//        pr_debug("\ttid %i: ecrt_master_send(%p)\n", 
//                tid, master->handle);
        rt_sem_signal(&master->lock);
    }
}

/* This callback is passed to EtherCAT during master initialisation. 
 *
 * Normally it is not allowable for anyone else to use the master other
 * than the owner himself to guarantee hard real-time response. In rare
 * cases, however, it may be necessary for EtherCAT to request private usage
 * of the master, for example to do EoE.
 * Using this function, EtherCAT has the chance to gain temporary exclusive 
 * use of the master.
 */
int
ecs_request_cb(void *p)
{
#define MASTER ((struct ecat_master *)p)

    /* Make sure that the master is not required in the near future */
    if ((get_cycles() - MASTER->last_call) > MASTER->grace_cycles) {
        pr_debug("ecs_request_cb() %i cycles left, wanted %llu, last call %llu\n", 
                (int)(get_cycles() - MASTER->last_call), 
                MASTER->grace_cycles, 
                MASTER->last_call);
        return -1;
    }
    rt_sem_wait(&MASTER->lock);
    return 0;
#undef MASTER
}

/* This callback is passed to EtherCAT during master initialisation. 
 *
 * With this function the master is released again.
 */
void
ecs_release_cb(void *p)
{
#define MASTER ((struct ecat_master *)p)
    rt_sem_signal(&MASTER->lock);
#undef MASTER
}

/*
 * Release all memory and free EtherCAT master
 */
void
ecs_end(void)
{
    unsigned int i, j;
    void *st_data = NULL;
    struct ecat_master *master, *n1;
    struct ecat_domain *domain, *n2;
    struct ecat_pdo *pdo, *n3;
    struct ecat_slave_block *slave_block, *n4;
    struct sdo_init *sdo, *n5;

    if (!ecat_data)
        return;

    list_for_each_entry_safe(master, n1, &ecat_data->master_list, list) {
        if (master->handle) {
            pr_debug("\tManually releasing master %p\n", master->handle);
            ecrt_release_master(master->handle);
        }
        list_for_each_entry_safe(domain, n2, &master->domain_list, list) {
            list_for_each_entry_safe(pdo, n3, &domain->simple_pdo_list, list) {
                pr_debug("Freeing PDO %p\n", pdo);
                kfree(pdo);
            }
            list_for_each_entry_safe(slave_block, n4, 
                    &domain->slave_block_list, list) {
                list_for_each_entry_safe(sdo, n5, 
                        &slave_block->sdo_list, list) {
                    pr_debug("Freeing SDO %p\n", sdo);
                    kfree(sdo);
                }
                pr_debug("Freeing SlaveBlock %p\n", slave_block);
                kfree(slave_block);
            }
            pr_debug("Freeing Domain %p\n", domain);
            kfree(domain);
        }
        pr_debug("Freeing Master %p\n", master);
        kfree(master);
    }

    pr_debug("going through %u sample times to find masters to release\n", 
            ecat_data->nst);
    for( i = 0; i < ecat_data->nst; i++) {
        pr_debug("going through %u masters \n", 
                ecat_data->st[i].master_count);
        for( j = 0; j < ecat_data->st[i].master_count; j++) {
            if (!ecat_data->st[i].master) {
                /* This check is detects whether a malloc of the master/domain
                 * structures has failed. In this case
                 * ecat_data->st[i].master is NULL and there is nothing else
                 * to do here */
                break;
            }
            if (!st_data) {
                /* The first master in the first sample time the start of 
                 * the sample time memory - the efficient array that was defined 
                 * in ecs_start() */
                st_data = &ecat_data->st[i].master[j];
            }
            pr_debug("\tecrt_release_master(%p)\n",
                    ecat_data->st[i].master[j].handle);
            ecrt_release_master(ecat_data->st[i].master[j].handle);
        }
    }

    if (st_data) {
        pr_debug("Releasing sample time data %p\n", st_data);
        kfree(st_data);
    }

    /* Don't need to check for null here, it is done right above */
    pr_debug("Releasing ecat_data %p\n", ecat_data);
    kfree(ecat_data);
}

/*
 * Start EtherCAT driver.
 *
 * This function concludes the caching of all slave initialisation and 
 * registration items. Now the EtherCAT driver can be contacted to activate
 * the subsystem.
 */
const char *
ecs_start(void)
{
    struct ecat_master *master, *n1;
    struct ecat_domain *domain, *n2;
    struct ecat_pdo *pdo, *n3;
    struct ecat_slave_block *slave_block, *n4;
    struct sdo_init *sdo, *n5;
    ec_slave_t *slave_ptr;
    void *ptr;
    uint16_t *p;
    size_t len;
    unsigned int i, j;
    unsigned int master_tid = 0;
    unsigned int total_masters = 0, total_domains = 0;
    int error;
    struct ecat_slave {
        ec_slave_t *slave_ptr;
        unsigned int locked;
    } *slaves;



    /* Count the number of masters and their domains so that it is possible
     * to allocate memory for their data structures. Although memory has 
     * already been allocated for these, the aim here is to allocate a 
     * contiguous memory block and access them using array indexing instead
     * of crawling though a linked list. Hope this is more efficient ;-)
     */
    list_for_each_entry(master, &ecat_data->master_list, list) {
        total_masters++;
        list_for_each_entry(domain, &master->domain_list, list) {
            ecat_data->st[domain->tid].master_count++;
            total_domains++;
        }
    }
    pr_debug("Total number of masters = %u, domains = %u\n", 
            total_masters, total_domains);

    /* Now that the master and domain counts are known, we can
     * get a large block of contiguous memory where the master and domain
     * structures are kept. This is better for caching, and list operators
     * are replaced with array operators */
    len = total_masters*sizeof(struct ecat_master)
        + total_domains*sizeof(struct ecat_domain);
    if (!(ptr = kmalloc(len, GFP_KERNEL))) {
        return no_mem_msg;
    }

    /* Subdivide the newly allocated space into the sample times */
    for( i = 0; i < ecat_data->nst; i++) {
        if (ecat_data->st[i].master_count) {
            ecat_data->st[i].master = ptr;
            ptr += sizeof(struct ecat_master)*ecat_data->st[i].master_count;
            ecat_data->st[i].master_count = 0;
        } else {
            ecat_data->st[i].master = NULL;
        }

        if (ecat_data->st[i].domain_count) {
            ecat_data->st[i].domain = ptr;
            ptr += sizeof(struct ecat_domain)*ecat_data->st[i].domain_count;
            ecat_data->st[i].domain_count = 0;
        } else {
            ecat_data->st[i].domain = NULL;
        }
    }

    /* OK, take a deep breath, you will need to bear with me here.
     * Now starting from master_list, go through the domains, and register
     * all of simple_pdo_list.
     * Going back to the domains, loop through slave_block_list, registering
     * its pdo_list, its sdo_list, and possibly RxPDO/TxPDO */

    /* Note that here we need to use list_for_each_entry_safe() because
     * the list members are removed one by one */
    list_for_each_entry_safe(master, n1, &ecat_data->master_list, list) {
        unsigned int master_idx;
        struct ecat_master *new_master;
        unsigned int slave_count = 0;
        unsigned int slave_idx = 0;

        /* Find out to which tid this master will be allocated. The fastest
         * tid will get it */
        master_tid = master->fastest_tid;
        master_idx = ecat_data->st[master_tid].master_count++;
        new_master = &ecat_data->st[master_tid].master[master_idx];

        /* Slaves is a temporary array where all registered slaves are kept.
         * The user has many ways of addressing a slave and the only way to 
         * detect whether two slaves are the same is when the master returns
         * the same pointer for both slaves. That is why the slaves are 
         * temporarily stored here */
        slaves = kzalloc(
                sizeof(struct ecat_slave) * master->slave_count, GFP_KERNEL);
        if (!slaves) {
            snprintf(errbuf, sizeof(errbuf), "%s", no_mem_msg);
            goto out_alloc_slaves;
        }

        /* Loop through all of the masters domains */
        list_for_each_entry_safe(domain, n2, &master->domain_list, list) {
            unsigned int tid, domain_idx;

            /* Go though the domain's simple_pdo_list. These slaves will not
             * be configured, i.e. no SDO's and TxPDO/RxPDO */
            pr_debug("Doing simple slave configuration\n");
            list_for_each_entry_safe(pdo, n3, &domain->simple_pdo_list, list) {
                /* Depending on the type of pdo (typed or range), have
                 * different calls to EtherCAT */
                pr_debug("Considering slave %s\n", pdo->slave_address);
                slave_ptr = ecrt_master_get_slave(master->handle, 
                        pdo->slave_address,
                        pdo->vendor_id,
                        pdo->product_code);
                if (!slave_ptr) {
                    snprintf(errbuf, sizeof(errbuf), 
                            "EtherCAT get_slave() %s on Master %u failed.",
                            pdo->slave_address, master->id);
                    goto out_get_slave;
                }

                /* Find out whether this slave has already been registered. */
                for (slave_idx = 0; slave_idx < slave_count; slave_idx++) {
                    if (slaves[slave_idx].slave_ptr == slave_ptr) {
                        pr_debug("Slave has been registered already\n");
                        break;
                    }
                }
                if (slave_idx < slave_count) {
                    /* Slave has already been registered, now check whether
                     * either the previous slave or this new slave id
                     * either configured or is an output.
                     * */
                    if (slaves[slave_idx].locked || pdo->output) {
                        snprintf(errbuf, sizeof(errbuf), 
                                "Cannot register EtherCAT slave %s on Master %u "
                                "twice as output.",
                                pdo->slave_address, master->id);
//                        goto out_get_slave;
                    }
                } else {
                    /* Never seen this slave before */
                    slaves[slave_count].slave_ptr = slave_ptr;
                    if (pdo->output)
                        slaves[slave_count].locked = 1;
                    slave_count++;
                    pr_debug("Slave has never been registered\n");
                }

                switch(pdo->type) {
                    case pdo_typed:
                        error = ecrt_domain_register_pdo(
                                domain->handle, slave_ptr,
                                pdo->pdo_entry_index, 
                                pdo->pdo_entry_subindex, 
                                pdo->data_ptr);
                        pr_debug("\t%p = ecrt_domain_register_pdo("
                                "%p, \"%s\", %i, %i, %i, %i, %p)\n", slave_ptr,
                                domain->handle,
                                pdo->slave_address, pdo->vendor_id, 
                                pdo->product_code, pdo->pdo_entry_index, 
                                pdo->pdo_entry_subindex, pdo->data_ptr);
                        break;
                    case pdo_range:
                        error = ecrt_domain_register_pdo_range(
                                domain->handle, slave_ptr,
                                pdo->pdo_direction, 
                                pdo->pdo_offset, pdo->pdo_length, 
                                pdo->data_ptr);
                        pr_debug("\t%p = ecrt_domain_register_pdo_range("
                                "%p, \"%s\", %i, %i, %i, %i, %i, %p)\n", slave_ptr,
                                domain->handle,
                                pdo->slave_address, pdo->vendor_id, 
                                pdo->product_code, pdo->pdo_direction, 
                                pdo->pdo_offset, pdo->pdo_length, 
                                pdo->data_ptr);
                        break;
                    default:
                        error = 1;
                        pr_info("Unknown pdo->type in %s\n", __func__);
                        break;
                }
                if (error) {
                    snprintf(errbuf, sizeof(errbuf), 
                            "EtherCAT register PDO for slave %s "
                            "on Master %u failed.",
                            pdo->slave_address, master->id);
                    goto out_register_pdo;
                }

                /* Don't need the pdo any more */
                list_del(&pdo->list);
                kfree(pdo);
            }

            /* Now go through the domain's slave_block_list. This list contains
             * enhanced slave configuration */
            pr_debug("Doing enhanced slave configuration\n");
            list_for_each_entry_safe(slave_block, n4, 
                    &domain->slave_block_list, list) {
                pr_debug("Considering slave %s\n", slave_block->slave_address);
                slave_ptr = ecrt_master_get_slave(master->handle, 
                        slave_block->slave_address,
                        slave_block->vendor_id,
                        slave_block->product_code);
                if (!slave_ptr) {
                    snprintf(errbuf, sizeof(errbuf), 
                            "EtherCAT get_slave() %s on Master %u failed.",
                            slave_block->slave_address, master->id);
                    goto out_get_slave;
                }

                /* Find out whether this slave has already been registered. */
                for (slave_idx = 0; slave_idx < slave_count; slave_idx++) {
                    if (slaves[slave_idx].slave_ptr == slave_ptr) {
                        pr_debug("Slave has been registered already.\n");
                        break;
                    }
                }
                if (slave_idx < slave_count) {
                    /* Slave has already been registered, now check whether
                     * either the previous slave or this new slave id
                     * either configured or is an output.
                     * */
                    if (slaves[slave_idx].locked
                            || slave_block->output
                            || slave_block->TxPDO 
                            || slave_block->RxPDO 
                            || !list_empty(&slave_block->sdo_list)) {
                        snprintf(errbuf, sizeof(errbuf), 
                                "Cannot register EtherCAT slave %s on Master %u "
                                "twice as output or configured device.",
                                slave_block->slave_address, master->id);
                        goto out_get_slave;
                    }
                } else {
                    pr_debug("Slave has never been registered.\n");
                    /* Never seen this slave before.
                     * Put the slave into the list of known slaves */
                    slaves[slave_count].slave_ptr = slave_ptr;
                    if (slave_block->output
                            || slave_block->TxPDO 
                            || slave_block->RxPDO 
                            || !list_empty(&slave_block->sdo_list)) {
                        slaves[slave_count].locked = 1;
                    }
                    slave_idx = slave_count++;
                }

                /* First do the PDO remapping if they exist */
                if ((p = slave_block->TxPDO)) {
                    pr_debug("TxPDO has changed: ");
                    ecrt_slave_pdo_mapping_clear(slave_ptr, EC_DIR_INPUT);
                    while (*p) {
#ifdef DEBUG
                        printk("0x%X ", *p);
#endif
                        if (ecrt_slave_pdo_mapping_add(slave_ptr, 
                                    EC_DIR_INPUT, *p++)) {
                            snprintf(errbuf, sizeof(errbuf), 
                                    "Cannot add TxPDO remapping %i for EtherCAT "
                                    "slave %s on Master %u.",
                                    *(p-1), slave_block->slave_address, 
                                    master->id);
                            goto out_get_slave;
                        }
                    }
#ifdef DEBUG
                    printk("\n");
#endif
                }
                if ((p = slave_block->RxPDO)) {
                    pr_debug("RxPDO has changed: ");
                    ecrt_slave_pdo_mapping_clear(slave_ptr, EC_DIR_OUTPUT);
                    while (*p) {
#ifdef DEBUG
                        printk("0x%X ", *p);
#endif
                        if (ecrt_slave_pdo_mapping_add(slave_ptr, 
                                    EC_DIR_OUTPUT, *p++)) {
                            snprintf(errbuf, sizeof(errbuf), 
                                    "Cannot add RxPDO remapping %i for EtherCAT "
                                    "slave %s on Master %u.",
                                    *(p-1), slave_block->slave_address, 
                                    master->id);
                            goto out_get_slave;
                        }
                    }
#ifdef DEBUG
                    printk("\n");
#endif
                }

                /* Register the PDO Entries */
                list_for_each_entry_safe(pdo, n3, 
                        &slave_block->pdo_entry_list, list) {
                    /* Depending on the type of pdo (typed or range), have
                     * different calls to EtherCAT */
                    switch(pdo->type) {
                        case pdo_typed:
                            error = ecrt_domain_register_pdo(
                                    domain->handle, slave_ptr,
                                    pdo->pdo_entry_index, 
                                    pdo->pdo_entry_subindex, 
                                    pdo->data_ptr);
                            pr_debug("\t%p = ecrt_domain_register_pdo("
                                    "%p, %p, %i, %i, %p)\n", 
                                    slave_ptr, domain->handle, slave_ptr,
                                    pdo->pdo_entry_index, 
                                    pdo->pdo_entry_subindex, pdo->data_ptr);
                            break;
                        case pdo_range:
                            error = ecrt_domain_register_pdo_range(
                                    domain->handle, slave_ptr,
                                    pdo->pdo_direction, 
                                    pdo->pdo_offset, pdo->pdo_length, 
                                    pdo->data_ptr);
                            pr_debug("\t%p = ecrt_domain_register_pdo_range("
                                    "%p, %p, %i, %i, %i, %p)\n", 
                                    slave_ptr, domain->handle, slave_ptr,
                                    pdo->pdo_direction, 
                                    pdo->pdo_offset, pdo->pdo_length, 
                                    pdo->data_ptr);
                            break;
                        default:
                            error = 1;
                            pr_info("Unknown pdo->type in %s\n", __func__);
                            break;
                    }
                    if (error) {
                        snprintf(errbuf, sizeof(errbuf), 
                                "EtherCAT register PDO for slave %s "
                                "on Master %u failed.",
                                slave_block->slave_address, master->id);
                        goto out_register_pdo;
                    }
                }

                /* Go through the SDO configuration parameters for a slave */
                list_for_each_entry_safe(sdo, n5, &slave_block->sdo_list, list) {
                    switch (sdo->type) {
                        case sdo8:
                            if (ecrt_slave_conf_sdo8(slave_ptr, 
                                        sdo->sdo_index, sdo->sdo_subindex,
                                        sdo->value.u8)) {
                                goto out_conf_sdo;
                            }
                            pr_debug ("ecrt_slave_conf_sdo8(%p, %i, %i, %i)\n",
                                    slave_ptr, 
                                    sdo->sdo_index, sdo->sdo_subindex,
                                    sdo->value.u8);
                            break;
                        case sdo16:
                            if (ecrt_slave_conf_sdo16(slave_ptr, 
                                        sdo->sdo_index, sdo->sdo_subindex,
                                        sdo->value.u16)) {
                                goto out_conf_sdo;
                            }
                            pr_debug ("ecrt_slave_conf_sdo16(%p, %i, %i, %i)\n",
                                    slave_ptr, 
                                    sdo->sdo_index, sdo->sdo_subindex,
                                    sdo->value.u16);
                            break;
                        case sdo32:
                            if (ecrt_slave_conf_sdo32(slave_ptr, 
                                        sdo->sdo_index, sdo->sdo_subindex,
                                        sdo->value.u32)) {
                                goto out_conf_sdo;
                            }
                            pr_debug ("ecrt_slave_conf_sdo32(%p, %i, %i, %i)\n",
                                    slave_ptr, 
                                    sdo->sdo_index, sdo->sdo_subindex,
                                    sdo->value.u32);
                            break;
                    }
                    /* Don't need the sdo any more */
                    list_del(&sdo->list);
                    kfree(sdo);
                }

                list_del(&slave_block->list);
                kfree(slave_block);
            }

            /* Move the domain to the sample time list */
            list_del(&domain->list);
            tid = domain->tid;
            domain_idx = ecat_data->st[tid].domain_count++;
            memcpy(&ecat_data->st[tid].domain[domain_idx],
                    list_entry(&domain->list, struct ecat_domain, list),
                    sizeof(struct ecat_domain));
            /* Free the old domain and set domain to the new address */
            kfree(domain);
            domain = &ecat_data->st[tid].domain[domain_idx];

            /* Master has moved. Update pointer */
            domain->master = new_master;
        }

        /* Now that all modifications are performed on the old master,
         * the data can be copied to the new location */
        list_del(&master->list);
        memcpy(new_master, 
                list_entry(&master->list, struct ecat_master, list),
                sizeof(struct ecat_master));

        /* Free the old master */
        kfree(slaves);
        kfree(master);
    }

    /* Now that everything is set up, let the master activate itself */
    for( i = 0; i < ecat_data->nst; i++) {
        for( j = 0; j < ecat_data->st[i].master_count; j++) {
            master = &ecat_data->st[i].master[j];

            /* Setup the lock, timing  and callbacks needed for sharing 
             * the master.
             *
             * Grace_cycles is a value that allows others to lock the network
             * card if the current value of get_cycles() since the last tick
             * is less than this value. If it is more, a new tick cycle is 
             * expected soon, in which case the others should wait. Set this
             * value to 20 microseconds less than the sample time's period */
            master->grace_cycles = 
                (ecat_data->st[master->fastest_tid].period - 20)*(cpu_khz/1000);
            rt_sem_init(&master->lock,1);
            ecrt_master_callbacks(master->handle, 
                    ecs_request_cb, ecs_release_cb, master);

            if (ecrt_master_activate(master->handle)) {
                goto out_master_activate;
            }
            pr_debug ("ecrt_master_activate(%p)\n", master->handle);
        }
    }

    return NULL;

out_conf_sdo:
    snprintf(errbuf, sizeof(errbuf), 
            "EtherCAT SDO configuration for slave %s on Master %i failed.",
            slave_block->slave_address, master->id);
out_register_pdo:
out_get_slave:
    kfree(slaves);
out_alloc_slaves:
    ecat_data->st[master_tid].master_count--;
    return errbuf;

out_master_activate:
    snprintf(errbuf, sizeof(errbuf),
            "Master %i activate failed", master->id);
    return errbuf;
}

/* 
 * Change the PDO mapping of a slave.
 *
 * This function is used to change the default PDO mapping for slaves.
 * NOTE: pointer map MUST be non-volatile (i.e. NOT on the stack)
 * */
const char *
ecs_reg_slave_pdomapping( struct ecat_slave_block *slave,
        char dir, /* 'T' = Transmit; 'R' = Receive */
        uint16_t *map)
{
    switch (dir) {
        case 'R':
        case 'r':
            slave->RxPDO = map;
            break;
        case 'T':
        case 't':
            slave->TxPDO = map;
            break;
        default:
            return "Invalid direction; only 't' 'T' 'r' 'R' allowed.";
    }
    return NULL;
}

/* This function queues SDO's after a EtherCAT slave is 
 * registered */
const char *
ecs_reg_slave_sdo( struct ecat_slave_block *slave,
        int sdo_type, uint16_t sdo_index,
        uint8_t sdo_subindex, uint32_t value)
{
    struct sdo_init *sdo;

    if (!(sdo = kmalloc(sizeof(struct sdo_init), GFP_KERNEL))) {
        return no_mem_msg;
    }
    list_add_tail(&sdo->list, &slave->sdo_list);
    sdo->sdo_index = sdo_index;
    sdo->sdo_subindex = sdo_subindex;
    switch (sdo_type) {
        case 8:
            sdo->type = sdo8;
            sdo->value.u8 = (uint8_t)value;
            break;
        case 16:
            sdo->type = sdo16;
            sdo->value.u16 = (uint16_t)value;
            break;
        case 32:
            sdo->type = sdo32;
            sdo->value.u32 = (uint32_t)value;
            break;
        default:
            return "Incorrect sdo type selected.";
            break;
    }

    return NULL;
}

/* An internal function to return a master pointer.
 * If the master does not exist, one is created */
struct ecat_master *
get_master(
        unsigned int master_id,

        const char **errmsg
        )
{
    struct ecat_master *master;

    /* Is the master registered - if not, create it */
    list_for_each_entry(master, &ecat_data->master_list, list) {
        if (master->id == master_id) {
            break;
        }
    }
    if (master == (typeof(master))&ecat_data->master_list) {
        if (!(master = kzalloc(sizeof(struct ecat_master), GFP_KERNEL))) {
            *errmsg = no_mem_msg;
            return NULL;
        }
        list_add_tail(&master->list, &ecat_data->master_list);
        master->id = master_id;
        INIT_LIST_HEAD(&master->domain_list);
        master->fastest_tid--;  /* wrap it around */

        pr_debug("Requesting master %i\n", master->id);
        master->handle = ecrt_request_master(master->id);
        pr_debug("\t%p = ecrt_request_master(%i)\n",
                master->handle, master->id);
        if (!master->handle) {
            snprintf(errbuf, sizeof(errbuf), 
                    "EtherCAT Master %u initialise failed.",
                    master->id);
            *errmsg = errbuf;
            return NULL;
        }
    }

    return master;
}


/* An internal function to return a domain pointer of a master with sample
 * time tid.
 * If the domain does not exist, one is created */
struct ecat_domain *
get_domain(
        struct ecat_master *master,
        unsigned int tid,

        const char **errmsg
        )
{
    struct ecat_domain *domain;

    if (ecat_data->st[tid].period 
            < ecat_data->st[master->fastest_tid].period) {

        /* Decrease the master count for the last tid, and increase for
         * the new tid */
        master->fastest_tid = tid;
    }

    /* Is the domain registered - if not, create it */
    list_for_each_entry(domain, &master->domain_list, list) {
        if (domain->tid == tid) {
            break;
        }
    }
    if (domain == (typeof(domain))&master->domain_list) {
        if (!(domain = kzalloc(sizeof(struct ecat_domain), GFP_KERNEL))) {
            *errmsg = no_mem_msg;
            return NULL;
        }
        list_add_tail( &domain->list, &master->domain_list);
        INIT_LIST_HEAD(&domain->simple_pdo_list);
        INIT_LIST_HEAD(&domain->slave_block_list);
        domain->tid = tid;
        domain->master = master;
        ecat_data->st[tid].domain_count++;

        /* Let the EtherCAT Driver create a domain */
        domain->handle = ecrt_master_create_domain(master->handle);
        pr_debug("\t%p = ecrt_master_create_domain(%p)\n",
                domain->handle, master->handle);
        if (!domain->handle) {
            snprintf(errbuf, sizeof(errbuf), 
                    "EtherCAT Domain initialise on Master %u failed.",
                    master->id);
            *errmsg = errbuf;
            return NULL;
        }
    }

    return domain;

}

/*
 * Register a slave.
 *
 * This function is used by the RTW Model to registers a single slave.
 * This is the first function that has to be called when dealing with complex
 * slaves - those that can be parameterised, etc.
 */
struct ecat_slave_block *
ecs_reg_slave_block(
        unsigned int tid,
        unsigned int master_id,

        const char *slave_address,
        uint32_t vendor_id, /**< vendor ID */
        uint32_t product_code, /**< product code */
        unsigned int output,  /* Set to 1 if this is an output */

        const char **errmsg
        )
{
    struct ecat_master *master;
    struct ecat_domain *domain;
    struct ecat_slave_block *slave;

    if (!(master = get_master(master_id, errmsg)))
        return NULL;
    if (!(domain = get_domain(master, tid, errmsg)))
        return NULL;

    /* Now the pointers to master and domain are set up. Register the pdo
     * with the master */
    if (!(slave = kzalloc(sizeof(struct ecat_slave_block), GFP_KERNEL))) {
        goto out_slave_alloc;
    }
    list_add_tail( &slave->list, &domain->slave_block_list);
    INIT_LIST_HEAD(&slave->sdo_list);
    INIT_LIST_HEAD(&slave->pdo_entry_list);

    slave->domain = domain;
    slave->slave_address = slave_address;
    slave->vendor_id = vendor_id;
    slave->product_code = product_code;
    slave->RxPDO = NULL;
    slave->TxPDO = NULL;
    slave->output = output;
    pr_debug("%s(): Registered slave block: tid = %u, master = %u, "
            "slave_address = %s, vendor id = %u, product code = %u, output = %u\n", 
            __func__,
            tid, master_id, 
            slave->slave_address,
            slave->vendor_id,
            slave->product_code,
            slave->output
            );

    master->slave_count++;

    return slave;

out_slave_alloc:
    *errmsg = no_mem_msg;
    return NULL;
}

/*
 * Register a PDO Entry to a slave.
 *
 * This function is used by the RTW Model to register a PDO Entry 
 * for enhanced slaves.
 */
const char *
ecs_reg_slave_pdo(
        struct ecat_slave_block *slave,

        uint16_t pdo_index, /**< PDO index */
        uint8_t pdo_subindex, /**< PDO subindex */
        void **data_ptr
        )
{
    struct ecat_pdo *pdo;

    /* Now the pointers to master and domain are set up. Register the pdo
     * with the master */
    if (!(pdo = kzalloc(sizeof(struct ecat_pdo), GFP_KERNEL))) {
        return no_mem_msg;
    }
    list_add_tail( &pdo->list, &slave->pdo_entry_list);

    /* Data for EtherCAT driver */
    pdo->type = pdo_typed;
    pdo->pdo_entry_index = pdo_index;
    pdo->pdo_entry_subindex = pdo_subindex;
    pdo->data_ptr = data_ptr;
    pr_debug("%s(): Registered slave PDO for slave %s: type %u, Index %u, "
            "subindex %u, ptr %p\n",
            __func__, slave->slave_address, pdo_typed, pdo_index, 
            pdo_subindex, data_ptr);

    return NULL;
}


ec_master_t *
ecs_get_master_ptr(unsigned int master_id, const char **errmsg)
{
    struct ecat_master *master;

    if (!(master = get_master(master_id, errmsg)))
        return NULL;

    return master->handle;
}

/* 
 * Register a single PDO Entry.
 *
 * This function is used by the RTW model to register a single PDO Entry from
 * a slave.
 */
const char *
ecs_reg_pdo(
        unsigned int tid,
        unsigned int master_id,

        const char *slave_address,
        uint32_t vendor_id, /**< vendor ID */
        uint32_t product_code, /**< product code */
        uint16_t pdo_index, /**< PDO index */
        uint8_t pdo_subindex, /**< PDO subindex */

        unsigned int output,  /* Set to 1 if this is an output */

        void **data_ptr
        )
{
    struct ecat_master *master;
    struct ecat_domain *domain;
    struct ecat_pdo *pdo;
    const char *errmsg;

    if (!(master = get_master(master_id, &errmsg)) ||
            !(domain = get_domain(master, tid, &errmsg))) {
        return errmsg;
    }

    /* Now the pointers to master and domain are set up. Register the pdo
     * with the master */
    if (!(pdo = kzalloc(sizeof(struct ecat_pdo), GFP_KERNEL))) {
        return no_mem_msg;
    }
    list_add_tail( &pdo->list, &domain->simple_pdo_list);

    /* Data for EtherCAT driver */
    pdo->type = pdo_typed;
    pdo->slave_address = slave_address;
    pdo->vendor_id = vendor_id;
    pdo->product_code = product_code;
    pdo->pdo_entry_index = pdo_index;
    pdo->pdo_entry_subindex = pdo_subindex;
    pdo->data_ptr = data_ptr;
    pdo->output = output;

    master->slave_count++;

    return NULL;
}

/*
 * Register a range of memory mapped data.
 *
 * This function is called from the RTW model to register a contiguous block of data 
 * from the EtherCAT image.
 */
const char *
ecs_reg_pdo_range(
        unsigned int tid,
        unsigned int master_id,

        const char *slave_address,
        uint32_t vendor_id, /**< vendor ID */
        uint32_t product_code, /**< product code */
        ec_direction_t pdo_direction,
        uint16_t pdo_offset,
        uint16_t pdo_length,

        void **data_ptr
        )
{
    struct ecat_master *master;
    struct ecat_domain *domain;
    struct ecat_pdo *pdo;
    const char *errmsg;

    if (!(master = get_master(master_id, &errmsg)) ||
            !(domain = get_domain(master, tid, &errmsg))) {
        return errmsg;
    }

    /* Now the pointers to master and domain are set up. Register the pdo
     * with the master */
    if (!(pdo = kzalloc(sizeof(struct ecat_pdo), GFP_KERNEL))) {
        return no_mem_msg;
    }
    list_add_tail( &pdo->list, &domain->simple_pdo_list);

    /* Data for EtherCAT driver */
    pdo->type = pdo_range;
    pdo->slave_address = slave_address;
    pdo->vendor_id = vendor_id;
    pdo->product_code = product_code;
    pdo->pdo_direction = pdo_direction;
    pdo->pdo_offset = pdo_offset;
    pdo->pdo_length = pdo_length;
    pdo->data_ptr = data_ptr;
    pdo->output = 0;

    master->slave_count++;

    return NULL;
}

/* 
 * Initialise all data structures in this file.
 *
 * This function has to be called before any anything else in this file. Data
 * structures are initialised and the sample time domains are prepared.
 * Compatability with the EtherCAT interface is also checked.
 */
const char *
ecs_init( 
        unsigned int *st    /* a 0 terminated list of sample times */
        )
{
    unsigned int nst;       /* Number of sample times */
    size_t len;
    int i;

    /* Make sure that the correct header version is used */
#if ECRT_VERSION_MAGIC != ECRT_VERSION(1,3)
#error Incompatible EtherCAT header file found.
#error This source is only compatible with EtherCAT Version 1.3
#endif

    /* Make sure that the EtherCAT driver has the correct interface */
    if (ECRT_VERSION_MAGIC != ecrt_version_magic()) {
        snprintf(errbuf, sizeof(errbuf), 
                "EtherCAT driver version 0x%02x is not supported. "
                "Expecting version 0x%02x", 
                ecrt_version_magic(), ECRT_VERSION_MAGIC);
        return errbuf;
    }

    /* Count how many sample times there are. The list is zero terminated */
    for (nst = 0; st[nst]; nst++);
    pr_debug("Number of sample times: %u\n", nst);

    /* Get memory for the data */
    len = sizeof(struct ecat_data) + 
        nst*sizeof(((struct ecat_data *)0)->st[0]);
    ecat_data = kzalloc( len, GFP_KERNEL);
    if (!ecat_data) {
        return no_mem_msg;
    }

    /* Set default values for all variables */
    ecat_data->nst = nst;

    INIT_LIST_HEAD(&ecat_data->master_list);

    /* Set the period for all sample times */
    for ( i = 0; i < nst; i++ )
        ecat_data->st[i].period = st[i];

    return NULL;
}
