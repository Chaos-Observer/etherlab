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

#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include <time.h>
#include "ecrt_support.h"

#ifndef EC_TIMESPEC2NANO
#define EC_TIMESPEC2NANO(TV) \
    (((TV).tv_sec - 946684800ULL) * 1000000000ULL + (TV).tv_nsec)
#endif

/* The following message gets repeated quite frequently. */
char *no_mem_msg = "Could not allocate memory";
char errbuf[256];

#define pr_debug(fmt, args...) printf(fmt, args)

/*#######################################################################
 * List definition, ideas taken from linux kernel
  #######################################################################*/
/**
 * container_of - cast a member of a structure out to the containing structure
 * @ptr:        the pointer to the member.
 * @type:       the type of the container struct this is embedded in.
 * @member:     the name of the member within the struct.
 *
 */
#define container_of(ptr, type, member) ({                      \
        const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
        (type *)( (char *)__mptr - offsetof(type,member) );})

/*
 * Simple doubly linked list implementation.
 *
 * Some of the internal functions ("__xxx") are useful when
 * manipulating whole lists rather than single entries, as
 * sometimes we already know the next/prev entries and we can
 * generate better code by using them directly rather than
 * using the generic single-entry routines.
 */

struct list_head {
    struct list_head *next, *prev;
};

static void INIT_LIST_HEAD(struct list_head *list)
{
	list->next = list;
	list->prev = list;
}

/**
 * list_for_each_entry	-	iterate over list of given type
 * @pos:	the type * to use as a loop cursor.
 * @head:	the head for your list.
 * @member:	the name of the list_struct within the struct.
 */
#define list_for_each(pos, head, member)				\
	for (pos = container_of((head)->next, typeof(*pos), member);	\
	     &pos->member != (head); 	                                \
	     pos = container_of(pos->member.next, typeof(*pos), member))

/**
 * list_add_tail - add a new entry
 * @new: new entry to be added
 * @head: list head to add it before
 *
 * Insert a new entry before the specified head.
 * This is useful for implementing queues.
 */
static void list_add_tail(struct list_head *new, struct list_head *head)
{
    new->next = head;
    new->prev = head->prev;
    head->prev->next = new;
    head->prev = new;
}

/*#######################################################################*/

struct endian_convert_t {
    void (*copy)(const struct endian_convert_t*);
    void *dst;
    const uint8_t *src;
    size_t index;
};


/* Every domain has one of these structures. There can exist exactly one
 * domain for a sample time on an EtherCAT Master. */
struct ecat_domain {
    struct list_head list;      /* Linked list of domains. This list
                                 * is only used prior to ecs_start(), where
                                 * it is reworked into an array for faster
                                 * access */

    char input;                 /* Input domain (TxPdo's) */
    char output;                /* Output domain (RxPdo's) */

    unsigned int id;            /* RTW id number assigned to this domain */

    ec_domain_t *handle;        /* used when calling EtherCAT functions */
    ec_domain_state_t state;    /* pointer to domain's state */

    struct ecat_master *master; /* The master this domain is registered
                                 * in. Every domain has exactly 1 master */

    unsigned int tid;           /* Id of the corresponding RealTime Workshop
                                 * task */

    size_t  input_count;
    size_t output_count;

    struct endian_convert_t *input_convert_list;
    struct endian_convert_t *output_convert_list;

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
    ec_master_state_t state;    /* Pointer for master's state */

    unsigned int refclk_trigger_init; /* Decimation for reference clock
                                         == 0 => do not use dc */
    unsigned int refclk_trigger; /* When == 1, trigger a time syncronisation */

    struct list_head domain_list;
};

/* Data used by the EtherCAT support layer */
static struct ecat_data {
    unsigned int nst;           /* Number of unique RTW sample times */
    unsigned int single_tasking;

    /* This list is used to store all masters during the registration
     * process. Thereafter this list is reworked in ecs_start(), moving 
     * the masters and domains into the st[] struct below */
    struct list_head master_list;

} ecat_data = {
    .master_list = {&ecat_data.master_list, &ecat_data.master_list},
};

/////////////////////////////////////////////////

/** Read non-aligned data types.
 *
 * The following functions are used to read non aligned data types.
 *
 * The return value is a generic data type that can represent the
 * source, left shifted so that the highest bits are occupied.
 *
 * The data is interpreted as little endian, which is the format
 * used by EtherCAT */

