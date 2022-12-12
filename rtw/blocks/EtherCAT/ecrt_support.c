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

#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include <time.h>
#include <endian.h>
#include <string.h>
#include <pthread.h>
#include "ecrt_support.h"

extern pthread_key_t monotonic_time_key;

#if MT
#include <semaphore.h>
extern pthread_key_t tid_key;
#endif

#define ETL_TIMESPEC2NANO(TV) ((TV).tv_sec * 1000000000ULL + (TV).tv_nsec)

/* The following message gets repeated quite frequently. */
const char *no_mem_msg = "Could not allocate memory";
char errbuf[256];

extern int ETL_is_major_step(void);

#if 0
#  define pr_debug(fmt, args...) printf(fmt, args)
#define DEBUG_IO
#else
#  define pr_debug(fmt, args...)
#endif

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
#define container_of(ptr, type) (type *)(void*)((char *)ptr - offsetof(type,list))

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
#define list_for_each(pos, head, type)                  \
	for (pos = container_of((head)->next, type);    \
	     &pos->list != (head);                      \
	     pos = container_of(pos->list.next, type))

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

struct ec_slave_sdo {
    struct list_head list;
    unsigned int master;
    unsigned int alias;
    unsigned int position;
    unsigned int len;
    void **addr;
};
struct list_head ec_slave_sdo_head = {&ec_slave_sdo_head, &ec_slave_sdo_head};


struct endian_convert_t {
    void (*copy)(const struct endian_convert_t*);
    void *dst;
    const void *src;
    size_t index;
};

/** EtherCAT domain.
 *
 * Every domain has one of these structures. There can exist exactly one
 * domain for a sample time on an EtherCAT Master.
 */
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
    unsigned int tid_trigger;

    size_t  input_count;
    size_t output_count;

    struct endian_convert_t *input_convert_list;
    struct endian_convert_t *output_convert_list;

    uint8_t *io_data;              /* IO data is located here */
};

/** EtherCAT master.
 *
 * For every EtherCAT Master, one such structure exists.
 */
struct ecat_master {
    struct list_head list;      /* Linked list of masters. This list
                                 * is only used prior to ecs_start(), where
                                 * it is reworked into an array for faster
                                 * access */

    unsigned int fastest_tid;   /* Fastest RTW task id that uses this
                                 * master */
    unsigned int tid_trigger;

    struct task_stats *task_stats; /* Index into global task_stats */

    unsigned int id;            /* Id of the EtherCAT master */
    ec_master_t *handle;        /* Handle retured by EtherCAT code */
    ec_master_state_t state;    /* Pointer for master's state */

    unsigned int refclk_trigger_init; /* Decimation for reference clock
                                         == 0 => do not use dc */
    unsigned int refclk_trigger; /* When == 1, trigger a time syncronisation */

#if MT
    sem_t lock;
#endif

    struct list_head domain_list;
};

/** EtherCAT support layer.
 *
 * Data used by the EtherCAT support layer.
 */
static struct ecat_data {
    unsigned int nst;           /* Number of unique RTW sample times */
    const unsigned int *st;
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
    *(uint8_t*)c->dst = *(const uint8_t*)c->src;
}

/*****************************************************************/

static void
ecs_write_uint1(const struct endian_convert_t *c)
{
    uint8_t mask = 1U << c->index;
    uint8_t *val = c->dst;

    *val = (*val & ~mask) | ((*(const uint8_t*)c->src & 1U) << c->index);
}

/*****************************************************************/

static void
ecs_write_uint2(const struct endian_convert_t *c)
{
    uint8_t mask = 3U << c->index;
    uint8_t *val = c->dst;

    *val = (*val & ~mask) | ((*(const uint8_t*)c->src & 3U) << c->index);
}

/*****************************************************************/

static void
ecs_write_uint3(const struct endian_convert_t *c)
{
    uint8_t mask = 7U << c->index;
    uint8_t *val = c->dst;

    *val = (*val & ~mask) | ((*(const uint8_t*)c->src & 7U) << c->index);
}

/*****************************************************************/

static void
ecs_write_uint4(const struct endian_convert_t *c)
{
    uint8_t mask = 15U << c->index;
    uint8_t *val = c->dst;

    *val = (*val & ~mask) | ((*(const uint8_t*)c->src & 15U) << c->index);
}

/*****************************************************************/

static void
ecs_write_uint5(const struct endian_convert_t *c)
{
    uint8_t mask = 31U << c->index;
    uint8_t *val = c->dst;

    *val = (*val & ~mask) | ((*(const uint8_t*)c->src & 31U) << c->index);
}

/*****************************************************************/

static void
ecs_write_uint6(const struct endian_convert_t *c)
{
    uint8_t mask = 63U << c->index;
    uint8_t *val = c->dst;

    *val = (*val & ~mask) | ((*(const uint8_t*)c->src & 63U) << c->index);
}

