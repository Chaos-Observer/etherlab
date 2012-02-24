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
//#error "Don't have config.h"
#endif

#include <stdio.h>
#include <pthread.h>
#include "ecrt_support.h"
#include "list.h"

/* The following message gets repeated quite frequently. */
char *no_mem_msg = "Could not allocate memory";
char errbuf[256];

#define my_kcalloc(nmemb,size,s) calloc((nmemb), (size))
#define my_kfree(ptr,s) free(ptr)
#define pr_debug(fmt, args...) printf(fmt, args)

/* Structure to temporarily store PDO objects prior to registration */
struct ecat_slave {
    struct list_head list;      /* Linked list of pdo's */

    /* Data required for ecrt_master_slave_config() */
    uint16_t alias;
    uint16_t position;
    uint32_t vendor_id; /**< vendor ID */
    uint32_t product_code; /**< product code */

    const ec_sync_info_t *sync_info;

    /* Distributed clock configuration */
    uint16_t assign_activate;   /**< If != 0, use distributed clocks */
    uint32_t dc_time_sync[4];   /**< Values for CycleTimeSync0, ShiftTimeSync0,
                                  * CycleTimeSync1, ShiftTimeSync1 */

    unsigned int sdo_config_count;
    const struct sdo_config *sdo_config;

    unsigned int soe_config_count;
    const struct soe_config *soe_config;

    unsigned int pdo_map_count;
    struct mapped_pdo {
        struct ecat_domain *domain;
        const struct pdo_map *mapping;
        size_t base_offset;
    } mapped_pdo[];
};

struct endian_convert_t {
    unsigned int pdo_datatype_size;
    void *data_ptr;
};


/* Every domain has one of these structures. There can exist exactly one
 * domain for a sample time on an EtherCAT Master. */
struct ecat_domain {
    struct list_head list;      /* Linked list of domains. This list
                                 * is only used prior to ecs_start(), where
                                 * it is reworked into an array for faster
                                 * access */

    unsigned int id;            /* RTW id number assigned to this domain */

    ec_domain_t *handle;        /* used when calling EtherCAT functions */
    ec_domain_state_t *state;   /* pointer to domain's state */

    struct ecat_master *master; /* The master this domain is registered
                                 * in. Every domain has exactly 1 master */

    unsigned int tid;           /* Id of the corresponding RealTime Workshop
                                 * task */

    unsigned int
        endian_convert_count;   /* Number of elements to convert when
                                 * doing endian conversion */
    struct endian_convert_t       
        *endian_convert_list;   /* List for data copy instructions. List is
                                 * terminated with list->from = NULL */

    size_t io_data_len;         /* Lendth of io_data image */
    void *io_data;              /* IO data is located here */
};

/* For every EtherCAT Master, one such structure exists */
struct ecat_master {
    struct list_head list;      /* Linked list of masters. This list
                                 * is only used prior to ecs_start(), where
                                 * it is reworked into an array for faster
                                 * access */

    unsigned int fastest_tid;   /* Fastest RTW task id that uses this 
                                 * master */

    struct task_stats *task_stats; /* Index into global task_stats */

    unsigned int id;            /* Id of the EtherCAT master */
    ec_master_t *handle;        /* Handle retured by EtherCAT code */
    ec_master_state_t *state;   /* Pointer for master's state */

    unsigned int refclk_trigger_init; /* Decimation for reference clock
                                         == 0 => do not use dc */
    unsigned int refclk_trigger; /* When == 1, trigger a time syncronisation */

    pthread_mutex_t lock;        /* Lock the ecat master */

    struct list_head slave_list; /* List of all slaves. Only active during
                                  * initialisation */

    /* Temporary storage for domains registered to this master. Every
     * sample time has a read and a write list
     */
    struct {
        struct list_head input_domain_list;
        struct list_head output_domain_list;
    } st_domain[];
};

/* Data used by the EtherCAT support layer */
static struct ecat_data {
    unsigned int nst;           /* Number of unique RTW sample times */
    unsigned int single_tasking;

    /* This list is used to store all masters during the registration
     * process. Thereafter this list is reworked in ecs_start(), moving 
     * the masters and domains into the st[] struct below */
    struct list_head master_list;

    struct st_data {
        unsigned int master_count;
        unsigned int input_domain_count;
        unsigned int output_domain_count;

        /* Array of masters and domains in this sample time. The data area
         * are slices out of st_data */
        struct ecat_master *master;
        struct ecat_domain *input_domain;
        struct ecat_domain *output_domain;

        /* The tick period in nanoseconds for this sample time */
        unsigned int period;
    } st[];
} *ecat_data;


/////////////////////////////////////////////////

/* Do input processing for a RTW task. It does the following:
 *      - calls ecrt_master_receive() for every master whose fastest 
 *        domain is in this task. 
 *      - calls ecrt_domain_process() for every domain in this task
 */