/*****************************************************************/
static void
ecs_copy_uint8(const struct endian_convert_t *c)
{
    *(uint8_t*)c->dst = *(uint8_t*)c->src;
}

/*****************************************************************/
static void
ecs_write_uint1(const struct endian_convert_t *c)
{
    uint8_t mask = 1U << c->index;
    uint8_t *val = c->dst;

    *val = (*val & ~mask) | ((*c->src & 1U) << c->index);
}


/*****************************************************************/
static void
ecs_write_uint2(const struct endian_convert_t *c)
{
    uint8_t mask = 3U << c->index;
    uint8_t *val = c->dst;

    *val = (*val & ~mask) | ((*c->src & 3U) << c->index);
}


/*****************************************************************/
static void
ecs_write_uint3(const struct endian_convert_t *c)
{
    uint8_t mask = 7U << c->index;
    uint8_t *val = c->dst;

    *val = (*val & ~mask) | ((*c->src & 7U) << c->index);
}


/*****************************************************************/
static void
ecs_write_uint4(const struct endian_convert_t *c)
{
    uint8_t mask = 15U << c->index;
    uint8_t *val = c->dst;

    *val = (*val & ~mask) | ((*c->src & 15U) << c->index);
}


/*****************************************************************/
static void
ecs_write_uint5(const struct endian_convert_t *c)
{
    uint8_t mask = 31U << c->index;
    uint8_t *val = c->dst;

    *val = (*val & ~mask) | ((*c->src & 31U) << c->index);
}


/*****************************************************************/
static void
ecs_write_uint6(const struct endian_convert_t *c)
{
    uint8_t mask = 63U << c->index;
    uint8_t *val = c->dst;

    *val = (*val & ~mask) | ((*c->src & 63U) << c->index);
}


/*****************************************************************/
static void
ecs_write_uint7(const struct endian_convert_t *c)
{
    uint8_t mask = 127U << c->index;
    uint8_t *val = c->dst;

    *val = (*val & ~mask) | ((*c->src & 127U) << c->index);
}

/*****************************************************************/
static void
ecs_write_le_uint16(const struct endian_convert_t *c)
{
    *(uint16_t*)c->dst = htole16(*(uint16_t*)c->src);
}

/*****************************************************************/
static void
ecs_write_le_uint24(const struct endian_convert_t *c)
{
    uint32_t value = htole32(*(uint32_t*)c->src);

    *(uint16_t*)c->dst = value >> 8;
    ((uint8_t*)c->dst)[2] = value;
}

/*****************************************************************/
static void
ecs_write_le_uint32(const struct endian_convert_t *c)
{
    *(uint32_t*)c->dst = htole32(*(uint32_t*)c->src);
}

/*****************************************************************/
static void
ecs_write_le_uint40(const struct endian_convert_t *c)
{
    uint64_t value = htole64(*(uint64_t*)c->src);

    *(uint32_t*)c->dst = value >> 8;
    ((uint8_t*)c->dst)[4] = value;
}

/*****************************************************************/
static void
ecs_write_le_uint48(const struct endian_convert_t *c)
{
    uint64_t value = htole64(*(uint64_t*)c->src);

    *(uint32_t*)c->dst = value >> 16;
    ((uint16_t*)c->dst)[2] = value;
}

/*****************************************************************/
static void
ecs_write_le_uint56(const struct endian_convert_t *c)
{
    uint64_t value = htole64(*(uint64_t*)c->src);

    *(uint32_t*)c->dst = value >> 24;
    ((uint16_t*)c->dst)[2] = value >> 8;
    ((uint8_t*)c->dst)[7] = value;
}

/*****************************************************************/
static void
ecs_write_le_uint64(const struct endian_convert_t *c)
{
    *(uint64_t*)c->dst = htole64(*(uint64_t*)c->src);
}

/*****************************************************************/
static void
ecs_write_le_single(const struct endian_convert_t *c)
{
    *(uint32_t*)c->dst = htole32(*(uint32_t*)c->src);
}

/*****************************************************************/
static void
ecs_write_le_double(const struct endian_convert_t *c)
{
    *(uint64_t*)c->dst = htole64(*(uint64_t*)c->src);
}

