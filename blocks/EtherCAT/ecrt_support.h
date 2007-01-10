#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
#endif

#include "ecdb.h"
#include "ecrt.h"

struct ecat_pdo;

void
ecs_send(int tid);
void
ecs_receive(int tid);
const char *
ecs_start(void);

const char *
ecs_reg_sdo( struct ecat_pdo *pdo,
        int sdo_type, uint16_t sdo_index,
        uint8_t sdo_subindex, uint32_t value);

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
        );
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
        );

void
ecs_end(void);

const char *
ecs_init( 
        unsigned int *st       /* Zero terminated list of sample times in 
                                 * microseconds */
        );