/*****************************************************************/

static void
ecs_write_uint7(const struct endian_convert_t *c)
{
    uint8_t mask = 127U << c->index;
    uint8_t *val = c->dst;

    *val = (*val & ~mask) | ((*(const uint8_t*)c->src & 127U) << c->index);
}

/*****************************************************************/

static void
ecs_write_le_uint16(const struct endian_convert_t *c)
{
    *(uint16_t*)c->dst = htole16(*(const uint16_t*)c->src);
}

/*****************************************************************/

static void
ecs_write_le_uint24(const struct endian_convert_t *c)
{
    uint32_t value = htole32(*(const uint32_t*)c->src);

    memcpy(c->dst, &value, 3);
}

/*****************************************************************/

static void
ecs_write_le_uint32(const struct endian_convert_t *c)
{
    *(uint32_t*)c->dst = htole32(*(const uint32_t*)c->src);
}

/*****************************************************************/

static void
ecs_write_le_uint40(const struct endian_convert_t *c)
{
    uint64_t value = htole64(*(const uint64_t*)c->src);

    memcpy(c->dst, &value, 5);
}

/*****************************************************************/

static void
ecs_write_le_uint48(const struct endian_convert_t *c)
{
    uint64_t value = htole64(*(const uint64_t*)c->src);

    memcpy(c->dst, &value, 6);
}

/*****************************************************************/

static void
ecs_write_le_uint56(const struct endian_convert_t *c)
{
    uint64_t value = htole64(*(const uint64_t*)c->src);

    memcpy(c->dst, &value, 7);
}

/*****************************************************************/

static void
ecs_write_le_uint64(const struct endian_convert_t *c)
{
    *(uint64_t*)c->dst = htole64(*(const uint64_t*)c->src);
}

/*****************************************************************/

static void
ecs_write_le_single(const struct endian_convert_t *c)
{
    *(uint32_t*)c->dst = htole32(*(const uint32_t*)c->src);
}

/*****************************************************************/

static void
ecs_write_le_double(const struct endian_convert_t *c)
{
    *(uint64_t*)c->dst = htole64(*(const uint64_t*)c->src);
}

/*****************************************************************/

static void
ecs_write_be_uint16(const struct endian_convert_t *c)
{
    *(uint16_t*)c->dst = htobe16(*(const uint16_t*)c->src);
}

/*****************************************************************/

static void
ecs_write_be_uint24(const struct endian_convert_t *c)
{
    uint32_t value = htobe32(*(const uint32_t*)c->src) >> 8;

    memcpy(c->dst, &value, 3);
}

/*****************************************************************/

static void
ecs_write_be_uint32(const struct endian_convert_t *c)
{
    *(uint32_t*)c->dst = htobe32(*(const uint32_t*)c->src);
}

/*****************************************************************/

static void
ecs_write_be_uint40(const struct endian_convert_t *c)
{
    uint64_t value = htobe64(*(const uint64_t*)c->src) >> 24;

    memcpy(c->dst, &value, 5);
}

/*****************************************************************/

static void
ecs_write_be_uint48(const struct endian_convert_t *c)
{
    uint64_t value = htobe64(*(const uint64_t*)c->src) >> 16;

    memcpy(c->dst, &value, 6);
}

/*****************************************************************/

static void
ecs_write_be_uint56(const struct endian_convert_t *c)
{
    uint64_t value = htobe64(*(const uint64_t*)c->src) >> 8;

    memcpy(c->dst, &value, 7);
}

/*****************************************************************/

static void
ecs_write_be_uint64(const struct endian_convert_t *c)
{
    *(uint64_t*)c->dst = htobe64(*(const uint64_t*)c->src);
}

/*****************************************************************/

static void
ecs_write_be_single(const struct endian_convert_t *c)
{
    *(uint32_t*)c->dst = htobe32(*(const uint32_t*)c->src);
}

/*****************************************************************/

static void
ecs_write_be_double(const struct endian_convert_t *c)
{
    *(uint64_t*)c->dst = htobe64(*(const uint64_t*)c->src);
}

/*****************************************************************/
/*****************************************************************/

static void
ecs_read_uint1(const struct endian_convert_t *c)
{
    *(uint8_t*)c->dst = (*(const uint8_t*)c->src >> c->index) & 0x01;
}

/*****************************************************************/

static void
ecs_read_uint2(const struct endian_convert_t *c)
{
    *(uint8_t*)c->dst = (*(const uint8_t*)c->src >> c->index) & 0x03;
}

/*****************************************************************/

static void
ecs_read_uint3(const struct endian_convert_t *c)
{
    *(uint8_t*)c->dst = (*(const uint8_t*)c->src >> c->index) & 0x07;
}