/*****************************************************************/
static void
ecs_write_be_uint16(const struct endian_convert_t *c)
{
    *(uint16_t*)c->dst = htobe16(*(uint16_t*)c->src);
}

/*****************************************************************/
static void
ecs_write_be_uint24(const struct endian_convert_t *c)
{
    uint32_t value = htobe32(*(uint32_t*)c->src);

    *(uint16_t*)c->dst = value >> 8;
    ((uint8_t*)c->dst)[2] = value;
}

/*****************************************************************/
static void
ecs_write_be_uint32(const struct endian_convert_t *c)
{
    *(uint32_t*)c->dst = htobe32(*(uint32_t*)c->src);
}

/*****************************************************************/
static void
ecs_write_be_uint40(const struct endian_convert_t *c)
{
    uint64_t value = htobe64(*(uint64_t*)c->src);

    *(uint32_t*)c->dst = value >> 8;
    ((uint8_t*)c->dst)[4] = value;
}

/*****************************************************************/
static void
ecs_write_be_uint48(const struct endian_convert_t *c)
{
    uint64_t value = htobe64(*(uint64_t*)c->src);

    *(uint32_t*)c->dst = value >> 16;
    ((uint16_t*)c->dst)[2] = value;
}

/*****************************************************************/
static void
ecs_write_be_uint56(const struct endian_convert_t *c)
{
    uint64_t value = htobe64(*(uint64_t*)c->src);

    *(uint32_t*)c->dst = value >> 24;
    ((uint16_t*)c->dst)[2] = value >> 8;
    ((uint8_t*)c->dst)[7] = value;
}

/*****************************************************************/
static void
ecs_write_be_uint64(const struct endian_convert_t *c)
{
    *(uint64_t*)c->dst = htobe64(*(uint64_t*)c->src);
}

/*****************************************************************/
static void
ecs_write_be_single(const struct endian_convert_t *c)
{
    *(uint32_t*)c->dst = htole32(*(uint32_t*)c->src);
}

/*****************************************************************/
static void
ecs_write_be_double(const struct endian_convert_t *c)
{
    *(uint64_t*)c->dst = htole64(*(uint64_t*)c->src);
}

/*****************************************************************/
/*****************************************************************/
static void
ecs_read_uint1(const struct endian_convert_t *c)
{
    *(uint8_t*)c->dst = (*c->src >> c->index) & 0x01;
}

/*****************************************************************/
static void
ecs_read_uint2(const struct endian_convert_t *c)
{
    *(uint8_t*)c->dst = (*c->src >> c->index) & 0x03;
}

/*****************************************************************/
static void
ecs_read_uint3(const struct endian_convert_t *c)
{
    *(uint8_t*)c->dst = (*c->src >> c->index) & 0x07;
}

/*****************************************************************/
static void
ecs_read_uint4(const struct endian_convert_t *c)
{
    *(uint8_t*)c->dst = (*c->src >> c->index) & 0x0F;
}

/*****************************************************************/
static void
ecs_read_uint5(const struct endian_convert_t *c)
{
    *(uint8_t*)c->dst = (*c->src >> c->index) & 0x1F;
}

/*****************************************************************/
static void
ecs_read_uint6(const struct endian_convert_t *c)
{
    *(uint8_t*)c->dst = (*c->src >> c->index) & 0x3F;
}

/*****************************************************************/
static void
ecs_read_uint7(const struct endian_convert_t *c)
{
    *(uint8_t*)c->dst = (*c->src >> c->index) & 0x7F;
}

/*****************************************************************/
static void
ecs_read_le_uint16(const struct endian_convert_t *c)
{
    *(uint16_t*)c->dst = le16toh(*(uint16_t*)c->src);
}

/*****************************************************************/
static void
ecs_read_le_uint24(const struct endian_convert_t *c)
{
    *(uint32_t*)c->dst = ((uint32_t)le16toh(*(uint16_t*)(c->src+1)) << 8) + *c->src;
}

/*****************************************************************/
static void
ecs_read_le_uint32(const struct endian_convert_t *c)
{
    *(uint32_t*)c->dst = le32toh(*(uint32_t*)c->src);
}

/*****************************************************************/
static void
ecs_read_le_uint40(const struct endian_convert_t *c)
{
    *(double*)c->dst = ((uint64_t)le32toh(*(uint32_t*)(c->src+1)) << 8) + *c->src;
}

