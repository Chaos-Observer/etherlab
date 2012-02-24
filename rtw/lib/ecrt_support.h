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

struct ecs_handle;
struct ec_slave {
    const struct ec_slave *next;
    unsigned int tid;
    unsigned int master;
    unsigned int domain;
    unsigned int alias;
    unsigned int position;
    unsigned int vendor;
    unsigned int product;
    size_t sdo_config_count;
    const struct sdo_config *sdo_config;
    size_t soe_config_count;
    const struct soe_config *soe_config;
    const ec_sync_info_t *ec_sync_info;
    const uint32_t dc_config[5];
    size_t input_count;
    size_t output_count;
    const struct pdo_map *pdo_map;
};

void ecs_send(struct ecs_handle *, size_t tid);
void ecs_receive(struct ecs_handle *, size_t tid);

void ecs_end(struct ecs_handle *ecs_handle, size_t nst);

const char *ecs_start( 
        const struct ec_slave *slave_head,
        unsigned int *st,       /* List of sample times in nanoseconds */
        size_t nst,
        unsigned int single_tasking,    /* Set if the model is single tasking,
                                         * even though there are more than one
                                         * sample time */
        struct ecs_handle **ecs_handle
        );

#ifdef __cplusplus
}
#endif