/*****************************************************************/

static void
ecs_read_uint4(const struct endian_convert_t *c)
{
    *(uint8_t*)c->dst = (*(const uint8_t*)c->src >> c->index) & 0x0F;
}

/*****************************************************************/

static void
ecs_read_uint5(const struct endian_convert_t *c)
{
    *(uint8_t*)c->dst = (*(const uint8_t*)c->src >> c->index) & 0x1F;
}

/*****************************************************************/

static void
ecs_read_uint6(const struct endian_convert_t *c)
{
    *(uint8_t*)c->dst = (*(const uint8_t*)c->src >> c->index) & 0x3F;
}

/*****************************************************************/

static void
ecs_read_uint7(const struct endian_convert_t *c)
{
    *(uint8_t*)c->dst = (*(const uint8_t*)c->src >> c->index) & 0x7F;
}

/*****************************************************************/

static void
ecs_read_le_uint16(const struct endian_convert_t *c)
{
    *(uint16_t*)c->dst = le16toh(*(const uint16_t*)c->src);
}

/*****************************************************************/

static void
ecs_read_le_uint24(const struct endian_convert_t *c)
{
    uint32_t val = 0;
    memcpy(&val, c->src, 3);
    *(uint32_t*)c->dst = le32toh(val);
}

/*****************************************************************/

static void
ecs_read_le_uint32(const struct endian_convert_t *c)
{
    *(uint32_t*)c->dst = le32toh(*(const uint32_t*)c->src);
}

/*****************************************************************/

static void
ecs_read_le_uint40(const struct endian_convert_t *c)
{
    uint64_t val = 0;
    memcpy(&val, c->src, 5);
    *(uint64_t*)c->dst = le64toh(val);
}

/*****************************************************************/

static void
ecs_read_le_uint48(const struct endian_convert_t *c)
{
    uint64_t val = 0;
    memcpy(&val, c->src, 6);
    *(uint64_t*)c->dst = le64toh(val);
}

/*****************************************************************/

static void
ecs_read_le_uint56(const struct endian_convert_t *c)
{
    uint64_t val = 0;
    memcpy(&val, c->src, 7);
    *(uint64_t*)c->dst = le64toh(val);
}

/*****************************************************************/

static void
ecs_read_le_uint64(const struct endian_convert_t *c)
{
    *(uint64_t*)c->dst = le64toh(*(const uint64_t*)c->src);
}

/*****************************************************************/

static void
ecs_read_le_single(const struct endian_convert_t *c)
{
    *(uint32_t*)c->dst = le32toh(*(const uint32_t*)c->src);
}

/*****************************************************************/

static void
ecs_read_le_double(const struct endian_convert_t *c)
{
    *(uint64_t*)c->dst = le64toh(*(const uint64_t*)c->src);
}

/*****************************************************************/

static void
ecs_read_be_uint16(const struct endian_convert_t *c)
{
    *(uint16_t*)c->dst = be16toh(*(const uint16_t*)c->src);
}

/*****************************************************************/

static void
ecs_read_be_uint24(const struct endian_convert_t *c)
{
    uint32_t val = 0;
    memcpy(&val, c->src, 3);
    *(uint64_t*)c->dst = be32toh(val) >> 8;
}

/*****************************************************************/

static void
ecs_read_be_uint32(const struct endian_convert_t *c)
{
    *(uint32_t*)c->dst = be32toh(*(const uint32_t*)c->src);
}

/*****************************************************************/

static void
ecs_read_be_uint40(const struct endian_convert_t *c)
{
    uint64_t val = 0;
    memcpy(&val, c->src, 5);
    *(uint64_t*)c->dst = be64toh(val) >> 24;
}

/*****************************************************************/

static void
ecs_read_be_uint48(const struct endian_convert_t *c)
{
    uint64_t val = 0;
    memcpy(&val, c->src, 6);
    *(uint64_t*)c->dst = be64toh(val) >> 16;
}

/*****************************************************************/

static void
ecs_read_be_uint56(const struct endian_convert_t *c)
{
    uint64_t val = 0;
    memcpy(&val, c->src, 7);
    *(uint64_t*)c->dst = be64toh(val) >> 8;
}

/*****************************************************************/

static void
ecs_read_be_uint64(const struct endian_convert_t *c)
{
    *(uint64_t*)c->dst = be64toh(*(const uint64_t*)c->src);
}

/*****************************************************************/

static void
ecs_read_be_single(const struct endian_convert_t *c)
{
    *(uint32_t*)c->dst = be32toh(*(const uint32_t*)c->src);
}


/*****************************************************************/

static void
ecs_read_be_double(const struct endian_convert_t *c)
{
    *(uint64_t*)c->dst = be64toh(*(const uint64_t*)c->src);
}