/*****************************************************************/
static void
ecs_read_le_uint48(const struct endian_convert_t *c)
{
    *(double*)c->dst =
        ((uint64_t)le32toh(*(uint32_t*)(c->src+2)) << 16)
        + le16toh(*(uint16_t*)c->src);
}

/*****************************************************************/
static void
ecs_read_le_uint56(const struct endian_convert_t *c)
{
    *(double*)c->dst =
        ((uint64_t)le32toh(*(uint32_t*)(c->src+3)) << 24)
        + ((uint64_t)le16toh(*(uint16_t*)(c->src+1)) <<  8)
        + *c->src;
}

/*****************************************************************/
static void
ecs_read_le_uint64(const struct endian_convert_t *c)
{
    *(double*)c->dst = le64toh(*(uint64_t*)c->src);
}

/*****************************************************************/
static void
ecs_read_le_single(const struct endian_convert_t *c)
{
    *(uint32_t*)c->dst = le32toh(*(uint32_t*)c->src);
}

/*****************************************************************/
static void
ecs_read_le_double(const struct endian_convert_t *c)
{
    *(uint64_t*)c->dst = le64toh(*(uint64_t*)c->src);
}

/*****************************************************************/
static void
ecs_read_be_uint16(const struct endian_convert_t *c)
{
    *(uint16_t*)c->dst = be16toh(*(uint16_t*)c->src);
}

/*****************************************************************/
static void
ecs_read_be_uint24(const struct endian_convert_t *c)
{
    *(uint32_t*)c->dst = ((uint32_t)*c->src << 16) + be16toh(*(uint16_t*)(c->src+1));
}

/*****************************************************************/
static void
ecs_read_be_uint32(const struct endian_convert_t *c)
{
    *(uint32_t*)c->dst = be32toh(*(uint32_t*)c->src);
}

/*****************************************************************/
static void
ecs_read_be_uint40(const struct endian_convert_t *c)
{
    *(double*)c->dst = ((uint64_t)*c->src << 32) + be32toh(*(uint32_t*)(c->src+1));
}

/*****************************************************************/
static void
ecs_read_be_uint48(const struct endian_convert_t *c)
{
    *(double*)c->dst =
        ((uint64_t)be16toh(*(uint16_t*)c->src) << 32)
        + be32toh(*(uint32_t*)(c->src+2));
}

/*****************************************************************/
static void
ecs_read_be_uint56(const struct endian_convert_t *c)
{
    *(double*)c->dst =
        ((uint64_t)*c->src << 48)
        + ((uint64_t)be16toh(*(uint16_t*)(c->src+1)) << 32)
        + be32toh(*(uint32_t*)(c->src+3));
}

/*****************************************************************/
static void
ecs_read_be_uint64(const struct endian_convert_t *c)
{
    *(double*)c->dst = be64toh(*(uint64_t*)c->src);
}

/*****************************************************************/
static void
ecs_read_be_single(const struct endian_convert_t *c)
{
    *(uint32_t*)c->dst = be32toh(*(uint32_t*)c->src);
}


/*****************************************************************/
static void
ecs_read_be_double(const struct endian_convert_t *c)
{
    *(uint64_t*)c->dst = be64toh(*(uint64_t*)c->src);
}

/* Do input processing for a RTW task. It does the following:
 *      - calls ecrt_master_receive() for every master whose fastest 
 *        domain is in this task. 
 *      - calls ecrt_domain_process() for every domain in this task
 */
void
ecs_receive(struct ecs_handle *ecs_handle, size_t tid)
{
    struct ecat_master *master;
    struct ecat_domain *domain;
    struct endian_convert_t *conversion_list;
        
    printf("%s\n", __func__);

    list_for_each(master, &ecat_data.master_list, list) {
        if (master->fastest_tid == tid) {
            ecrt_master_receive(master->handle);
            ecrt_master_state(master->handle, &master->state);
        }
        
        printf("master %p\n", master->handle);
        list_for_each(domain, &master->domain_list, list) {

            if (domain->tid != tid)
                continue;

            ecrt_domain_process(domain->handle);
            ecrt_domain_state(domain->handle, &domain->state);
            printf("   queue %p %p\n", domain->handle, &domain->state);

            if (!domain->input)
                continue;

            /*
            for (conversion_list = domain->input_convert_list;
                    conversion_list->copy; conversion_list++)
                conversion_list->copy(conversion_list);
                */
        }
    }
}

