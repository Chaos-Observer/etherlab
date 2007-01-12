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
    struct list_head sdo_list;

    /* This structure is used with both pdo types */
    ec_pdo_reg_t data;

    /* This data is used only with pdo_range */
    ec_direction_t pdo_direction;
    uint16_t pdo_offset;
    uint16_t pdo_length;
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

    struct list_head pdo_list;  /* List where the temporary PDO objects
                                 * are stored in. This list is discarded
                                 * during ecs_start() */

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
        pr_debug("\ttid %i: ecrt_master_receive(%p)\n", 
                tid, master->handle);
        rt_sem_signal(&master->lock);
    }

    for (i = 0; i < ecat_data->st[tid].domain_count; i++) {
        domain = &ecat_data->st[tid].domain[i];
        rt_sem_wait(&domain->master->lock);
        ecrt_domain_process(domain->handle);
        pr_debug("\ttid %i: ecrt_domain_process(%p)\n", 
                tid, domain->handle);
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
        pr_debug("\ttid %i: ecrt_domain_queue(%p)\n", 
                tid, domain->handle);
        rt_sem_signal(&domain->master->lock);
    }

    for (i = 0; i < ecat_data->st[tid].master_count; i++) {
        master = &ecat_data->st[tid].master[i];
        master->last_call = get_cycles();
        rt_sem_wait(&master->lock);
        ecrt_master_run(master->handle);
        ecrt_master_send(master->handle);
        pr_debug("\ttid %i: ecrt_master_send(%p)\n", 
                tid, master->handle);
        rt_sem_signal(&master->lock);
    }
}

/* There was some problem during initialisation - free
 * allocated memory that will not be freed when model insert code
 * calls ecs_end() */