/*****************************************************************/

/* Do input processing for a RTW task.
 *
 * It does the following:
 *  - calls ecrt_master_receive() for every master whose fastest domain is in
 *    this task.
 *  - calls ecrt_domain_process() for every domain in this task
 */
void
ecs_receive(void)
{
    struct ecat_master *master;
    struct ecat_domain *domain;
    struct endian_convert_t *conversion_list;
    int trigger;
    unsigned int tid = 0;

#if MT
    tid = *(unsigned int*)pthread_getspecific(tid_key);
#endif

    if (!tid && !ETL_is_major_step())
        return;

    list_for_each(master, &ecat_data.master_list, struct ecat_master) {

#if MT
        sem_wait(&master->lock);
        trigger = master->fastest_tid == tid;
#else
        trigger = !--master->tid_trigger;
#endif

        if (trigger) {
#ifdef EC_HAVE_SYNC_TO
            struct timespec *monotonic_time =
                (struct timespec *) pthread_getspecific(monotonic_time_key);

            ecrt_master_application_time(master->handle,
                    ETL_TIMESPEC2NANO(*monotonic_time));
#endif
            ecrt_master_receive(master->handle);
            ecrt_master_state(master->handle, &master->state);

#ifdef DEBUG_IO
            pr_debug("%s master(%i)\n", __func__, master->fastest_tid);
#endif
        }

        list_for_each(domain, &master->domain_list, struct ecat_domain) {

#if MT
            trigger = domain->tid != tid;
#else
            trigger = --domain->tid_trigger;
#endif
            if (trigger)
                continue;

            ecrt_domain_process(domain->handle);
            ecrt_domain_state(domain->handle, &domain->state);

#ifdef DEBUG_IO
            pr_debug("%s domain(%i)\n", __func__, domain->tid);
#endif

            if (!domain->input)
                continue;

            for (conversion_list = domain->input_convert_list;
                    conversion_list->copy; conversion_list++) {
                conversion_list->copy(conversion_list);
            }
        }
#if MT
        sem_post(&master->lock);
#endif
    }
}

/* Do EtherCAT output processing for a RTW task.
 *
 * It does the following:
 * - calls ecrt_master_run() for every master whose fastest task domain is in
 *   this task
 * - calls ecrt_master_send() for every domain in this task
 */
void
ecs_send(void)
{
    struct ecat_master *master;
    struct ecat_domain *domain;
    struct endian_convert_t *conversion_list;
    int trigger;
    unsigned int tid = 0;

#if MT
    tid = *(unsigned int*)pthread_getspecific(tid_key);
#endif

    if (!tid && !ETL_is_major_step())
        return;

    list_for_each(master, &ecat_data.master_list, struct ecat_master) {

#if MT
        sem_wait(&master->lock);
#endif

        list_for_each(domain, &master->domain_list, struct ecat_domain) {

#if MT
            trigger = domain->tid != tid;
#else
            trigger = domain->tid_trigger;
#endif
            if (trigger)
                continue;
            domain->tid_trigger = domain->tid;

#ifdef DEBUG_IO
            pr_debug("%s domain(%i)\n", __func__, domain->tid);
#endif

            if (domain->output) {
                for (conversion_list = domain->output_convert_list;
                        conversion_list->copy; conversion_list++) {
                    conversion_list->copy(conversion_list);
                }
            }

            ecrt_domain_queue(domain->handle);
        }

#if MT
        trigger = master->fastest_tid == tid;
#else
        trigger = !master->tid_trigger;
#endif
        if (trigger) {
            struct timespec tp;
            master->tid_trigger = master->fastest_tid;

#ifndef EC_HAVE_SYNC_TO
            clock_gettime(CLOCK_MONOTONIC, &tp);
            ecrt_master_application_time(master->handle,
                    ETL_TIMESPEC2NANO(tp));
#endif

            if (master->refclk_trigger_init && !--master->refclk_trigger) {
#ifdef EC_HAVE_SYNC_TO
                clock_gettime(CLOCK_MONOTONIC, &tp);
                ecrt_master_sync_reference_clock_to(master->handle,
                        ETL_TIMESPEC2NANO(tp));
#else
                ecrt_master_sync_reference_clock(master->handle);
#endif
                master->refclk_trigger = master->refclk_trigger_init;
            }

            ecrt_master_sync_slave_clocks(master->handle);
            ecrt_master_send(master->handle);

#ifdef DEBUG_IO
            pr_debug("%s master(%i)\n", __func__, master->fastest_tid);
#endif
        }
#if MT
        sem_post(&master->lock);
#endif
    }
}

/***************************************************************************/