/* Do EtherCAT output processing for a RTW task. It does the following:
 *      - calls ecrt_master_run() for every master whose fastest task
 *        domain is in this task
 *      - calls ecrt_master_send() for every domain in this task
 */
void
ecs_send(struct ecs_handle *ecs_handle, size_t tid)
{
    struct ecat_master *master;
    struct ecat_domain *domain;
    struct endian_convert_t *conversion_list;

    printf("%s\n", __func__);
    list_for_each(master, &ecat_data.master_list, list) {
        list_for_each(domain, &master->domain_list, list) {

            if (domain->tid != tid)
                continue;

            /*
                || !domain->output)
            for (conversion_list = domain->input_convert_list;
                    conversion_list->copy; conversion_list++)
                conversion_list->copy(conversion_list);
                */

            ecrt_domain_queue(domain->handle);
        }

        if (master->fastest_tid == tid) {
            struct timespec tp;

            clock_gettime(CLOCK_MONOTONIC, &tp);
            ecrt_master_application_time(master->handle,
                    EC_TIMESPEC2NANO(tp));

            if (master->refclk_trigger_init && !--master->refclk_trigger) {
                ecrt_master_sync_reference_clock(master->handle);
                master->refclk_trigger = master->refclk_trigger_init;
            }

            ecrt_master_sync_slave_clocks(master->handle);
            ecrt_master_send(master->handle);
        }
    }
}

/***************************************************************************/
struct ecat_master *get_master(
        unsigned int master_id, unsigned int tid, const char **errmsg)
{
    struct ecat_master *master;

    list_for_each(master, &ecat_data.master_list, list) {
        if (master->id == master_id) {

            if (master->fastest_tid > tid)
                master->fastest_tid = tid;

            return master;
        }
    }

    master = calloc(1, sizeof(struct ecat_master));
    master->id = master_id;
    master->fastest_tid = tid;
    INIT_LIST_HEAD(&master->domain_list);
    list_add_tail(&master->list, &ecat_data.master_list);

    master->handle = ecrt_request_master(master_id);
    printf("New manster(%u) = %p\n", master_id, master->handle);

    if (!master->handle) {
        snprintf(errbuf, sizeof(errbuf),
                "ecrt_request_master(%u) failed", master_id);
        *errmsg = errbuf;
        return NULL;
    }

    return master;
}

/***************************************************************************/
        struct ecat_domain *
get_domain( struct ecat_master *master, unsigned int domain_id,
        char input, char output, unsigned int tid, const char **errmsg)
{
    struct ecat_domain *domain;

    printf("get_comain(master=%u, domain=%u, input=%u, output=%u, tid=%u)\n",
            master->id, domain_id, input, output, tid);

    list_for_each(domain, &master->domain_list, list) {
        printf("check: domain=%u, input=%u output=%u, tid=%u\n",
                domain->id, domain->output, domain->input, domain->tid);
        if (domain->id == domain_id
                && ((domain->output && output) || (domain->input && input))
                && domain->tid == tid) {
            printf("Fojund domain\n");
            return domain;
        }
    }

    domain = calloc(1, sizeof(struct ecat_domain));
    domain->id = domain_id;
    domain->tid = tid;
    domain->master = master;
    domain->input  =  input != 0;
    domain->output = output != 0;
    list_add_tail(&domain->list, &master->domain_list);

    domain->handle = ecrt_master_create_domain(master->handle);
    printf("New domain(%u) = %p IP=%u, OP=%u, tid=%u\n",
            domain_id, domain->handle, input, output, tid);

    if (!domain->handle) {
        snprintf(errbuf, sizeof(errbuf),
                "ecrt_master_create_domain(master=%u) failed", master->id);
        *errmsg = errbuf;
        return NULL;
    }

    return domain;
}

    const char *
init_domains( struct ecat_master *master)
{
    return NULL;
}

/***************************************************************************/
    const char *