void 
abort_init(void)
{
    struct ecat_master *master, *n1;
    struct ecat_domain *domain, *n2;
    struct ecat_pdo *pdo, *n3;
    struct sdo_init *sdo, *n4;

    list_for_each_entry_safe(master, n1, &ecat_data->master_list, list) {
        list_for_each_entry_safe(domain, n2, &master->domain_list, list) {
            list_for_each_entry_safe(pdo, n3, &domain->pdo_list, list) {
                list_for_each_entry_safe(sdo, n4, &pdo->sdo_list, list) {
                    kfree(sdo);
                }
                kfree(pdo);
            }
            kfree(domain);
        }
        kfree(master);
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
                MASTER,
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

void
ecs_end(void)
{
    unsigned int i, j;
    void *st_data = NULL;

    for( i = 0; i < ecat_data->nst; i++)
        for( j = 0; j < ecat_data->st[i].master_count; j++) {
            if (!st_data)
                st_data = &ecat_data->st[i].master[j];
            pr_debug("\tecrt_release_master(%p)\n",
                    ecat_data->st[i].master[j].handle);
            ecrt_release_master(ecat_data->st[i].master[j].handle);
            pr_debug("\tecrt_release_master(%p)\n",
                    ecat_data->st[i].master[j].handle);
        }


    kfree(st_data);
    kfree(ecat_data);
}

const char *
ecs_start(void)
{
    struct ecat_master *master, *n1;
    struct ecat_domain *domain, *n2;
    struct ecat_pdo *pdo, *n3;
    ec_slave_t *slave;
    struct sdo_init *sdo, *n4;
    void *ptr;
    size_t len;
    unsigned int i, j;
    unsigned int master_tid = 0;
    unsigned int total_masters = 0, total_domains = 0;


    /* Now that the master and domain counts are known, we can
     * get a large block of contiguous memory where the master and domain
     * structures are kept. This is better for caching, and list operators
     * are now replaced with array operators */
    for( i = 0; i < ecat_data->nst; i++) {
        total_masters += ecat_data->st[i].master_count;
        total_domains += ecat_data->st[i].domain_count;
    }
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

    /* Note that here we need to use list_for_each_entry_safe() because
     * the list members are removed one by one */
    list_for_each_entry_safe(master, n1, &ecat_data->master_list, list) {
        unsigned int master_idx;
        struct ecat_master *new_master;

        pr_debug("Requesting master %i\n", master->id);
        master->handle = ecrt_request_master(master->id);
        pr_debug("\t%p = ecrt_request_master(%i)\n",
                master->handle, master->id);
        if (!master->handle) {
            snprintf(errbuf, sizeof(errbuf), 
                    "EtherCAT Master %u initialise failed.",
                    master->id);
            goto out_request_master;
        }

        /* Find out to which tid this master will be allocated. The fastest
         * tid will get it */
        master_tid = master->fastest_tid;
        master_idx = ecat_data->st[master_tid].master_count++;
        new_master = &ecat_data->st[master_tid].master[master_idx];

        ecrt_master_callbacks(master->handle, 
                ecs_request_cb, ecs_release_cb, new_master);

        /* grace_cycles is a value that allows others to lock the network
         * card if the current value of get_cycles() since the last tick
         * is less than this value. If it is more, a new tick cycle is 
         * expected soon, in which case the others should wait. Set this
         * value to 20 microseconds less than the sample time's period */
        master->grace_cycles = 
            (ecat_data->st[master_tid].period - 20)*(cpu_khz/1000);

        list_for_each_entry_safe(domain, n2, &master->domain_list, list) {
            unsigned int tid, domain_idx;

            domain->handle = ecrt_master_create_domain(master->handle);
            pr_debug("\t%p = ecrt_master_create_domain(%p)\n",
                    domain->handle, master->handle);
            if (!domain->handle) {
                snprintf(errbuf, sizeof(errbuf), 
                        "EtherCAT Domain initialise on Master %u failed.",
                        master->id);
                goto out_create_domain;
            }

            list_for_each_entry_safe(pdo, n3, &domain->pdo_list, list) {
                /* Depending on the type of pdo (typed or range), have
                 * different calls to EtherCAT */
                switch(pdo->type) {
                    case pdo_typed:
                        slave = ecrt_domain_register_pdo(
                                domain->handle, 
                                pdo->data.slave_address, pdo->data.vendor_id, 
                                pdo->data.product_code, pdo->data.pdo_index, 
                                pdo->data.pdo_subindex, pdo->data.data_ptr);
                        pr_debug("\t%p = ecrt_domain_register_pdo("
                                "%p, \"%s\", %i, %i, %i, %i, %p)\n", slave,
                                domain->handle,
                                pdo->data.slave_address, pdo->data.vendor_id, 
                                pdo->data.product_code, pdo->data.pdo_index, 
                                pdo->data.pdo_subindex, pdo->data.data_ptr);
                        break;
                    case pdo_range:
                        slave = ecrt_domain_register_pdo_range(
                                domain->handle, 
                                pdo->data.slave_address, pdo->data.vendor_id, 
                                pdo->data.product_code, pdo->pdo_direction, 
                                pdo->pdo_offset, pdo->pdo_length, 
                                pdo->data.data_ptr);
                        pr_debug("\t%p = ecrt_domain_register_pdo_range("
                                "%p, \"%s\", %i, %i, %i, %i, %i, %p)\n", slave,
                                domain->handle,
                                pdo->data.slave_address, pdo->data.vendor_id, 
                                pdo->data.product_code, pdo->pdo_direction, 
                                pdo->pdo_offset, pdo->pdo_length, 
                                pdo->data.data_ptr);
                        break;
                    default:
                        pr_info("Unknown pdo->type in %s\n", __func__);
                        slave = NULL;
                        break;
                }
                if (!slave) {
                    snprintf(errbuf, sizeof(errbuf), 
                            "EtherCAT register PDO for slave %s "
                            "on Master %u failed.",
                            pdo->data.slave_address, master->id);
                    goto out_register_pdo;
                }

                list_for_each_entry_safe(sdo, n4, &pdo->sdo_list, list) {
                    switch (sdo->type) {
                        case sdo8:
                            if (ecrt_slave_conf_sdo8(slave, 
                                        sdo->sdo_index, sdo->sdo_subindex,
                                        sdo->value.u8)) {
                                goto out_conf_sdo;
                            }
                            pr_debug ("ecrt_slave_conf_sdo8(%p, %i, %i, %i)\n",
                                    slave, 
                                        sdo->sdo_index, sdo->sdo_subindex,
                                        sdo->value.u8);
                            break;
                        case sdo16:
                            if (ecrt_slave_conf_sdo16(slave, 
                                        sdo->sdo_index, sdo->sdo_subindex,
                                        sdo->value.u16)) {
                                goto out_conf_sdo;
                            }
                            pr_debug ("ecrt_slave_conf_sdo16(%p, %i, %i, %i)\n",
                                    slave, 
                                        sdo->sdo_index, sdo->sdo_subindex,
                                        sdo->value.u16);
                            break;
                        case sdo32:
                            if (ecrt_slave_conf_sdo32(slave, 
                                        sdo->sdo_index, sdo->sdo_subindex,
                                        sdo->value.u32)) {
                                goto out_conf_sdo;
                            }
                            pr_debug ("ecrt_slave_conf_sdo32(%p, %i, %i, %i)\n",
                                    slave, 
                                        sdo->sdo_index, sdo->sdo_subindex,
                                        sdo->value.u32);
                            break;
                    }
                    /* Don't need the sdo any more */
                    list_del(&sdo->list);
                    kfree(sdo);
                }
                /* Don't need the pdo any more */
                list_del(&pdo->list);
                kfree(pdo);
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
        kfree(master);

    }

    /* Now that everything is set up, let the master activate itself */
    for( i = 0; i < ecat_data->nst; i++)
        for( j = 0; j < ecat_data->st[i].master_count; j++) {
            master = &ecat_data->st[i].master[j];
            if (ecrt_master_activate(master->handle)) {
                goto out_master_activate;
            }
            pr_debug ("ecrt_master_activate(%p)\n", master->handle);
            rt_sem_init(&master->lock,1);
        }

    return NULL;

out_conf_sdo:
    snprintf(errbuf, sizeof(errbuf), 
            "EtherCAT SDO for slave %s failed.",
            pdo->data.slave_address);
out_register_pdo:
out_create_domain:
    ecat_data->st[master_tid].master_count--;
    ecrt_release_master(master->handle);
out_request_master:
    abort_init();
    return errbuf;

out_master_activate:
    snprintf(errbuf, sizeof(errbuf),
            "Master %i activate failed", master->id);
    return errbuf;
}

/* This function queues SDO's after a registered EtherCAT Terminal is 
 * registered */
const char *
ecs_reg_sdo( struct ecat_pdo *pdo,
        int sdo_type, uint16_t sdo_index,
        uint8_t sdo_subindex, uint32_t value)
{
    const char *errmsg;
    struct sdo_init *sdo;

    if (!(sdo = kmalloc(sizeof(struct sdo_init), GFP_KERNEL))) {
        errmsg = no_mem_msg;
        goto out;
    }
    list_add_tail(&sdo->list, &pdo->sdo_list);
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
            errmsg = "Incorrect sdo type selected.";
            goto out;
            break;
    }

    return NULL;

out:
    abort_init();
    return errmsg;
}

struct ecat_pdo *
_new_pdo(
        unsigned int tid,
        unsigned int master_id,

        const char **errmsg
        )
{
    struct ecat_master *master;
    struct ecat_domain *domain;
    struct ecat_pdo *pdo;
    unsigned int found;

    /* Is the master registered - if not, create it */
    found = 0;
    list_for_each_entry(master, &ecat_data->master_list, list) {
        if (master->id == master_id) {
            found = 1;
            break;
        }
    }
    if (!found) {
        if (!(master = kzalloc(sizeof(struct ecat_master), GFP_KERNEL))) {
            goto out_new_pdo;
        }
        list_add_tail(&master->list, &ecat_data->master_list);
        master->id = master_id;
        INIT_LIST_HEAD(&master->domain_list);
        master->fastest_tid = tid;
        ecat_data->st[tid].master_count++;
    }

    if (ecat_data->st[tid].period 
            < ecat_data->st[master->fastest_tid].period) {

        /* Decrease the master count for the last tid, and increase for
         * the new tid */
        ecat_data->st[master->fastest_tid].master_count--;
        ecat_data->st[tid].master_count++;
        master->fastest_tid = tid;
    }

    /* Is the domain registered - if not, create it */
    found = 0;
    list_for_each_entry(domain, &master->domain_list, list) {
        if (domain->tid == tid) {
            found = 1;
            break;
        }
    }
    if (!found) {
        if (!(domain = kzalloc(sizeof(struct ecat_domain), GFP_KERNEL))) {
            goto out_new_pdo;
        }
        list_add_tail( &domain->list, &master->domain_list);
        INIT_LIST_HEAD(&domain->pdo_list);
        domain->tid = tid;
        domain->master = master;
        ecat_data->st[tid].domain_count++;
    }

    /* Now the pointers to master and domain are set up. Register the pdo
     * with the master */
    if (!(pdo = kzalloc(sizeof(struct ecat_pdo), GFP_KERNEL))) {
        goto out_new_pdo;
    }
    list_add_tail( &pdo->list, &domain->pdo_list);
    INIT_LIST_HEAD(&pdo->sdo_list);

    return pdo;

out_new_pdo:
    *errmsg = no_mem_msg;
    return NULL;
}

struct ecat_pdo *
ecs_reg_pdo(
        unsigned int tid,
        unsigned int master_id,

        const char *slave_address,
        uint32_t vendor_id, /**< vendor ID */
        uint32_t product_code, /**< product code */
        uint16_t pdo_index, /**< PDO index */
        uint8_t pdo_subindex, /**< PDO subindex */
        void **data_ptr,

        const char **errmsg
        )
{
    struct ecat_pdo *pdo;

    if (!(pdo = _new_pdo( tid, master_id, errmsg))) {
        goto out_reg_pdo;
    }


    /* Data for EtherCAT driver */
    pdo->type = pdo_typed;
    pdo->data.slave_address = slave_address;
    pdo->data.vendor_id = vendor_id;
    pdo->data.product_code = product_code;
    pdo->data.pdo_index = pdo_index;
    pdo->data.pdo_subindex = pdo_subindex;
    pdo->data.data_ptr = data_ptr;

    return pdo;

out_reg_pdo:
    abort_init();
    return NULL;
}

struct ecat_pdo *
ecs_reg_pdo_range(
        unsigned int tid,
        unsigned int master_id,

        const char *slave_address,
        uint32_t vendor_id, /**< vendor ID */
        uint32_t product_code, /**< product code */
        ec_direction_t pdo_direction,
        uint16_t pdo_offset,
        uint16_t pdo_length,
        void **data_ptr,

        const char **errmsg
        )
{
    struct ecat_pdo *pdo;

    if (!(pdo = _new_pdo( tid, master_id, errmsg))) {
        goto out_reg_pdo_range;
    }


    /* Data for EtherCAT driver */
    pdo->type = pdo_range;
    pdo->data.slave_address = slave_address;
    pdo->data.vendor_id = vendor_id;
    pdo->data.product_code = product_code;
    pdo->pdo_direction = pdo_direction;
    pdo->pdo_offset = pdo_offset;
    pdo->pdo_length = pdo_length;
    pdo->data.data_ptr = data_ptr;

    return pdo;

out_reg_pdo_range:
    abort_init();
    return NULL;
}

const char *
ecs_init( 
        unsigned int *st    /* a 0 terminated list of sample times */
        )
{
    unsigned int nst;       /* Number of sample times */
    size_t len;
    int i;

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