static struct ecat_master *
get_master(
        unsigned int master_id, unsigned int tid, const char **errmsg)
{
    struct ecat_master *master;

#if !MT
    /* Note that ecs_setup_master() sets tid to ~0U! */
    if (tid < ecat_data.nst)
        tid = ecat_data.st[tid] / ecat_data.st[0];
#endif

    list_for_each(master, &ecat_data.master_list, struct ecat_master) {

        if (master->id == master_id) {

            if (master->fastest_tid > tid) {
                master->fastest_tid = tid;
                master->tid_trigger = master->fastest_tid;
            }

            return master;
        }
    }

    master = calloc(1, sizeof(struct ecat_master));
    master->id = master_id;
    master->fastest_tid = tid;
    master->tid_trigger = tid;
#if MT
    sem_init(&master->lock, 0, 1);
#endif
    INIT_LIST_HEAD(&master->domain_list);
    list_add_tail(&master->list, &ecat_data.master_list);

    master->handle = ecrt_request_master(master_id);

    if (!master->handle) {
        snprintf(errbuf, sizeof(errbuf),
                "ecrt_request_master(%u) failed", master_id);
        *errmsg = errbuf;
        return NULL;
    }

    return master;
}

/***************************************************************************/

static struct ecat_domain *
get_domain( struct ecat_master *master, unsigned int domain_id,
        char input, char output, unsigned int tid, const char **errmsg)
{
    struct ecat_domain *domain;

#if !MT
    tid = ecat_data.st[tid] / ecat_data.st[0];
#endif

    pr_debug("get_domain(master=%u, domain=%u, input=%u, output=%u, tid=%u)\n",
            master->id, domain_id, input, output, tid);

    /* Go through every master's domain list to see whether the
     * required domain exists */
    list_for_each(domain, &master->domain_list, struct ecat_domain) {
        if (domain->id == domain_id
                && ((domain->output && output) || (domain->input && input))
                && domain->tid == tid) {

            /* Set input and output flags. The flags are cumulative */
            domain->input  |=  input != 0;
            domain->output |= output != 0;

            return domain;
        }
    }

    /* No domain found, create new one */
    domain = calloc(1, sizeof(struct ecat_domain));
    domain->id = domain_id;
    domain->tid = tid;
    domain->tid_trigger = domain->tid;
    domain->master = master;
    domain->input  =  input != 0;
    domain->output = output != 0;
    list_add_tail(&domain->list, &master->domain_list);

    domain->handle = ecrt_master_create_domain(master->handle);
    if (!domain->handle) {
        snprintf(errbuf, sizeof(errbuf),
                "ecrt_master_create_domain(master=%u) failed", master->id);
        *errmsg = errbuf;
        return NULL;
    }

    pr_debug("New domain(%u) = %p IP=%u, OP=%u, tid=%u\n",
            domain_id, domain->handle, input, output, tid);

    return domain;
}

/***************************************************************************/

static const char *
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

        pr_debug("offset=%i %li\n", pdo_map->offset, pdo_map_end - pdo_map);

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