register_pdos( ec_slave_config_t *slave_config, struct ecat_domain *domain,
        char dir, struct pdo_map *pdo_map, size_t count)
{
    const struct pdo_map *pdo_map_end = pdo_map + count;
    const char *failed_method;

    for (; pdo_map != pdo_map_end; pdo_map++) {
        unsigned int bitlen = pdo_map->datatype % 1000;

        pdo_map->offset = ecrt_slave_config_reg_pdo_entry(
                slave_config,
                pdo_map->pdo_entry_index,
                pdo_map->pdo_entry_subindex,
                domain->handle,
                &pdo_map->bit_pos);

        printf("offset=%i %i\n", pdo_map->offset, pdo_map_end - pdo_map);

        pdo_map->domain = domain;
        if (dir)
            domain->input_count++;
        else
            domain->output_count++;

        if (pdo_map->offset < 0) {
            failed_method = "ecrt_slave_config_reg_pdo_entry";
            goto out_slave_failed;
        }

        if (bitlen < 8) {
            pdo_map->bit_pos += bitlen * pdo_map->idx;
            pdo_map->offset += pdo_map->bit_pos / 8;
            pdo_map->bit_pos = pdo_map->bit_pos % 8;
        }
        else {
            if (pdo_map->bit_pos) {
                snprintf(errbuf, sizeof(errbuf), 
                        "Pdo Entry #x%04X.%u is not byte aligned",
                        pdo_map->pdo_entry_index,
                        pdo_map->pdo_entry_subindex);
                return errbuf;
            }
            pdo_map->offset += bitlen*pdo_map->idx / 8;
        }
    }

    return NULL;

out_slave_failed:
    snprintf(errbuf, sizeof(errbuf), 
            "%s() failed ", failed_method);
    return errbuf;
}

/***************************************************************************/
    const char *
init_slave( struct ecs_handle *ecs_handle, size_t nst,
        const struct ec_slave *slave)
{
    struct ecat_master *master;
    struct ecat_domain *domain;
    ec_slave_config_t *slave_config;
    const struct sdo_config *sdo;
    const struct soe_config *soe;
    const char *failed_method;

    printf("Initializing slave %u\n", slave->position);
    if (!(master = get_master(slave->master, slave->tid, &failed_method)))
        return failed_method;

    slave_config = ecrt_master_slave_config(master->handle, slave->alias,
            slave->position, slave->vendor, slave->product);
    if (!slave_config) {
        failed_method = "ecrt_master_slave_config";
        goto out_slave_failed;
    }

    /* Inform EtherCAT of how the slave configuration is expected */
    if (ecrt_slave_config_pdos(slave_config, EC_END, slave->ec_sync_info)) {
        failed_method = "ecrt_slave_config_pdos";
        goto out_slave_failed;
    }

    /* Send SDO configuration to the slave */
    for (sdo = slave->sdo_config; 
            sdo != &slave->sdo_config[slave->sdo_config_count]; sdo++) {
        if (sdo->byte_array) {
            if (ecrt_slave_config_complete_sdo(slave_config, 
                        sdo->sdo_index, (const uint8_t*)sdo->byte_array,
                        sdo->sdo_attribute)) {
                failed_method = "ecrt_slave_config_complete_sdo";
                goto out_slave_failed;
            }
        }
        else {
            switch (sdo->datatype) {
                case 1008U:
                    if (ecrt_slave_config_sdo8(slave_config, 
                                sdo->sdo_index, sdo->sdo_attribute,
                                (uint8_t)sdo->value)) {
                        failed_method = "ecrt_slave_config_sdo8";
                        goto out_slave_failed;
                    }
                    break;

                case 1016U:
                    if (ecrt_slave_config_sdo16(slave_config, 
                                sdo->sdo_index, sdo->sdo_attribute,
                                (uint16_t)sdo->value)) {
                        failed_method = "ecrt_slave_config_sdo16";
                        goto out_slave_failed;
                    }
                    break;

                case 1032U:
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
                    (const uint8_t*)soe->data,
                    soe->data_len)) {
            failed_method = "ecrt_slave_config_idn";
            goto out_slave_failed;
        }
    }

    /* Configure distributed clocks for slave if assign_activate is set */
    if (slave->dc_config[0]) {
        ecrt_slave_config_dc(slave_config, slave->dc_config[0],
                slave->dc_config[1], slave->dc_config[2],
                slave->dc_config[3], slave->dc_config[4]);
    }

    /* Register RxPdo's (output domain) */
    if (slave->rxpdo_count) {
        const char *err;

        domain = get_domain(master, slave->domain, 0, 1, slave->tid, &err);
        if (!domain)
            return err;

        if ((err = register_pdos(slave_config, domain, 0,
                        slave->pdo_map, slave->rxpdo_count)))
            return err;
    }

    /* Register TxPdo's (input domain) */
    if (slave->txpdo_count) {
        const char *err;

        domain = get_domain(master, slave->domain, 1, 0, slave->tid, &err);
        if (!domain)
            return err;

        if ((err = register_pdos(slave_config, domain, 1,
                        slave->pdo_map + slave->rxpdo_count,
                        slave->txpdo_count)))
            return err;
    }

    return NULL;

out_slave_failed:
    snprintf(errbuf, sizeof(errbuf), 
            "%s() failed ", failed_method);
    return errbuf;
}

