#include <stdint.h>
#include <ecrt.h>

#ifdef __cplusplus
extern "C" {
#endif

struct pdo_map {
    uint16_t pdo_entry_index;
    uint8_t pdo_entry_subindex;
    unsigned int datatype;
    unsigned int idx;
    void *address;
};

/* Structure to temporarily store SDO objects prior to registration */
struct soe_config {
    /* SoE values. Used by EtherCAT functions */
    uint16_t idn;
    const char *data;
    size_t data_len;
};

/* Structure to temporarily store SDO objects prior to registration */
struct sdo_config {
    /* The data type of the sdo: 
     * only si_uin8_t, si_uint16_t, si_uint32_t are allowed */
    unsigned int datatype;

    /* SDO values. Used by EtherCAT functions */
    uint16_t sdo_index;
    size_t sdo_attribute;   /* Either Subindex, or length of byte_array */
    uint32_t value;
    const char *byte_array;
};

struct ecat_master;

void ecs_send(int tid);
void ecs_receive(int tid);
const char *ecs_start(void);

const char*
ecs_reg_slave(
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

        unsigned int soe_config_count, /**< Number of SoE Configuration
                                         objects */
        const struct soe_config *soe_config, /**< SoE Configuration data */

        const ec_sync_info_t *pdo_info, /**< PDO Configuration objects */

        const uint32_t *dc_opmode_config, /**< Vector of 5 values:
                                            * [ AssignActivate,
                                            *   CycleTimeSync0, ShiftTimeSync0,
                                            *   CycleTimeSync1, ShiftTimeSync1]
                                            *
                                            * NULL = no dc */

        unsigned int pdo_input_count, /**< Number of RxPDO mapping objects 
                                             passed in \a pdo */
        unsigned int pdo_output_count, /**< Number of TxPDO mapping objects 
                                             passed in \a pdo */
        const struct pdo_map *input_pdo /**< PDO mapping objects */
        );

const char *ecs_setup_master(
        unsigned int master_id, 
        unsigned int refclk_sync_dec,
        void **master);

ec_domain_t *ecs_get_domain_ptr(
        unsigned int master_id, 
        unsigned int domain_id, 
        ec_direction_t dir,
        unsigned int tid,
        const char **errmsg);

void ecs_end(void);

const char *ecs_init( 
        unsigned int *st,       /* Zero terminated list of sample times in 
                                 * nanoseconds */
        unsigned int single_tasking /* Set if the model is single tasking,
                                     * even though there are more than one
                                     * sample time */
        );

/** Read non-aligned data types. */
uint32_t ecs_read_uint24(void *byte);
uint64_t ecs_read_uint40(void *byte);
uint64_t ecs_read_uint48(void *byte);
uint64_t ecs_read_uint56(void *byte);

/** Write non-aligned data types. */
void ecs_write_uint24(void *byte, uint32_t value);
void ecs_write_uint40(void *byte, uint64_t value);
void ecs_write_uint48(void *byte, uint64_t value);
void ecs_write_uint56(void *byte, uint64_t value);

#ifdef __cplusplus
}
#endif

