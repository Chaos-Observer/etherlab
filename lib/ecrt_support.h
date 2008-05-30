#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <inttypes.h>
#define __init
#endif

#include <include/ecrt.h>
#include <include/etl_data_info.h>

struct pdo {
    unsigned int pdo_info_index;
    unsigned int pdo_entry_info_index;

    enum si_datatype_t pdo_datatype;
    void **address;
    unsigned int *bitoffset;
};

/* Structure to temporarily store SDO objects prior to registration */
struct sdo_config {
    /* The data type of the sdo: 
     * only si_uin8_t, si_uint16_t, si_uint32_t are allowed */
    enum si_datatype_t datatype;

    /* SDO values. Used by EtherCAT functions */
    uint16_t sdo_index;
    uint8_t sdo_subindex;
    uint32_t value;
};

void ecs_send(int tid);
void ecs_receive(int tid);
const char *ecs_start(void);

const char*
__init ecs_reg_slave(
        unsigned int tid,       /**< Task id of the task this slave will 
                                     run in */
        unsigned int master_id, /**< Master id this slave is connected to */
        unsigned int domain_id, /**< Domain id this slave should be assigned
                                     to */

        uint16_t slave_alias,   /**< Alias number as the base from which 
                                     the position is determined */
        uint16_t slave_position,/**< EtherCAT location of the slave in the 
                                     bus */
        uint32_t vendor_id,     /**< vendor ID */
        uint32_t product_code,  /**< product code */

        unsigned int sdo_config_count, /**< The number of SDO configuration 
                                          objects passed in \a sdo_config */
        const struct sdo_config *sdo_config, /**< Slave configuration objects */

        const ec_pdo_info_t *pdo_info, /**< PDO Configuration objects */

        unsigned int pdo_mapping_count, /**< Number of PDO mapping objects 
                                             passed in \a pdo_mapping */
        const struct pdo *pdo /**< PDO mapping objects */
        );

ec_master_t *ecs_get_master_ptr(
        unsigned int master_id, 
        const char **errmsg);

void ecs_end(void);

const char *ecs_init( 
        unsigned int *st       /* Zero terminated list of sample times in 
                                 * microseconds */
        );