/***************************************************************************/
const char * ecs_start( 
        const struct ec_slave *slave_head,
        unsigned int *st,       /* List of sample times in nanoseconds */
        size_t nst,
        unsigned int single_tasking,    /* Set if the model is single tasking,
                                         * even though there are more than one
                                         * sample time */
        struct ecs_handle **ecs_handle
        )
{
    const char *err;
    const struct ec_slave *slave;
    struct ecat_master *master;

    ecat_data.nst = nst;
    ecat_data.single_tasking = single_tasking;

    for (slave = slave_head; slave; slave = slave->next) {
    printf("init: %i\n", __LINE__);
        if ((err = init_slave(*ecs_handle, nst, slave)))
            goto out;
    }
    printf("init: %i\n", __LINE__);

    list_for_each(master, &ecat_data.master_list, list) {
        struct ecat_domain *domain;

        printf("init: %i\n", __LINE__);
        if (ecrt_master_activate(master->handle)) {
            snprintf(errbuf, sizeof(errbuf),
                    "Master %i activate failed", master->id);
            return errbuf;
        }

        list_for_each(domain, &master->domain_list, list) {
            domain->io_data = ecrt_domain_data(domain->handle);
            printf("domain %p master=%u domain=%u, IP=%u, OP=%u, tid=%u\n",
                    domain->io_data, domain->master->id, domain->id,
                    domain->input, domain->output, domain->tid);

            domain->input_convert_list =
                calloc(domain->input_count + domain->output_count,
                        sizeof(struct endian_convert_t));
            domain->output_convert_list =
                domain->input_convert_list + domain->input_count + 1;

            domain->input_count = 0;
            domain->output_count = 0;
        }
    }

    for (slave = slave_head; slave; slave = slave->next) {
        struct pdo_map *pdo_map = slave->pdo_map;
        struct pdo_map *pdo_map_end = slave->pdo_map + slave->rxpdo_count;

        for (; pdo_map != pdo_map_end; pdo_map++) {
            struct endian_convert_t *convert =
                pdo_map->domain->output_convert_list
                + pdo_map->domain->output_count++;
            size_t bitlen = pdo_map->datatype % 1000;
            size_t bytes = bitlen / 8;

            convert->src = pdo_map->address;
            convert->dst = pdo_map->domain->io_data
                + pdo_map->offset
                + bytes * pdo_map->idx;

            if (pdo_map->datatype < 1008) {
                /* bit operations */
                void (*convert_funcs[])(const struct endian_convert_t *) = {
                    ecs_write_uint1, ecs_write_uint2,
                    ecs_write_uint3, ecs_write_uint4,
                    ecs_write_uint5, ecs_write_uint6,
                    ecs_write_uint7,
                };
                size_t bitpos = pdo_map->bit_pos + bitlen * pdo_map->idx;
                convert->copy = convert_funcs[bitlen-1];
                convert->dst = pdo_map->domain->io_data
                    + pdo_map->offset
                    + bitpos / 8;
                convert->index = bitpos % 8;
            }
            else if (pdo_map->datatype > 3000) {
                /* floating points */
                void (*convert_funcs[][2])(const struct endian_convert_t *) = {
                    {ecs_write_le_single, ecs_write_be_single},
                    {ecs_write_le_double, ecs_write_be_double},
                };
                convert->copy =
                    convert_funcs[bytes / 4 - 1][pdo_map->bigendian];
            }
            else {
                /* integers */
                void (*convert_funcs[][2])(const struct endian_convert_t *) = {
                    {ecs_copy_uint8,      ecs_copy_uint8     },
                    {ecs_write_le_uint16, ecs_write_be_uint16},
                    {ecs_write_le_uint24, ecs_write_be_uint24},
                    {ecs_write_le_uint32, ecs_write_be_uint32},
                    {ecs_write_le_uint40, ecs_write_be_uint40},
                    {ecs_write_le_uint48, ecs_write_be_uint48},
                    {ecs_write_le_uint56, ecs_write_be_uint56},
                    {ecs_write_le_uint64, ecs_write_be_uint64},
                };
                convert->copy =
                    convert_funcs[bytes - 1][pdo_map->bigendian];
            }
        }

        /* At this point pdo_map points to the start of TxPdo's
         * Only have to update pdo_map_end */
        pdo_map_end += slave->txpdo_count;
        for (; pdo_map != pdo_map_end; pdo_map++) {
            struct endian_convert_t *convert =
                pdo_map->domain->input_convert_list
                + pdo_map->domain->input_count++;
            size_t bitlen = pdo_map->datatype % 1000;
            size_t bytes = bitlen / 8;

            convert->dst = pdo_map->address;
            convert->src = pdo_map->domain->io_data
                + pdo_map->offset
                + bytes * pdo_map->idx;

            if (pdo_map->datatype < 1008) {
                void (*convert_funcs[])(const struct endian_convert_t *) = {
                    ecs_read_uint1, ecs_read_uint2,
                    ecs_read_uint3, ecs_read_uint4,
                    ecs_read_uint5, ecs_read_uint6,
                    ecs_read_uint7,
                };
                size_t bitpos = pdo_map->bit_pos + bitlen * pdo_map->idx;
                convert->copy = convert_funcs[bitlen-1];
                convert->src = pdo_map->domain->io_data
                    + pdo_map->offset
                    + bitpos / 8;
                convert->index = bitpos % 8;
            }
            else if (pdo_map->datatype > 3000) {
                void (*convert_funcs[][2])(const struct endian_convert_t *) = {
                    {ecs_read_le_single, ecs_read_be_single},
                    {ecs_read_le_double, ecs_read_be_double},
                };
                convert->copy =
                    convert_funcs[bytes / 4 - 1][pdo_map->bigendian];
            }
            else {
                void (*convert_funcs[][2])(const struct endian_convert_t *) = {
                    {ecs_copy_uint8,     ecs_copy_uint8    },
                    {ecs_read_le_uint16, ecs_read_be_uint16},
                    {ecs_read_le_uint24, ecs_read_be_uint24},
                    {ecs_read_le_uint32, ecs_read_be_uint32},
                    {ecs_read_le_uint40, ecs_read_be_uint40},
                    {ecs_read_le_uint48, ecs_read_be_uint48},
                    {ecs_read_le_uint56, ecs_read_be_uint56},
                    {ecs_read_le_uint64, ecs_read_be_uint64},
                };
                convert->copy =
                    convert_funcs[bytes - 1][pdo_map->bigendian];
            }
        }
    }

    return NULL;

out:
    printf("initout : %s\n", err);
    return err;
}

/***************************************************************************/
void ecs_end(struct ecs_handle *ecs_handle, size_t nst)
{
}

/***************************************************************************/
    const char *
ecs_setup_master( unsigned int master_id,
        unsigned int refclk_sync_dec, void **master_p)
{
    const char *errmsg;
    /* Get the master structure, making sure not to change the task it 
     * is currently assigned to */
    struct ecat_master *master = get_master(master_id, ~0U, &errmsg);

    if (!master)
        return errmsg;

    if (master_p)
        *master_p = master->handle;

    master->refclk_trigger_init = refclk_sync_dec;
    master->refclk_trigger = 1;

    return NULL;
}

/***************************************************************************/
    ec_domain_t *
ecs_get_domain_ptr(unsigned int master_id, unsigned int domain_id, 
        ec_direction_t dir, unsigned int tid, const char **errmsg)
{
    struct ecat_master *master;
    struct ecat_domain *domain;

    if (!(master = get_master(master_id, tid, errmsg)))
        return NULL;

    domain = get_domain(master, domain_id,
            dir == EC_DIR_INPUT, dir == EC_DIR_OUTPUT, tid, errmsg);
    return domain ? domain->handle : NULL;
}

#if 0
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
#endif