void
ecs_receive(int tid)
{
    struct ecat_master *master = ecat_data->st[tid].master;
    struct ecat_master * const master_end = 
        master + ecat_data->st[tid].master_count;
    struct ecat_domain *domain = ecat_data->st[tid].input_domain;
    struct ecat_domain * const domain_end = 
        domain + ecat_data->st[tid].input_domain_count;
    struct endian_convert_t *conversion_list;
        
    for ( ; master != master_end; master++) {
        pthread_mutex_lock (&master->lock);
        ecrt_master_receive(master->handle);
        ecrt_master_state(master->handle, master->state);
        pthread_mutex_unlock (&master->lock);
    }

    for ( ; domain != domain_end; domain++) {
        pthread_mutex_lock (&domain->master->lock);
        ecrt_domain_process(domain->handle);
        ecrt_domain_state(domain->handle, domain->state);
        ecrt_domain_queue(domain->handle);
        pthread_mutex_unlock (&domain->master->lock);
        for (conversion_list = domain->endian_convert_list;
                conversion_list->data_ptr;
                conversion_list++) {
            switch (conversion_list->pdo_datatype_size) {
                case 8:
                    *(uint64_t*)conversion_list->data_ptr =
                        le64toh(*(uint64_t*)conversion_list->data_ptr);
                    break;
                case 4:
                    *(uint32_t*)conversion_list->data_ptr =
                        le32toh(*(uint32_t*)conversion_list->data_ptr);
                    break;
                case 2:
                    *(uint16_t*)conversion_list->data_ptr =
                        le32toh(*(uint16_t*)conversion_list->data_ptr);
                    break;
            }
        }
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
    struct ecat_master *master = ecat_data->st[tid].master;
    struct ecat_master * const master_end = 
        master + ecat_data->st[tid].master_count;
    struct ecat_domain *domain = ecat_data->st[tid].output_domain;
    struct ecat_domain * const domain_end = 
        domain + ecat_data->st[tid].output_domain_count;
    struct endian_convert_t *conversion_list;

    for ( ; domain != domain_end; domain++) {
        for (conversion_list = domain->endian_convert_list; 
                conversion_list->data_ptr;
                conversion_list++) {
            switch (conversion_list->pdo_datatype_size) {
                case 8:
                    *(uint64_t*)conversion_list->data_ptr =
                        htole64(*(uint64_t*)conversion_list->data_ptr);
                    break;
                case 4:
                    *(uint32_t*)conversion_list->data_ptr =
                        htole32(*(uint32_t*)conversion_list->data_ptr);
                    break;
                case 2:
                    *(uint16_t*)conversion_list->data_ptr =
                        htole16(*(uint16_t*)conversion_list->data_ptr);
                    break;
            }
        }
        pthread_mutex_lock (&domain->master->lock);
        ecrt_domain_process(domain->handle);
        ecrt_domain_state(domain->handle, domain->state);

        ecrt_domain_queue(domain->handle);
        pthread_mutex_unlock (&domain->master->lock);
    }

    for ( ; master != master_end; master++) {
//        ecrt_master_application_time(master->handle,
//                task_stats->monotonic_time);

        pthread_mutex_lock (&master->lock);
        if (master->refclk_trigger_init && !--master->refclk_trigger) {
            ecrt_master_sync_reference_clock(master->handle);
            master->refclk_trigger = master->refclk_trigger_init;
        }

        ecrt_master_sync_slave_clocks(master->handle);
        ecrt_master_send(master->handle);
        pthread_mutex_unlock (&master->lock);
    }
}

// /* This callback is passed to EtherCAT during master initialisation. 
//  *
//  * Normally it is not allowable for anyone else to use the master other
//  * than the owner himself to guarantee hard real-time response. In rare
//  * cases, however, it may be necessary for EtherCAT to request private usage
//  * of the master, for example to do EoE.
//  * Using this function, EtherCAT has the chance to gain temporary exclusive 
//  * use of the master.
//  */
// static void
// ecs_send_cb(void *p)
// {
// #define MASTER ((struct ecat_master *)p)
//     pthread_mutex_lock (&MASTER->lock);
//     ecrt_master_send_ext(MASTER->handle);
//     pthread_mutex_unlock (&MASTER->lock);
// #undef MASTER
// }
// 
// /* This callback is passed to EtherCAT during master initialisation. 
//  */
// static void
// ecs_receive_cb(void *p)
// {
// #define MASTER ((struct ecat_master *)p)
//     pthread_mutex_lock (&MASTER->lock);
//     ecrt_master_receive(MASTER->handle);
//     pthread_mutex_unlock (&MASTER->lock);
// #undef MASTER
// }

/* Initialise all slaves listed in master->slave_list. */
static const char*
init_slaves( struct ecat_master *master)
{
    struct ecat_slave *slave;
    ec_slave_config_t *slave_config;
    const struct sdo_config *sdo;
    const struct soe_config *soe;
    const char* failed_method;
    struct mapped_pdo *mapped_pdo;

    list_for_each_entry(slave, &master->slave_list, list) {

        pr_debug("Considering slave %p %u:%u on master %u(%p)\n", 
                slave,
                slave->alias, slave->position,
                master->id, master->handle);
        slave_config = ecrt_master_slave_config( master->handle, slave->alias,
                slave->position, slave->vendor_id, slave->product_code);
        if (!slave_config) {
            failed_method = "ecrt_master_slave_config";
            goto out_slave_failed;
        }

        /* Inform EtherCAT of how the slave configuration is expected */
        if (ecrt_slave_config_pdos(slave_config, 
                    EC_END, slave->sync_info)) {
            failed_method = "ecrt_slave_config_pdos";
            goto out_slave_failed;
        }

        /* Send SDO configuration to the slave */
        for (sdo = slave->sdo_config; 
                sdo != &slave->sdo_config[slave->sdo_config_count]; sdo++) {
            if (sdo->byte_array) {
                if (ecrt_slave_config_complete_sdo(slave_config, 
                            sdo->sdo_index, sdo->byte_array,
                            sdo->sdo_attribute)) {
                    failed_method = "ecrt_slave_config_complete_sdo";
                    goto out_slave_failed;
                }
            }
            else {
                switch (sdo->datatype) {
                    case pd_uint8_T:
                        if (ecrt_slave_config_sdo8(slave_config, 
                                    sdo->sdo_index, sdo->sdo_attribute,
                                    (uint8_t)sdo->value)) {
                            failed_method = "ecrt_slave_config_sdo8";
                            goto out_slave_failed;
                        }
                        break;
                    case pd_uint16_T:
                        if (ecrt_slave_config_sdo16(slave_config, 
                                    sdo->sdo_index, sdo->sdo_attribute,
                                    (uint16_t)sdo->value)) {
                            failed_method = "ecrt_slave_config_sdo16";
                            goto out_slave_failed;
                        }
                        break;
                    case pd_uint32_T:
                        if (ecrt_slave_config_sdo32(slave_config, 
                                    sdo->sdo_index, sdo->sdo_attribute,
                                    sdo->value)) {
                            failed_method = "ecrt_slave_config_sdo32";
                            goto out_slave_failed;
                        }
                        break;
                    default:
                        failed_method = "ecrt_slave_config_sdo_unknown";
                        goto out_slave_failed;
                        break;
                }
            }
        }

        /* Send SoE configuration to the slave */
        for (soe = slave->soe_config; 
                soe != &slave->soe_config[slave->soe_config_count]; soe++) {
            if (ecrt_slave_config_idn(slave_config,
                        0, /* drive_no */
                        soe->idn,
                        EC_AL_STATE_PREOP,
                        soe->data,
                        soe->data_len)) {
                failed_method = "ecrt_slave_config_idn";
                goto out_slave_failed;
            }
        }

        /* Configure distributed clocks for slave if assign_activate is set */
        ecrt_slave_config_dc(slave_config, slave->assign_activate,
                slave->dc_time_sync[0], slave->dc_time_sync[1],
                slave->dc_time_sync[2], slave->dc_time_sync[3]);

        /* Now map the required PDO's into the process image. The offset
         * inside the image is returned by the function call */
        for (mapped_pdo = slave->mapped_pdo;
                mapped_pdo != &slave->mapped_pdo[slave->pdo_map_count]; 
                mapped_pdo++) {
            const struct pdo_map *mapping = mapped_pdo->mapping;
            int offset = ecrt_slave_config_reg_pdo_entry(
                        slave_config,
                        mapping->pdo_entry_index,
                        mapping->pdo_entry_subindex,
                        mapped_pdo->domain->handle,
                        mapping->bitoffset);
            if (offset < 0) {
                failed_method = "ecrt_slave_config_reg_pdo_entry";
                goto out_slave_failed;
            }
            mapped_pdo->base_offset = offset;
            if (mapping->bitoffset) 
                pr_debug("Bit Offset %p %u: %u\n", 
                        mapping->bitoffset, slave->position, 
                        *mapping->bitoffset);
        }
    }

    return 0;

out_slave_failed:
    snprintf(errbuf, sizeof(errbuf), 
            "EtherCAT %s() failed "
            "for slave #%u:%u on master %u", failed_method,
            slave->alias, slave->position, master->id);
    return errbuf;

}


/* An internal function to return a master pointer.
 * If the master does not exist, one is created */
struct ecat_master *
get_master(
        unsigned int master_id,
        unsigned int tid,

        const char **errmsg
        )
{
    struct ecat_master *master;
    unsigned int i;
    ptrdiff_t len;

    /* Is the master registered? */
    list_for_each_entry(master, &ecat_data->master_list, list) {
        if (master->id == master_id) {
            /* An EtherCAT master device is assigned to the fastest task
             * in the system. All other tasks just queue their
             * packets, waiting to be sent by the fastest task.
             *
             * When these code lines are reached, a master structure has 
             * already been created. Now just check whether the new new
             * tast period isn't maybe faster than the one the master is
             * currently assigned to, changing the management variables
             * if this is the case */
            if (ecat_data->st[master->fastest_tid].period > 
                    ecat_data->st[tid].period) {

                /* Tell the previous task that it has one less master */
                ecat_data->st[master->fastest_tid].master_count--;

                /* Tell the new that task that it has a new master */
                ecat_data->st[tid].master_count++;
                master->fastest_tid = tid;

                /* Where the master should get its statistics from */
//                master->task_stats =
//                    &task_stats[ecat_data->single_tasking ? 0 : tid];
            }
            return master;
        }
    }

    /* Master with id master_id does not exist - create it */

    /* Although master is not initialised here, it is not dereferenced,
     * only address operators are being used. */
    len = (void*)&master->st_domain[ecat_data->nst] - (void*)master;
    if (!(master = my_kcalloc(1, len, "master"))) {
        *errmsg = no_mem_msg;
        return NULL;
    }
    list_add_tail(&master->list, &ecat_data->master_list);

    /* Initialise variables */
    master->id = master_id;
    for (i = 0; i < ecat_data->nst; i++) {
        INIT_LIST_HEAD(&master->st_domain[i].input_domain_list);
        INIT_LIST_HEAD(&master->st_domain[i].output_domain_list);
    }
    INIT_LIST_HEAD(&master->slave_list);
    master->fastest_tid = tid;
//    master->task_stats =
//        &task_stats[ecat_data->single_tasking ? 0 : tid];
    ecat_data->st[tid].master_count++;

    master->state = my_kcalloc(1, sizeof(*master->state), "master state");
    if (!master->state) {
        *errmsg = no_mem_msg;
        return NULL;
    }

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

    return master;
}


/* An internal function to return a domain pointer of a master with sample
 * time tid.
 * If the domain does not exist, one is created */
struct ecat_domain *
get_domain(
        struct ecat_master *master,
        unsigned int domain_id,
        ec_direction_t direction,
        unsigned int tid,

        const char **errmsg
        )
{
    struct ecat_domain *domain;
    struct list_head *domain_list;
    unsigned int *domain_counter;

    /* Is the domain registered - if not, create it
     * The master maintains a separate list for the read and write 
     * directions for every sample time */
    switch (direction) {
        case EC_DIR_OUTPUT:
            domain_list = &master->st_domain[tid].output_domain_list;
            domain_counter = &ecat_data->st[tid].output_domain_count;
            break;
        case EC_DIR_INPUT:
            domain_list = &master->st_domain[tid].input_domain_list;
            domain_counter = &ecat_data->st[tid].input_domain_count;
            break;
        default:
            snprintf(errbuf, sizeof(errbuf), 
                    "Unknown ec_direction_t used for domain %u, tid %i.",
                    domain_id, tid);
            *errmsg = errbuf;
            return NULL;
    };
    list_for_each_entry(domain, domain_list, list) {
        if (domain->id == domain_id) {
            /* Yes, the domain has been registered once before */
            pr_debug("%s domain %u:%u for tid %u exists\n",
                    direction == EC_DIR_OUTPUT ? "Output" : "Input",
                    master->id, domain_id, tid);
            return domain;
        }
    }

    /* This is a new domain. Allocate memory */
    if (!(domain = my_kcalloc(1, sizeof(struct ecat_domain), "domain"))) {
        *errmsg = no_mem_msg;
        return NULL;
    }
    list_add_tail( &domain->list, domain_list);

    /* Initialise variables */
    domain->tid = tid;
    domain->master = master;
    domain->id = domain_id;
    (*domain_counter)++;

    /* Memory for the domain's state */
    domain->state = my_kcalloc(1, sizeof(*domain->state), "domain state");
    if (!domain->state) {
        *errmsg = no_mem_msg;
        return NULL;
    }

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

    pr_debug("New %s domain %u:%u for tid %u created\n",
            direction == EC_DIR_OUTPUT ? "output" : "input",
            master->id, domain_id, tid);

    return domain;

}

/*
 * Register a slave.
 *
 * This function is used by the RTW Model to registers a single slave.
 * This is the first function that has to be called when dealing with complex
 * slaves - those that can be parameterised, etc.
 */
const char*
ecs_reg_slave(
        unsigned int tid,
        unsigned int master_id,
        unsigned int domain_id,

        uint16_t slave_alias,
        uint16_t slave_position,
        uint32_t vendor_id, /**< vendor ID */
        uint32_t product_code, /**< product code */

        unsigned int sdo_config_count,
        const struct sdo_config *sdo_config,

        unsigned int soe_config_count,
        const struct soe_config *soe_config,

        const ec_sync_info_t *sync_info,

        const uint32_t *dc_opmode_config,

        unsigned int pdo_count,
        const struct pdo_map *pdo_map
        )
{
    const char *errmsg;
    struct ecat_master *master;
    struct ecat_domain *input_domain = NULL;
    struct ecat_domain *output_domain = NULL;
    struct ecat_slave  *slave = NULL;
    size_t len;
    int i;
    pr_debug( "ecs_reg_slave( tid = %u, master_id = %u, domain_id = %u, "
        "slave_alias = %hu, slave_position = %hu, vendor_id = 0x%x, "
        "product_code = 0x%x, "
        "sdo_config_count = %u, *sdo_config = %p, "
        "soe_config_count = %u, *soe_config = %p, "
        "*sync_info = %p, pdo_count = %u, *pdo_map = %p )\n",
        tid, master_id, domain_id,
        slave_alias, slave_position, vendor_id, product_code,
        sdo_config_count, sdo_config,
        soe_config_count, soe_config,
        sync_info, pdo_count, pdo_map
        );

    /* Get a pointer to the master, creating a new one if it does not
     * exist already */
    if (!(master = get_master(master_id, tid, &errmsg)))
        return errmsg;

    /* Get memory for the slave structure */
    len = (void*)&slave->mapped_pdo[pdo_count] - (void*)slave;
    slave = my_kcalloc(1, len, "slave");
    if (!slave) {
        goto out_slave_alloc;
    }
    list_add_tail(&slave->list, &master->slave_list);

    /* Slave identification */
    slave->alias = slave_alias;
    slave->position = slave_position;
    slave->vendor_id = vendor_id;
    slave->product_code = product_code;

    /* Copy the SDO's locally */
    slave->sdo_config = sdo_config;
    slave->sdo_config_count = sdo_config_count;

    /* Copy the SoE's locally */
    slave->soe_config = soe_config;
    slave->soe_config_count = soe_config_count;

    /* Copy the PDO structures locally */
    slave->sync_info = sync_info;

    if (dc_opmode_config) {
        slave->assign_activate = *dc_opmode_config++;
        slave->dc_time_sync[0] = *dc_opmode_config++;
        slave->dc_time_sync[1] = *dc_opmode_config++;
        slave->dc_time_sync[2] = *dc_opmode_config++;
        slave->dc_time_sync[3] = *dc_opmode_config++;
    }

    /* The pdo lists the PDO's that should be mapped
     * into the process image and also contains pointers where the data
     * should be copied, as well as the data type. 
     * Here we go through the list assigning the mapped PDO to the correct
     * domain depending on whether it is an input or output */
    slave->pdo_map_count = pdo_count;
    for (i = 0; i < pdo_count; i++) {
        struct ecat_domain *domain;
        unsigned int endian_convert_count;

        pr_debug("Doing pdo_mapping %i\n", i);
        slave->mapped_pdo[i].mapping = &pdo_map[i];
        switch (pdo_map[i].dir) {
            /* PDO Input as seen by the slave, i.e. block inputs. The
             * corresponding slaves in the field are outputs, 
             * eg. analog and digital outputs */
            case EC_DIR_INPUT: 
                if (!input_domain) {
                    input_domain = get_domain(master, domain_id, 
                            EC_DIR_INPUT, tid, &errmsg);
                    if (!input_domain) {
                        return errmsg;
                    }
                }
                domain = input_domain;
                pr_debug("Selecting Input Domain for PDO Entry #x%04X\n",
                        pdo_map[i].pdo_entry_index);
                break;

            /* PDO Output as seen by the slave, i.e. block outputs. The
             * corresponding slaves in the field are inputs, 
             * eg. analog and digital inputs */
            case EC_DIR_OUTPUT: 
                if (!output_domain) {
                    output_domain = get_domain(master, domain_id, 
                            EC_DIR_OUTPUT, tid, &errmsg);
                    if (!output_domain) {
                        return errmsg;
                    }
                }
                domain = output_domain;
                pr_debug("Selecting Output Domain for PDO Entry #x%04X\n",
                        pdo_map[i].pdo_entry_index);
                break;

            default:
                return "Unknown PDO Direction encountered.";
        }
        slave->mapped_pdo[i].domain = domain;

        endian_convert_count = pdo_map[i].pdo_datatype_size > 1;
        endian_convert_count +=
            (pdo_map[i].pdo_datatype_size == 7
             || pdo_map[i].pdo_datatype_size == 6);
        domain->endian_convert_count +=
            endian_convert_count * pdo_map[i].vector_len;
    }

    return NULL;

out_slave_alloc:
    return no_mem_msg;
}

const char *
ecs_setup_master(unsigned int master_id, unsigned int refclk_sync_dec,
        void **master_p)
{
    const char *errmsg;
    struct ecat_master *master;

    /* Get the master structure, making sure not to change the task it 
     * is currently assigned to */
    if (!(master = get_master(master_id, ecat_data->nst - 1, &errmsg)))
        return errmsg;

    if (master_p)
        *master_p = master->handle;

    master->refclk_trigger_init = refclk_sync_dec;
    master->refclk_trigger = 1;

    return NULL;
}

ec_domain_t *
ecs_get_domain_ptr(unsigned int master_id, unsigned int domain_id, 
        ec_direction_t dir, unsigned int tid, const char **errmsg)
{
    struct ecat_master *master;
    struct ecat_domain *domain;

    /* Get the master structure, making sure not to change the task it 
     * is currently assigned to */
    if (!(master = get_master(master_id, ecat_data->nst - 1, errmsg)))
        return NULL;

    if (!(domain = get_domain(master, domain_id, dir, tid, errmsg)))
        return NULL;

    return domain->handle;
}

void
cleanup_mem(void)
{
    struct ecat_master *master, *m;
    struct ecat_domain *domain, *d;
    struct ecat_slave  *slave,  *s;
    int i;

    // Free all the memory allocated for master and domain structures
    // Note that kfree(NULL) is allowed.
    list_for_each_entry_safe(master, m, &ecat_data->master_list, list) {
        for (i = 0; i < ecat_data->nst; i++) {

            list_for_each_entry_safe(domain, d, 
                    &master->st_domain[i].input_domain_list, list) {
                my_kfree(domain, "domain");
            }

            list_for_each_entry_safe(domain, d, 
                    &master->st_domain[i].output_domain_list, list) {
                my_kfree(domain, "domain");
            }
        }

        list_for_each_entry_safe(slave, s, &master->slave_list, list) {
            my_kfree(slave, "slave");
        }

        // master->handle is NULL if the handle has already been transferred
        // to ecat_data->st[].master. It will be released there.
        if (master->handle)
            ecrt_release_master(master->handle);

        my_kfree(master, "master");
    }

    // Close the list so that it is not cleaned up again
    INIT_LIST_HEAD(&ecat_data->master_list);
}

const char *
init_domains(struct ecat_master *master, struct ecat_master *new_master) 
{
    unsigned int i;
    struct ecat_domain *domain;

    /* Loop through all of the masters sample time domains domains */
    for (i = 0; i < ecat_data->nst; i++) {
        unsigned int tid, domain_idx;

        /* Initialise all slaves in the input domain */
        list_for_each_entry(domain,
                &master->st_domain[i].input_domain_list, list) {
            /* The list has to be zero terminated */
            domain->endian_convert_list = 
                my_kcalloc(domain->endian_convert_count + 1,
                        sizeof(*domain->endian_convert_list),
                        "input copy list");
            if (!domain->endian_convert_list)
                return no_mem_msg;

            domain->io_data = ecrt_domain_data(domain->handle);
//            /* Allocate local IO memory */
//            domain->io_data_len = ecrt_domain_size(domain->handle);
//            domain->io_data = 
//                my_kcalloc(1, domain->io_data_len, "input io data");
//            if (!domain->io_data)
//                return no_mem_msg;
//            ecrt_domain_external_memory(domain->handle, domain->io_data);

            /* Move the domain to the sample time array */
            tid = domain->tid;
            domain_idx = ecat_data->st[tid].input_domain_count++;
            domain->master = new_master;
            ecat_data->st[tid].input_domain[domain_idx] = *domain;
        }

        /* Initialise all slaves in the output domain */
        list_for_each_entry(domain,
                &master->st_domain[i].output_domain_list, list) {

            /* The list has to be zero terminated */
            domain->endian_convert_list = 
                my_kcalloc( domain->endian_convert_count + 1,
                        sizeof(*domain->endian_convert_list),
                        "output copy list");
            if (!domain->endian_convert_list)
                return no_mem_msg;

//            /* Allocate local IO memory */
//            domain->io_data_len = ecrt_domain_size(domain->handle);
//            domain->io_data = 
//                my_kcalloc(1, domain->io_data_len, "output io data");
//            if (!domain->io_data)
//                return no_mem_msg;
//            ecrt_domain_external_memory(domain->handle, domain->io_data);

            /* Move the domain to the sample time array */
            tid = domain->tid;
            domain_idx = ecat_data->st[tid].output_domain_count++;
            domain->master = new_master;
            ecat_data->st[tid].output_domain[domain_idx] = *domain;
        }
    }

    return NULL;
}

const char *
init_slave_iodata(struct ecat_slave *slave)
{
    struct mapped_pdo *mapped_pdo;
    unsigned int i;

    for (mapped_pdo = slave->mapped_pdo;
            mapped_pdo != &slave->mapped_pdo[slave->pdo_map_count];
            mapped_pdo++) {
        const struct pdo_map *pdo = mapped_pdo->mapping;
        char *data_ptr = 
            mapped_pdo->domain->io_data + mapped_pdo->base_offset;
        unsigned int bitposition = pdo->bitoffset ? *pdo->bitoffset : 0;
        struct endian_convert_t *endian_convert;

        for (i = 0; i < pdo->vector_len; i++) {
            pdo->address[i] = data_ptr;
            pr_debug("Assigning io Address %u %p\n", i, pdo->address[i]);

            if (pdo->bitoffset) {
                pdo->bitoffset[i] = bitposition;

                bitposition += pdo->bitlen;
                data_ptr += bitposition >> 3;
                bitposition &= 0x07;
            }
            else {
                data_ptr += pdo->pdo_datatype_size;
            }

            /* Note: mapped_pdo->domain is still a pointer to 
             * the old domain structure (the one in the linked
             * list), not the in the new sample time structure 
             * (ecat_data->st[].input_domain). Since this will be 
             * thrown away soon, we can misuse its 
             * endian_convert_list pointer as a counter. */
            if (pdo->pdo_datatype_size == 6 || pdo->pdo_datatype_size == 7) {
                /* Data sizes of 7 and 6 need to endian conversions:
                 * one uint32_t and one uint16_t */
                endian_convert = 
                    mapped_pdo->domain->endian_convert_list++;
                endian_convert->data_ptr = data_ptr;
                endian_convert->pdo_datatype_size = 4;

                endian_convert = 
                    mapped_pdo->domain->endian_convert_list++;
                endian_convert->data_ptr = data_ptr + 4;
                endian_convert->pdo_datatype_size = 2;
            }
            else if (pdo->pdo_datatype_size > 1) {
                /* All other data sizes except for 1 need only 
                 * one endian conversion */
                endian_convert = 
                    mapped_pdo->domain->endian_convert_list++;
                endian_convert->data_ptr = data_ptr;
                endian_convert->pdo_datatype_size =
                    pdo->pdo_datatype_size & ~1U;
            }
        }
    } 
    return NULL;
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
    struct ecat_master *master;
    unsigned int master_tid = 0;
    struct st_data *st;

    /* Allocate space for all the master and domain structures */
    for( st = ecat_data->st; st != &ecat_data->st[ecat_data->nst]; st++) {
        /* Although some of the *_count can be zero, kzalloc is able to 
         * deal with it. kzalloc returns ZERO_SIZE_PTR in this case.
         * kfree(ZERO_SIZE_PTR) is allowed! */

        st->master = 
            my_kcalloc(st->master_count, sizeof(*st->master), "master array");
        if (!st->master)
            return no_mem_msg;
        st->master_count = 0;

        st->input_domain = my_kcalloc(st->input_domain_count,
                sizeof(*st->input_domain), "input domain array");
        if (!st->input_domain)
            return no_mem_msg;
        st->input_domain_count = 0;

        st->output_domain = my_kcalloc(st->output_domain_count,
                sizeof(*st->output_domain), "output domain array");
        if (!st->output_domain)
            return no_mem_msg;
        st->output_domain_count = 0;

        /* Reset the counters. These are incremented later on again when
         * each master and domain structure in the arrays are initialised */
    }

    /* Go though the master list one by one and register the slaves */
    list_for_each_entry(master, &ecat_data->master_list, list) {
        unsigned int master_idx;
        struct ecat_master *new_master;
        struct ecat_slave *slave;
        const char *err;

        /* Find out to which tid this master will be allocated. The fastest
         * tid will get it */
        master_tid = master->fastest_tid;
        master_idx = ecat_data->st[master_tid].master_count++;
        new_master = &ecat_data->st[master_tid].master[master_idx];

        if ((err = init_slaves(master)))
            return err;

        pr_debug ("ecrt_master_activate(%p)\n", master->handle);
        if (ecrt_master_activate(master->handle)) {
            snprintf(errbuf, sizeof(errbuf),
                    "Master %i activate failed", master->id);
            return errbuf;
        }

        if ((err = init_domains(master, new_master)))
            return err;

        /* Now that the masters are activated, we can go through the
         * slave list again and setup the endian_convert_list in the domains
         * that the slave is registered in */
        list_for_each_entry(slave, &master->slave_list, list) {
            if ((err = init_slave_iodata(slave)))
                return err;
        }

        /* Pass the responsibility from the master in the list structure
         * to the one in the array */
        *new_master = *master;
        master->handle = NULL;

        /* Setup the lock, timing  and callbacks needed for sharing 
         * the master. */
        pthread_mutex_init(&new_master->lock, NULL);
//        ecrt_master_callbacks(new_master->handle, 
//                ecs_send_cb, ecs_receive_cb, new_master);

    }

    /* Release all temporary memory */
    cleanup_mem();

    return NULL;
}

void
ecs_end(void)
{
    struct st_data *st;

    pr_debug("%s()\n", __func__);

    if (!ecat_data)
        return;

    if (!list_empty(&ecat_data->master_list))
        cleanup_mem();

    for (st = ecat_data->st; st != &ecat_data->st[ecat_data->nst]; st++) {
        struct ecat_master *master;
        struct ecat_domain *domain;

        /* Free all allocated memory. Note that kfree(NULL) as well as
         * kfree(ZERO_SIZE_PTR), i.e. pointers returned by kmalloc(0), are
         * explicitly allowed */

        /* Free masters */
        for (master = st->master; 
                master && master != &st->master[st->master_count]; 
                master++) {
            if (master->handle) {
//                ecrt_master_callbacks(master->handle, NULL, NULL, NULL);
                ecrt_release_master(master->handle);
            }
            my_kfree(master->state, "master state");
        }
        my_kfree(st->master, "master array");

        /* Free input domains */
        for (domain = st->input_domain;
                domain != &st->input_domain[st->input_domain_count];
                domain++) {
            my_kfree(domain->endian_convert_list, "input copy list");
            my_kfree(domain->state, "domain state");
            my_kfree(domain->io_data, "input io data");
        }
        pr_debug("Freeing memory %p\n", st->output_domain);
        my_kfree(st->input_domain, "input domain array");
        pr_debug("finished Freeing memory %p\n", st->output_domain);

        /* Free output domains */
        for (domain = st->output_domain;
                domain != &st->output_domain[st->output_domain_count];
                domain++) {
            my_kfree(domain->endian_convert_list, "output copy list");
            my_kfree(domain->state, "domain state");
            my_kfree(domain->io_data, "output io data");
        }
        my_kfree(st->output_domain, "output domain array");
    }

    my_kfree(ecat_data, "ecat_data");
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
        unsigned int *st,    /* a 0 terminated list of sample times */
        unsigned int single_tasking
        )
{
    unsigned int nst;       /* Number of sample times */
    size_t len;
    int i;

    /* Make sure that the correct header version is used */
#if ECRT_VERSION_MAGIC != ECRT_VERSION(1,5)
#error Incompatible EtherCAT header file found.
#error This source is only compatible with EtherCAT Version 1.5
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
    len = (void*)&ecat_data->st[nst] - (void*)ecat_data;
    ecat_data = my_kcalloc(1, len, "ecat_data");
    if (!ecat_data) {
        return no_mem_msg;
    }

    /* Set default values for all variables */
    ecat_data->nst = nst;
    ecat_data->single_tasking = single_tasking;

    INIT_LIST_HEAD(&ecat_data->master_list);

    /* Set the period for all sample times */
    for ( i = 0; i < nst; i++ )
        ecat_data->st[i].period = st[i];

    return NULL;
}

/** Read non-aligned data types.
 *
 * The following functions are used to read non aligned data types.
 *
 * The return value is a generic data type that can represent the
 * source, left shifted so that the highest bits are occupied.
 *
 * The data is interpreted as little endian, which is the format
 * used by EtherCAT */

uint32_t ecs_read_uint24(void *byte)
{
    return 
        ((uint32_t) *(uint8_t*)(byte+2)) << 16
        | (uint32_t)*(uint16_t*)byte;
}

uint64_t ecs_read_uint40(void *byte)
{
    return 
        ((uint64_t) *(uint8_t*)(byte+4)) << 32
        | (uint64_t)*(uint32_t*)byte;
}

uint64_t ecs_read_uint48(void *byte)
{
    return
        ((uint64_t) *(uint16_t*)(byte+4)) << 40
        | (uint64_t)*(uint32_t*) byte;
}

uint64_t ecs_read_uint56(void *byte)
{
    return
        ((uint64_t)  *(uint8_t *)(byte+6)) << 48
        | ((uint64_t)*(uint16_t*)(byte+4)) << 40
        |  (uint64_t)*(uint32_t*) byte;
}

/** Write non-aligned data types.
 *
 * The following functions are used to write non-aligned data types.
 *
 * The source value is a generic data type that can represent the
 * destination.
 *
 * The data is interpreted as little endian, which is the format
 * used by EtherCAT */
void ecs_write_uint24(void *byte, uint32_t value)
{
    *(uint16_t*) byte    =  value;
    *(uint8_t *)(byte+2) =  value >> 16;
}
void ecs_write_uint40(void *byte, uint64_t value)
{
    *(uint32_t*) byte    =  value;
    *(uint8_t *)(byte+4) =  value >> 32;
}
void ecs_write_uint48(void *byte, uint64_t value)
{
    *(uint32_t*) byte    =  value;
    *(uint16_t*)(byte+4) =  value >> 32;
}
void ecs_write_uint56(void *byte, uint64_t value)
{
    *(uint32_t*) byte    =  value;
    *(uint16_t*)(byte+4) =  value >> 32;
    *(uint8_t *)(byte+6) =  value >> 48;
}