static const char *
init_slave(const struct ec_slave *slave)
{
    struct ecat_master *master;
    struct ecat_domain *domain;
    ec_slave_config_t *slave_config;
    const struct sdo_config *sdo;
    const struct ec_slave_sdo *sdo_req;
    const struct soe_config *soe;
    const char *failed_method;

    pr_debug("Initializing slave %u\n", slave->position);
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

    /* Configure sync manager timeout settings */
    ecrt_slave_config_watchdog(slave_config,
            0, /* basic watchdog intervals (default is 2498 cycles of 40 ns,
                  zero means leave default setting) */
            30000 /* number of base intervals
                      FIXME make this configurable! */
        );

    /* Send SDO configuration to the slave */
    for (sdo = slave->sdo_config;
            sdo != &slave->sdo_config[slave->sdo_config_count]; sdo++) {
        if (sdo->byte_array) {
            if (sdo->sdo_subindex >= 0) {
                if (ecrt_slave_config_sdo(slave_config,
                            sdo->sdo_index, sdo->sdo_subindex,
                            (const uint8_t*)sdo->byte_array,
                            sdo->value)) {
                    failed_method = "ecrt_slave_config_sdo";
                    goto out_slave_failed;
                }
            }
            else {
                if (ecrt_slave_config_complete_sdo(slave_config,
                            sdo->sdo_index,
                            (const uint8_t*)sdo->byte_array,
                            sdo->value)) {
                    failed_method = "ecrt_slave_config_complete_sdo";
                    goto out_slave_failed;
                }
            }
        }
        else {
            union {
                uint8_t   u8;
                int8_t    s8;
                uint16_t u16;
                int16_t  s16;
                uint32_t u32;
                int32_t  s32;
                uint64_t u64;
                int64_t  s64;
                float    f32;
                double   f64;
            } value;
            const void *data = 0;
            size_t len = 0;

            switch (sdo->datatype) {
                case 1008U:
                    value.u8 = sdo->value;
                    data = &value.u8;
                    len = 1;
                    break;

                case 2008U:
                    value.s8 = sdo->value;
                    data = &value.s8;
                    len = 1;
                    break;

                case 1016U:
                    EC_WRITE_U16(&value.u16, sdo->value);
                    data = &value.u16;
                    len = 2;
                    break;

                case 2016U:
                    EC_WRITE_S16(&value.s16, sdo->value);
                    data = &value.s16;
                    len = 2;
                    break;

                case 1032U:
                    EC_WRITE_U32(&value.u32, sdo->value);
                    data = &value.u32;
                    len = 4;
                    break;

                case 2032U:
                    EC_WRITE_S32(&value.s32, sdo->value);
                    data = &value.s32;
                    len = 4;
                    break;

                case 1064U:
                    EC_WRITE_U64(&value.u64, sdo->value);
                    data = &value.u64;
                    len = 8;
                    break;

                case 2064U:
                    EC_WRITE_S64(&value.s64, sdo->value);
                    data = &value.s64;
                    len = 8;
                    break;

#ifdef EC_WRITE_REAL
                case 3032U:
                    EC_WRITE_REAL(&value.f32, sdo->value);
                    data = &value.f32;
                    len = 4;
                    break;
#endif

#ifdef EC_WRITE_LREAL
                case 3064U:
                    EC_WRITE_LREAL(&value.f64, sdo->value);
                    data = &value.f64;
                    len = 8;
                    break;
#endif
            }

            if (len && ecrt_slave_config_sdo(slave_config,
                        sdo->sdo_index, sdo->sdo_subindex,
                        (const uint8_t*)data, len)) {
                failed_method = "ecrt_slave_config_sdo";
                goto out_slave_failed;
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
    if (slave->dc_config.assign_activate) {
        if (slave->dc_config.cycle_time0 + slave->dc_config.cycle_time1 <= 0) {
            failed_method = "(ecrt_slave_config_dc: CycleTimeSYNC0 + CycleTimeSYNC1 > 0)";
            goto out_slave_failed;
        }
        ecrt_slave_config_dc(slave_config,
                slave->dc_config.assign_activate,
                slave->dc_config.cycle_time0,
                slave->dc_config.shift_time,
                slave->dc_config.cycle_time1,
                0);
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

    list_for_each(sdo_req, &ec_slave_sdo_head, struct ec_slave_sdo) {
        if (sdo_req->master != slave->master
                || sdo_req->alias != slave->alias
                || sdo_req->position != slave->position)
            continue;

        *sdo_req->addr =
            ecrt_slave_config_create_sdo_request(slave_config, 0, 0, sdo_req->len);
    }

    return NULL;

out_slave_failed:
    snprintf(errbuf, sizeof(errbuf),
            "%s() failed while configuring slave %u:%u",
            failed_method, slave->alias, slave->position);
    return errbuf;
}

/***************************************************************************/
int ecs_sdo_handler(
        unsigned int master_id,
        unsigned int alias,
        unsigned int position,
        unsigned int len,
        void **addr)
{
    struct ec_slave_sdo *s = malloc(sizeof(struct ec_slave_sdo));

    if (!s)
        return 1;

    s->master = master_id;
    s->alias = alias;
    s->position = position;
    s->len = len;
    s->addr = addr;

    list_add_tail(&s->list, &ec_slave_sdo_head);
    return 0;
}

/***************************************************************************/

const char *ecs_init(
        unsigned int *st,
        size_t nst,
        unsigned int single_tasking     /* Set if the model is single tasking,
                                         * even though there is more than one
                                         * sample time */
        )
{
    ecat_data.nst = nst;
    ecat_data.st  = st;
    ecat_data.single_tasking = single_tasking;

    return 0;
}

/***************************************************************************/

const char * ecs_start_slaves(
        const struct ec_slave *slave_head
        )
{
    const char *err;
    const struct ec_slave *slave;
    struct ecat_master *master;

    for (slave = slave_head; slave; slave = slave->next) {
    pr_debug("init: %i\n", __LINE__);
        if ((err = init_slave(slave)))
            goto out;
    }
    pr_debug("init: %i\n", __LINE__);

    list_for_each(master, &ecat_data.master_list, struct ecat_master) {
        struct ecat_domain *domain;

        pr_debug("init: %i\n", __LINE__);
        if (ecrt_master_activate(master->handle)) {
            snprintf(errbuf, sizeof(errbuf),
                    "Master %i activate failed", master->id);
            return errbuf;
        }

        list_for_each(domain, &master->domain_list, struct ecat_domain) {
            domain->io_data = ecrt_domain_data(domain->handle);
            pr_debug("domain %p master=%u domain=%u, IP=%u, OP=%u, tid=%u\n",
                    domain->io_data, domain->master->id, domain->id,
                    domain->input, domain->output, domain->tid);

            domain->input_convert_list =
                calloc(domain->input_count + domain->output_count + 2,
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
            convert->dst = pdo_map->domain->io_data + pdo_map->offset;

            if (pdo_map->datatype < 1008) {
                /* bit operations */
                static void (* const convert_funcs[])(
				const struct endian_convert_t *) = {
                    ecs_write_uint1, ecs_write_uint2,
                    ecs_write_uint3, ecs_write_uint4,
                    ecs_write_uint5, ecs_write_uint6,
                    ecs_write_uint7,
                };
                convert->copy = convert_funcs[bitlen-1];
                convert->dst = pdo_map->domain->io_data + pdo_map->offset;
                convert->index = pdo_map->bit_pos;
            }
            else if (pdo_map->datatype > 3000) {
                /* floating points */
                static void (* const convert_funcs[][2])(
				const struct endian_convert_t *) = {
                    {ecs_write_le_single, ecs_write_be_single},
                    {ecs_write_le_double, ecs_write_be_double},
                };
                convert->copy =
                    convert_funcs[bytes / 4 - 1][pdo_map->bigendian];
            }
            else {
                /* integers */
                static void (* const convert_funcs[][2])(
				const struct endian_convert_t *) = {
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
            convert->src = pdo_map->domain->io_data + pdo_map->offset;

            if (pdo_map->datatype < 1008) {
                static void (* const convert_funcs[])(
				const struct endian_convert_t *) = {
                    ecs_read_uint1, ecs_read_uint2,
                    ecs_read_uint3, ecs_read_uint4,
                    ecs_read_uint5, ecs_read_uint6,
                    ecs_read_uint7,
                };
                convert->copy = convert_funcs[bitlen-1];
                convert->src = pdo_map->domain->io_data + pdo_map->offset;
                convert->index = pdo_map->bit_pos;
            }
            else if (pdo_map->datatype > 3000) {
                static void (* const convert_funcs[][2])(
				const struct endian_convert_t *) = {
                    {ecs_read_le_single, ecs_read_be_single},
                    {ecs_read_le_double, ecs_read_be_double},
                };
                convert->copy =
                    convert_funcs[bytes / 4 - 1][pdo_map->bigendian];
            }
            else {
                static void (* const convert_funcs[][2])(
				const struct endian_convert_t *) = {
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
    pr_debug("initout : %s\n", err);
    return err;
}

/***************************************************************************/

void ecs_end(size_t nst)
{
    (void)nst;
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
        char input, char output, unsigned int tid, const char **errmsg)
{
    struct ecat_master *master;
    struct ecat_domain *domain;

    if (!(master = get_master(master_id, tid, errmsg)))
        return NULL;

    domain = get_domain(master, domain_id, input, output, tid, errmsg);
    return domain ? domain->handle : NULL;
}

/***************************************************************************/

#if TESTDTYPES
/* Compile with
 * gcc -DTESTDTYPES=1 -I../../include -o ecrt ecrt_support.c -lethercat -pthread
 */

#include <assert.h>

pthread_key_t monotonic_time_key;

int ETL_is_major_step(void)
{
    return 0;
}

int main(int argc, char** argv)
{
    uint8_t raw[] = {0x12, 0x34, 0x56, 0x78, 0xde, 0xad, 0xbe, 0xef};

    {
        uint8_t val = 0;
        struct endian_convert_t table = {
            .dst = &val,
            .src = raw+7,
            .index = 0,
        };

        ecs_read_uint1(&table); assert(val == 0x01);
        ecs_read_uint2(&table); assert(val == 0x03);
        ecs_read_uint3(&table); assert(val == 0x07);
        ecs_read_uint4(&table); assert(val == 0x0f);
        ecs_read_uint5(&table); assert(val == 0x0f);
        ecs_read_uint6(&table); assert(val == 0x2f);
        ecs_read_uint7(&table); assert(val == 0x6f);
        ecs_copy_uint8(&table); assert(val == 0xef);
    }

    {
        uint8_t val = 0;
        struct endian_convert_t table = {
            .dst = &val,
            .src = raw+7,
            .index = 0,
        };

        ecs_write_uint1(&table); assert(val == 0x01);
        ecs_write_uint2(&table); assert(val == 0x03);
        ecs_write_uint3(&table); assert(val == 0x07);
        ecs_write_uint4(&table); assert(val == 0x0f);
        ecs_write_uint5(&table); assert(val == 0x0f);
        ecs_write_uint6(&table); assert(val == 0x2f);
        ecs_write_uint7(&table); assert(val == 0x6f);
    }

    {
        uint8_t val = 0xff;
        struct endian_convert_t table = {
            .dst = &val,
            .src = raw,
            .index = 0,
        };

        ecs_write_uint1(&table); assert(val == 0xfe);
        ecs_write_uint2(&table); assert(val == 0xfe);
        ecs_write_uint3(&table); assert(val == 0xfa);
        ecs_write_uint4(&table); assert(val == 0xf2);
        ecs_write_uint5(&table); assert(val == 0xf2);
        ecs_write_uint6(&table); assert(val == 0xd2);
        ecs_write_uint7(&table); assert(val == 0x92);
    }

    {
        uint16_t val = 0;
        struct endian_convert_t table = {
            .dst = &val,
            .src = raw,
        };

        ecs_write_le_uint16(&table); assert(val == 0x3412);
        ecs_write_be_uint16(&table); assert(val == 0x1234);

        ecs_read_le_uint16(&table); assert(val == 0x3412);
        ecs_read_be_uint16(&table); assert(val == 0x1234);
    }

    {
        uint32_t val = ~0U;
        struct endian_convert_t table = {
            .dst = &val,
            .src = raw,
        };

        ecs_write_le_uint24(&table); assert(val == 0xff563412);
        ecs_write_be_uint24(&table); assert(val == 0xff123456);

        ecs_write_le_uint32(&table); assert(val == 0x78563412);
        ecs_write_be_uint32(&table); assert(val == 0x12345678);

        ecs_read_le_uint24(&table); assert(val  ==   0x563412);
        ecs_read_be_uint24(&table); assert(val  ==   0x123456);

        ecs_read_le_uint32(&table); assert(val  == 0x78563412);
        ecs_read_be_uint32(&table); assert(val  == 0x12345678);
    }

    {
        uint64_t val = ~0ULL;
        struct endian_convert_t table = {
            .dst = &val,
            .src = raw,
        };

        ecs_write_le_uint40(&table); assert(val == 0xffffffde78563412);
        ecs_write_be_uint40(&table); assert(val == 0xffffff12345678de);

        ecs_write_le_uint48(&table); assert(val == 0xffffadde78563412);
        ecs_write_be_uint48(&table); assert(val == 0xffff12345678dead);

        ecs_write_le_uint56(&table); assert(val == 0xffbeadde78563412);
        ecs_write_be_uint56(&table); assert(val == 0xff12345678deadbe);

        ecs_write_le_uint64(&table); assert(val == 0xefbeadde78563412);
        ecs_write_be_uint64(&table); assert(val == 0x12345678deadbeef);

        ecs_read_le_uint40(&table); assert(val  ==       0xde78563412);
        ecs_read_be_uint40(&table); assert(val  ==       0x12345678de);

        ecs_read_le_uint48(&table); assert(val  ==     0xadde78563412);
        ecs_read_be_uint48(&table); assert(val  ==     0x12345678dead);

        ecs_read_le_uint56(&table); assert(val  ==   0xbeadde78563412);
        ecs_read_be_uint56(&table); assert(val  ==   0x12345678deadbe);

        ecs_read_le_uint64(&table); assert(val  == 0xefbeadde78563412);
        ecs_read_be_uint64(&table); assert(val  == 0x12345678deadbeef);
    }


    {
        float val = 0.330, src, dst;

        struct endian_convert_t table = {
            .dst = &dst,
            .src = &src,
        };

        *(uint32_t*)&src = htole32(*(uint32_t*)&val);
        ecs_read_le_single(&table); assert(dst == val);
        ecs_write_le_single(&table); assert(dst == val);
        ecs_read_be_single(&table); assert(dst != val);
        ecs_write_be_single(&table); assert(dst != val);

        *(uint32_t*)&src = htobe32(*(uint32_t*)&val);
        ecs_read_be_single(&table); assert(dst == val);
        ecs_write_be_single(&table); assert(dst == val);
        ecs_read_le_single(&table); assert(dst != val);
        ecs_write_le_single(&table); assert(dst != val);
    }

    {
        double val = 0.330, src, dst;

        struct endian_convert_t table = {
            .dst = &dst,
            .src = &src,
        };

        *(uint64_t*)&src = htole64(*(uint64_t*)&val);
        ecs_read_le_double(&table); assert(dst == val);
        ecs_write_le_double(&table); assert(dst == val);
        ecs_read_be_double(&table); assert(dst != val);
        ecs_write_be_double(&table); assert(dst != val);

        *(uint64_t*)&src = htobe64(*(uint64_t*)&val);
        ecs_read_be_double(&table); assert(dst == val);
        ecs_write_be_double(&table); assert(dst == val);
        ecs_read_le_double(&table); assert(dst != val);
        ecs_write_le_double(&table); assert(dst != val);
    }

}
#endif
