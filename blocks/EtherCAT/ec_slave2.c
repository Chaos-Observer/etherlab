/*
 * $Id$
 *
 * This SFunction implements a generic EtherCAT Slave.
 *
 * Copyright (c) 2008, Richard Hacker
 * License: GPL
 *
 * Description:
 * This generic SFunction can be used to realise any EtherCAT slave that
 * is used in EtherLab. All the necessary information needed
 * to configure the slave, all the outputs and inputs, scaling,
 * filtering, etc. is specified via the parameters passed to the SFunction.
 *
 * There are 14 parameters. These are described below:
 *
 * 1: Master - numeric scalar value
 *   The master that this slave is connected to.
 *
 * 2: Domain - numeric scalar value
 *   The domain in which this slave will exchange its data in. All slaves
 *   with the same PDO direction having the same domain number in a
 *   sample time will exchange data together in one atomic unit.
 *
 * 3. Alias - numeric scalar value
 *   The zero'th position reference for this slave. Every slave is
 *   identified by its position in the network ring. Apart from that, some
 *   slaves have the capability to store an arbitrary number called alias
 *   permanently. This makes it possible to identify a slave no matter
 *   where it is located in the ring, under the premise that it is unique.
 *   Using this slave with the given alias as a reference, it is possible
 *   to address subsequent slaves relative to it.
 *   Specifying an alias greater than zero uses relative addressing
 *   to identify the particular slave.
 *
 * 4. Position - numeric scalar value
 *   Slave network position relative to an alias if alias is specified,
 *   otherwise relative to the first slave when alias = 0. The first
 *   EtherCAT Slave has position 0.
 *
 * 5. Product Name - string
 *   An arbitrary string that identifies a slave.
 *
 * 6. Vendor - numeric scalar value
 *   The EtherCAT vendor identifier assigned to the vendor
 *
 * 7. Product Code - numeric scalar value
 *   The EtherCAT product code of the slave
 *
 * 8. Slave Revision Number - numeric scalar value
 *   Software revision number of a slave. Increment this number if the
 *   firmware of a slave is revised.
 *   Sidenote: The vendor, product code and revision constructs a key
 *   that identifies a slave as well as its firmware state.
 *
 * 9. PDO Information - N-by-4 numeric matrix
 *   A matrix with 4 columns that is used to assign a set of PDO's to
 *   a sync manager. Only data of PDO's mapped in a sync manager is exchanged
 *   with the EtherCAT Master (TBC). The columns have the following functions:
 *    1 - Data direction as seen by the Master;
 *        1 = Input, i.e. data that travels from the slave to the master
 *        0 = Output, i.e. data that travels from the master to the slaves
 *    2 - PDO Index to map
 *    3 - Zero based row pointing to Pdo Entry Info where mapping starts
 *    4 - Number of Pdo Entry Info rows to map
 *
 * 10. Pdo Entry Info - N-by-4 numeric matrix
 *   A matrix of 3 columns describing the PDO entry information. It is used
 *   to configure the slave's PDO's. The columns have the following
 *   functions:
 *    1 - Index
 *    2 - Subindex
 *    3 - Bit length: number of bits that are mapped by the pdo entry
 *    4 - PDO Data type: one of 1, -8, 8, -16, 16, -32, 32 mapping to
 *        SS_BOOLEAN, SS_INT8, SS_UINT8, SS_INT16, SS_UINT16, 
 *        SS_INT32 and SS_UINT32
 *
 * 11. SDO Configuration - N-by-4 numeric matrix
 *   A matrix with 4 columns that is used to configure a slave. The
 *   configuration is transferred to the slave during initialisation.
 *   The columns have the following functions:
 *    1 - Index
 *    2 - Subindex
 *    3 - Data type: one of 8, 16, 32
 *    4 - Value
 *
 * 12. Block input port specification - structure vector
 *   This structure defines the Simulink's block inputs. The number of
 *   elements in this structure define the block's input count, i.e.
 *   one element for every input. The structure has the following fields:
 *   'pdo_map':         N-by-2 numeric matrix (required)
 *                      This matrix defines which PDO is mapped. The number
 *                      of rows (N) define the port width. The columns have
 *                      the following function:
 *                      1 - Zero based PDO info from argument 10 to map.
 *                          Only PDO Info with direction = 1 are allowed
 *                          for inputs.
 *                      2 - The offset of PDO Entry Information (argument 9)
 *                          to map. This value is added to the index pointed
 *                          to by column 3 of PDO Info to determine the
 *                          absolute row in PDO Entry Information to map.
 *
 *   'pdo_data_type':   scalar numeric value (optional)
 *                      data type of the pdo:
 *                          one of 1, 8, -8, 16, -16, 32, -32 mapping to
 *                          boolean_t, uint8_t, int8_t, uint16_t, int16_t,
 *                          uint32_t, int32_t
 *                      If this field is not specified, the data type is
 *                      derived from the number of bits specified in
 *                      parameter 9.
 *
 *   'pdo_full_scale':  scalar numeric value (optional)
 *                      If this field is not specified, the input port is
 *                      the "raw" PDO value. This also means that the port
 *                      has the same data type as specified in the data type
 *                      field of the PDO.
 *                      If this field specified, the input port value
 *                      is multiplied by this value before writing it to
 *                      the PDO, i.e.
 *                      'PDO value' = input * 'pdo_full_scale'
 *
 *                      Ideally this value is the maximum value
 *                      that the PDO can assume, thus an input range of
 *                      [-1.0 .. 1.0> for signed PDO's and [0 .. 1.0> for
 *                      unsigned PDO's is mapped to the full PDO value range.
 *
 *                      For example, a pdo_full_scale = 32768 would map the
 *                      input -1.0..1.0 to the entire value range of a int16_t
 *                      PDO data type
 *
 *                      Note that integer overflow and underflow is checked
 *                      before the value is written to the PDO, thus avoiding
 *                      "wrapping" effects
 *
 *   'gain':            numeric vector (optional)
 *                      An optional vector or scalar. If it is a vector,
 *                      it must have the same number of elements as there
 *                      are rows in 'pdo_map'. This field is only considered
 *                      when 'pdo_full_scale' is specified.
 *                      If this field is specified, the value written to
 *                      the PDO is premultiplied before assignment.
 *                      i.e.
 *                      'PDO value' = 'gain' .* input * 'pdo_full_scale'
 *
 *                      Note that integer overflow and underflow is checked
 *                      before the value is written to the PDO, thus avoiding
 *                      "wrapping" effects
 *
 *   'gain_name':       string (optional)
 *                      This optional field is only considered if 'gain' is
 *                      specified.
 *                      If this field is specified, the 'gain' values will
 *                      appear as an application parameter with this name.
 *                      Thus it is possible to modify it while the model
 *                      is running. Note that the names must all be unique
 *                      for a block
 *
 *   Any other fields are ignored.
 *
 * 13. Block output port specification - structure vector
 *   This structure defines the Simulink's block outputs. The number of
 *   elements in this structure define the block's output count, i.e.
 *   one element for every output. The structure has the following fields:
 *   'pdo_map':         N-by-2 numeric matrix (required)
 *                      This matrix defines which PDO is mapped. The number
 *                      of rows (N) define the port width. The columns have
 *                      the following function:
 *                      1 - Zero based PDO info from argument 10 to map.
 *                          Only PDO Info with direction = 0 are allowed
 *                          for outputs.
 *                      2 - The offset of PDO Entry Information (argument 9)
 *                          to map. This value is added to the index pointed
 *                          to by column 3 of PDO Info to determine the
 *                          absolute row in PDO Entry Information to map.
 *
 *   'pdo_full_scale':  scalar numeric value (optional)
 *                      If this field is not specified, the output port is
 *                      the "raw" PDO value. This also means that the port
 *                      has the same data type as specified in the data type
 *                      of the PDO.
 *                      If this field specified, the output port has data
 *                      type 'double_T'. The PDO value is then divided by
 *                      this value and written to the output port, i.e.
 *                      output = 'PDO value' / 'pdo_full_scale'
 *
 *                      Ideally this value is the maximum value that the
 *                      PDO can assume, thus mapping the PDO value range to
 *                      [0 .. 1.0> for unsigned PDO data types and
 *                      [-1.0 .. 1.0> for signed PDO data types.
 *
 *                      For example, a pdo_full_scale = 32768 would map the
 *                      entire value range of a int16_t PDO data type
 *                      from -1.0 to 1.0
 *
 *   'gain':            numeric vector (optional)
 *                      An optional vector or scalar. If it is a vector,
 *                      it must have the same number of elements as there
 *                      are rows in 'pdo_map'. This field is only considered
 *                      when 'pdo_full_scale' is specified.
 *                      If this field is specified, the PDO value is further
 *                      multiplied by it after applying 'pdo_full_scale'.
 *                      i.e.
 *                      output = 'gain' .* 'PDO value' / 'pdo_full_scale'
 *
 *   'gain_name':       string (optional)
 *                      This optional field is only considered if 'gain' is
 *                      specified.
 *                      If this field is specified, the 'gain' values will
 *                      appear as an application parameter with this name.
 *                      Thus it is possible to modify it while the model
 *                      is running. Note that the names must all be unique
 *                      for a block
 *
 *   'offset':          numeric vector (optional)
 *                      An optional vector or scalar. If it is a vector,
 *                      it must have the same number of elements as there
 *                      are rows in 'pdo_map'. This field is only considered
 *                      when 'pdo_full_scale' is specified.
 *                      If this field is specified, it is added to the result
 *                      of applying 'pdo_full_scale' and 'gain',
 *                      i.e.
 *                      output = 'gain' .* 'PDO value' / 'pdo_full_scale'
 *                               .+ offset
 *
 *   'offset_name':     string (optional)
 *                      This optional field is only considered if 'offset' is
 *                      specified.
 *                      See 'gain_name' for an explanation
 *
 *   'filter':          numeric vector (optional)
 *                      An optional vector or scalar. If it is a vector,
 *                      it must have the same number of elements as there
 *                      are rows in 'pdo_map'. This field is only considered
 *                      when 'pdo_full_scale' is specified.
 *                      If this field is specified, the output value is low
 *                      pass filtered before written to the output port and
 *                      after applying possible 'gain' and 'offset' values.
 *                      The value specified here is used as the "loop gain"
 *                      for the low pass filter. Depending on the sample
 *                      time specified for this block, either a continuous
 *                      (sample time = 0.0) or a discrete (sample time > 0.0)
 *                      filter is used. The filter equations are the
 *                      following:
 *                      Continuous filter:
 *                      y' = k * (u - y)
 *                      where:
 *                              y' is the output derivative
 *                              y is the output value
 *                              u is the filter's input
 *                              k is the value of 'filter'
 *
 *                      Discrete filter:
 *                      y(n) = k * u(n) + (1 - k) * y(n-1)
 *                      where:
 *                              y(n) is the filter state at sample n
 *                              u(n) is the filter's input
 *                              k    is the input weight
 *
 *                      In both cases, specifying a larger value for 'filter'
 *                      results in a filter with a higher cutoff frequency.
 *
 *                      NOTE: specifying values of k where k * Ts >= 1.0
 *                      (Ts is the sample time) will result in an unstable
 *                      filter! This value is NOT checked!
 *
 *   'filter_name':     string (optional)
 *                      This optional field is only considered if 'filter' is
 *                      specified.
 *                      See 'gain_name' for an explanation
 *
 *   Any other fields are ignored.
 *
 * 14. Sample Time: numeric scalar value.
 *   The sample time with which this block is executed. Specifying 0.0
 *   executes it at the fastest sample time specified in
 *   Configuration Parameters -> Solver. On the other hand, if an output
 *   filter is specified for any output, you have to select a Solver in
 *   to do the required integration of the continuous filter.
 *
 *   Specifying a value > 0 will execute this block at the specified time
 *   intervals. If an output filter is specified, a discrete filter is
 *   chosen, thus it is not required to select one ODE solvers.
 *
 */

#define S_FUNCTION_NAME  ec_slave2
#define S_FUNCTION_LEVEL 2

#include "simstruc.h"

#include <stdint.h>

#define ADDRESS                                            0
#define ETHERCAT_INFO                                      1
#define LAYOUT_VERSION        mxGetScalar(ssGetSFcnParam(S,2))
#define SDO_CONFIG                                         3
#define INPUT                                              4
#define OUTPUT                                             5
#define TSAMPLE               mxGetScalar(ssGetSFcnParam(S,6))
#define PARAM_COUNT                                        7

char errmsg[256];

#define abs(x) ((x) >= 0 ? (x) : -(x))
#define max(x,y) ((x) >= (y) ? (x) : (y))

struct ecat_slave {

    uint_T master;
    uint_T domain;
    uint_T alias;
    uint_T position;

    /* EtherCAT Info */
    uint32_T vendor_id;
    uint32_T revision_no;
    uint32_T product_code;
    char_T *type;

    /* Software layout of the slave. Slaves with the key 
     * (vendor_id,product_code,layout_version) share the same EtherCAT Info
     * data in the generated RTW code, meaning that only one data set 
     * for every unique key exists. */
    uint32_T layout_version;

    struct sdo_config {
        uint16_T index;
        uint8_T subindex;

        /* Data type. One of SS_UINT8, SS_UINT16, SS_UINT32 */
        uint_T datatype;

        /* Configuration value */
        uint_T value;
    } *sdo_config;
    uint_T sdo_config_count;

    struct sync_manager {
        uint16_t index;
        enum {EC_DIR_INPUT, EC_DIR_OUTPUT} direction;
        struct pdo {
            uint16_t index;
            struct pdo_entry {
                uint16_t index;
                uint8_t subindex;
                uint_T bitlen;
                int_T datatype;
            } *entry;
            unsigned int entry_count;
        } *pdo;
        unsigned int pdo_count;
    } *sync_manager;
    uint_T sync_manager_count;

    /* Maximum port width of both input and output ports. This is a 
     * temporary value that is used to check whether the Simulink block
     * has mixed port widths */
    uint_T max_port_width;

    /* Set if the Simulink block has mixed port widths */
    boolean_T mixed_port_widths;

    struct io_port {

        struct mapped_pdo {
            struct pdo *pdo;
            struct pdo_entry *pdo_entry;
        } *mapped_pdo;
        uint_T mapped_pdo_count;

        /* The length of the significant bits in the pdo entry */
        uint32_T data_bit_len;

        /* The data type of the PDO. 
         * Either set directly using the field 'PdoDataType' if specified,
         * otherwise use pdo_entry->datatype.
         * Only the values SS_UINT8, SS_INT8, SS_UINT16, SS_INT16, 
         * SS_UINT32, SS_INT32 are allowed.
         * This value is used in the real-time code to cast a void* */
        uint_T pdo_data_type;

        /* Data type of the IO port 
         * If 'PdoFullScale' is specified, it is SS_DOUBLE, otherwise 
         * a data type is chosen that suits data_bit_len */
        uint_T data_type;

        /* Number of signals for the port
         * This is also the number of *map entries,
         * i.e. one PDO for every signal.
         */
        uint_T width;

        /* Set if bitshifting is necessary for this port */
        boolean_T bitop;

        /* The following specs are needed to determine whether the
         * input/output value needs to be scaled before writing to
         * the PDO. 
         *
         * If 'raw' is set, following values are ignored.
         *
         * If raw is unset, the value written to / read from the PDO
         * is scaled using gain and/or offset; see introduction concerning
         * pdo_full_scale
         *
         * gain_values appears as an application parameter if supplied.
         */
        real_T pdo_full_scale;

        int_T  gain_count;
        char_T *gain_name;
        real_T *gain_values;

        int_T  offset_count;
        char_T *offset_name;
        real_T *offset_values;

        int_T  filter_count;
        char_T *filter_name;
        real_T *filter_values;

    } *output_port, *input_port;

    /* Number of input and output ports */
    uint_T input_port_count;
    uint_T output_port_count;

    uint_T iwork_count;
    uint_T pwork_count;
    uint_T filter_count;
    uint_T runtime_param_count;
};


int_T
get_data_type(SimStruct *S, const char_T *variable, const char_T *field,
        int_T width, uint_T *data_type, boolean_T only_aligned, 
        boolean_T only_unsigned)
{
    const char_T *reason = "Data type specified is not byte aligned";

    if (!width) {
        reason =  "Error: Data type with zero width not allowed.";
        goto out;
    }
    else if (width < 0) {
        if (only_unsigned) {
            reason = "Only unsigned data types allowed";
            goto out;
        }
        switch (width) {
            case -8:
                *data_type = SS_INT8;
                break;
            case -16:
                *data_type = SS_INT16;
                break;
            case -32:
                *data_type = SS_INT32;
                break;
            default:
                reason = "Error: Non byte-aligned signed data type; "
                    "Allowed are only -8, -16 or -32.";
                goto out;
                break;
        }
    }
    else if (width > 32) {
        reason = "Error: Unsigned data type must be in the range [1..32].";
        goto out;
    }
    else if (width > 16) {
        if (width != 32 && only_aligned) {
            /* Leave pointer reason */
            goto out;
        }
        *data_type = SS_UINT32;
    }
    else if (width > 8) {
        if (width != 16 && only_aligned) {
            /* Leave pointer reason */
            goto out;
        }
        *data_type = SS_UINT16;
    }
    else if (width > 1) {
        if (width != 8 && only_aligned) {
            /* Leave pointer reason */
            goto out;
        }
        *data_type = SS_UINT8;
    } else {
        *data_type = SS_BOOLEAN;
    }

    return 0;

out:
    snprintf(errmsg, sizeof(errmsg),
            "SFunction parameter %s.%s specified invalid data type %i: %s.",
            variable, field, width, reason);
    ssSetErrorStatus(S,errmsg);
    return -1;
}

#if 0

const char_T*
get_parameter_values(SimStruct* S, const mxArray* output, uint_T port,
        const char_T* field, int_T *count, uint_T port_width,
        real_T **values )
{
    real_T *value;
    uint_T i;
    const mxArray* src = mxGetField(output, port, field);

    if (!src)
        return NULL;

    *count = mxGetNumberOfElements(src);
    if (!*count)
        return field;

    if (!mxIsDouble(src))
        return field;
    value = mxGetPr(src);
    if (!value)
        return field;
    if (*count != 1 && *count != port_width)
        return field;
    *values = mxCalloc(*count, sizeof(real_T));
    if (!*values)
        return ssGetErrorStatus(S);
    for (i = 0; i < *count; i++)
        (*values)[i] = value[i];

    return NULL;
}

const char_T*
get_parameter_name(SimStruct* S, const mxArray* output, uint_T port,
        const char_T* field, char_T **name)
{
    uint_T len;
    const mxArray* plhs;

    plhs = mxGetField(output, port, field);
    if (!plhs)
        return NULL;

    len = mxGetN(plhs) + 1;
    *name = mxMalloc(len);
    if (!*name)
        return ssGetErrorStatus(S);

    if (mxGetString(plhs, *name, len))
        return field;

    return NULL;
}

const char_T*
get_ioport_config(SimStruct *S, const mxArray* src, struct io_spec *io_spec,
        uint_T *total_map_count, uint_T *iwork_count,
        const struct ecat_slave *slave, uint_T port, uint_T port_dir)
{
    const mxArray* pdo_map = mxGetField(src, port, "pdo_map");
    const mxArray* pdo_full_scale = mxGetField(src, port, "pdo_full_scale");
    const char_T* errfield = NULL;
    const char_T* err;
    const char_T* dir_str = port_dir ? "input" : "output";
    real_T *pdo_info_idx;
    real_T *pdo_entry_offset;
    uint_T sig_idx;
    uint_T pdo_entry_idx;
    uint_T port_bit_len;

    /* Check that the required fields for port are specified */
    if (!pdo_map) {
        snprintf(errmsg, sizeof(errmsg),
                "Required field %s(%u).pdo_map not specified.",
                dir_str, port+1);
        return errmsg;
    }

    /* Make sure matrices have the right dimensions */
    io_spec->map_count = mxGetM(pdo_map);
    if (mxGetN(pdo_map) != 2 || !mxIsDouble(pdo_map) 
            || !io_spec->map_count) {
        /* PDO map must have 2 columns */
        snprintf(errmsg, sizeof(errmsg),
                "Matrix specified for %s(%u).%s must be a numeric array "
                "with 2 columns.",
                dir_str, port+1, "pdo_map");
        return errmsg;
    }

    /* The first guess of port width is the number of mapped pdo's. This
     * might change later on */
    io_spec->port_width = io_spec->map_count;

    pdo_info_idx = mxGetPr(pdo_map);
    pdo_entry_offset = pdo_info_idx + io_spec->map_count;

    io_spec->map = mxCalloc(io_spec->map_count, sizeof(*io_spec->map));
    if (!io_spec->map)
        return ssGetErrorStatus(S);
    *total_map_count += io_spec->map_count;

    /* Check the ranges of pdo_info_idx and pdo_entry_offset, and return
     * the real pdo_entry_idx */
    err = get_pdo_entry_idx(&pdo_entry_idx, slave,
            pdo_info_idx[0], pdo_entry_offset[0]);
    if (err) {
        snprintf(errmsg, sizeof(errmsg),
                "error occurred while determinig pdo entry index "
                "of pdo[0] for %s(%u): %s", dir_str, port+1, err);
        return errmsg;
    }

    /* Find out the pdo bit length for this port. It is specified by
     * the first element of pdo_entry_idx for this port */
    port_bit_len = slave->pdo_entry_info[pdo_entry_idx].bitlen;

    /* Find out the pdo bit length for this port. It is specified by
     * the first element of pdo_entry_idx for this port */
    io_spec->pdo_data_type = slave->pdo_entry_info[pdo_entry_idx].datatype;

    io_spec->raw = pdo_full_scale ? 0 : 1;

    if (slave->pdo_entry_info[pdo_entry_idx].bitmask) {
        if (!io_spec->raw) {
            snprintf(errmsg, sizeof(errmsg),
                    "Boolean signals cannot be scaled for %s(%u).",
                    dir_str, port+1);
            return errmsg;
        }

        *iwork_count += io_spec->map_count;
        io_spec->bitmask = slave->pdo_entry_info[pdo_entry_idx].bitmask;
    }

    if (slave->pdo_entry_info[pdo_entry_idx].count > 1) {
        if (io_spec->map_count > 1) {
            snprintf(errmsg, sizeof(errmsg),
                    "PDO Entry %u on %s port %u is a vector.\n"
                    "When PDO Entry is a vector, only allowed to map "
                    "1 PDO per port.",
                    io_spec->map_count,
                    dir_str, port+1);
            return errmsg;
        }
        io_spec->port_width = slave->pdo_entry_info[pdo_entry_idx].count;
    }

    for (sig_idx = 0; sig_idx < io_spec->map_count; sig_idx++) {
        /* Check the ranges of pdo_info_idx and pdo_entry_offset, and return
         * the real pdo_entry_idx */
        err = get_pdo_entry_idx(&pdo_entry_idx, slave,
                pdo_info_idx[sig_idx], pdo_entry_offset[sig_idx]);
        if (err) {
            snprintf(errmsg, sizeof(errmsg),
                    "error occurred while determinig pdo entry index "
                    "of pdo[%u] for %s(%u): %s", 
                    sig_idx, dir_str, port+1, err);
            return errmsg;
        }

        /* Check that the pdo_bit_len of the mapped pdo's is the same
         * as that for the first one */
        if (slave->pdo_entry_info[pdo_entry_idx].bitlen != port_bit_len) {
            snprintf(errmsg, sizeof(errmsg),
                    "Cannot mix different PDO data types for "
                    "%s(%u).", dir_str, port+1);
            return errmsg;
        }

        /* Make sure that the port direction is correct */
        if (slave->pdo_info[(uint_T)pdo_info_idx[sig_idx]][0] != port_dir) {
            snprintf(errmsg, sizeof(errmsg),
                    "PDO direction has wrong direction for %s(%u)",
                    dir_str, port+1);
            return errmsg;
        }

        io_spec->map[sig_idx].pdo_info_idx = pdo_info_idx[sig_idx];
        io_spec->map[sig_idx].pdo_entry_idx = pdo_entry_offset[sig_idx];
    }

    if (io_spec->raw) {
        return NULL;
    }

    /* Output is double. This means that the source value must be scaled
     * so that the source value range is mapped to [0..1] */
    io_spec->pdo_full_scale = mxGetScalar(pdo_full_scale);
    if (!mxIsDouble(pdo_full_scale) || io_spec->pdo_full_scale <= 0.0) {
        snprintf(errmsg, sizeof(errmsg),
                "Specified field %s(%u).pdo_full_scale "
                "must be a positive numeric scalar.",
                dir_str, port+1);
        return errmsg;
    }

    /* Check that the vector length is either 1 or port_width
     * for the fields 'gain', 'offset' and 'filter' if they are specified */
    do {
        errfield = get_parameter_values(S, src, port, "gain",
            &io_spec->gain_count, io_spec->port_width,
            &io_spec->gain_values);
        if (errfield) break;

        /* Block inputs do not have offset and filter features */
        if (port_dir == 1) break;

        errfield = get_parameter_values(S, src, port, "offset",
            &io_spec->offset_count, io_spec->port_width,
            &io_spec->offset_values);
        if (errfield) break;

        errfield = get_parameter_values(S, src, port, "filter",
            &io_spec->filter_count, io_spec->port_width,
            &io_spec->filter_values);
        if (errfield) break;

    } while(0);
    if (errfield) {
        snprintf(errmsg, sizeof(errmsg),
                "Vector for %s(%u).%s is not a valid numeric array "
                "with 1 or %u elements.",
                dir_str, port+1, errfield, io_spec->port_width);
        return errmsg;
    }

    /* Get the names for the parameters */
    do {
        if (io_spec->gain_count) {
            errfield = get_parameter_name(S, src, port, "gain_name",
                    &io_spec->gain_name);
            /* FIXME: Remove the restriction that a gain_name must be
             * provided when a gain is specified */
            if (!io_spec->gain_name)
                errfield = "gain_name";
            if (errfield) break;
        }

        if (io_spec->offset_count) {
            errfield = get_parameter_name(S, src, port, "offset_name",
                    &io_spec->offset_name);
            /* FIXME: Remove the restriction that a offset_name must be
             * provided when a offset is specified */
            if (!io_spec->offset_name)
                errfield = "offset_name";
            if (errfield) break;
        }

        if (io_spec->filter_count) {
            errfield = get_parameter_name(S, src, port, "filter_name",
                    &io_spec->filter_name);
            /* FIXME: Remove the restriction that a filter_name must be
             * provided when a filter is specified */
            if (!io_spec->filter_name)
                errfield = "filter_name";
            if (errfield) break;
        }
    } while(0);
    if (errfield) {
        snprintf(errmsg, sizeof(errmsg),
                "No valid string for %s(%u).%s supplied.",
                dir_str, port+1, errfield);
        return errmsg;
    }


    return NULL;
}
#endif

int_T
err_no_zero_field(SimStruct *S, const char_T *variable, const char_T *field)
{
    snprintf(errmsg, sizeof(errmsg),
            "SFunction parameter %s field '%s' is not allowed to be zero.",
            variable, field);
    ssSetErrorStatus(S,errmsg);
    return -1;
}

int_T
err_no_empty_string_field(SimStruct *S, const char_T *variable, 
        const char_T *field)
{
    snprintf(errmsg, sizeof(errmsg),
            "SFunction parameter %s field '%s' is not allowed to be empty.",
            variable, field);
    ssSetErrorStatus(S,errmsg);
    return -1;
}

int_T
err_no_field(SimStruct *S, const char_T *variable, 
        const char_T *type, const char_T *field)
{
    snprintf(errmsg, sizeof(errmsg),
            "SFunction parameter %s "
            "does not have the required %s field '%s'.",
            variable, type, field);
    ssSetErrorStatus(S,errmsg);
    return -1;
}

int_T
err_no_num_field(SimStruct *S, const char_T *variable,
        const char_T *field)
{
    return err_no_field(S, variable, "numeric", field);
}

int_T
err_no_str_field(SimStruct *S, const char_T *variable,
        const char_T *field)
{
    return err_no_field(S, variable, "string", field);
}

int_T
get_numeric_field(SimStruct *S, const char_T *variable,
        const mxArray *src, uint_T idx, boolean_T zero_not_allowed,
        const char_T *field_name, real_T *dest)
{
    const mxArray *field = mxGetField(src, idx, field_name);

    *dest = 0.0;

    if (!field) {
        err_no_num_field(S, variable, field_name);
        return 1;
    }
    if (!mxIsNumeric(field)) {
        err_no_num_field(S, variable, field_name);
        return 2;
    }

    *dest = mxGetScalar(field);

    if (zero_not_allowed && !*dest) {
        err_no_zero_field(S, variable, field_name);
        return 3;
    }

    return 0;
}

#define CHECK_CALLOC(S,n,m,dest) \
    do {                                                                \
        if (!(n))                                                       \
                break;                                                  \
        dest = mxCalloc((n),(m));                                       \
        if (!(dest)) {                                                  \
            ssSetErrorStatus((S), "calloc() failure; no memory left."); \
            return -1;                                                  \
        }                                                               \
    } while(0)

int_T
get_string_field(SimStruct *S, const char_T *variable,
        const mxArray *src, uint_T idx, boolean_T zero_not_allowed,
        const char_T *field_name, char_T **dest)
{
    const mxArray *field = mxGetField(src, idx, field_name);
    uint_T len;

    if (!field || !mxIsChar(field)) {
        return err_no_str_field(S, variable, field_name);
    }

    len = mxGetN(field);

    if (!len) {
        *dest = NULL;

        return zero_not_allowed
            ? err_no_empty_string_field(S, variable, field_name)
            : 0;
    }

    CHECK_CALLOC(S,len,1,*dest);

    if (!mxGetString(field, *dest, len)) {
        return err_no_str_field(S, variable, field_name);
    }

    return 0;
}

#define RETURN_ON_ERROR(val)    \
    do {                        \
        int_T err = val;        \
        if (err)                \
            return err;         \
    } while(0)

int_T
get_slave_address(SimStruct *S, struct ecat_slave *slave)
{
    const mxArray *address       = ssGetSFcnParam(S,ADDRESS);
    real_T val;

    RETURN_ON_ERROR(get_numeric_field(S, ADDRESS, address, 0, 0,
                "Master", &val));
    slave->master = val;
    RETURN_ON_ERROR(get_numeric_field(S, ADDRESS, address, 0, 0,
                "Domain", &val));
    slave->domain = val;
    RETURN_ON_ERROR(get_numeric_field(S, ADDRESS, address, 0, 0,
                "Alias", &val));
    slave->alias = val;
    RETURN_ON_ERROR(get_numeric_field(S, ADDRESS, address, 0, 0,
                "Position", &val));
    slave->position = val;
    return 0;
}

int_T
get_ethercat_info(SimStruct *S, struct ecat_slave *slave)
{
    const mxArray *ec_info = ssGetSFcnParam(S,ETHERCAT_INFO);
    const mxArray *sm_field;
    const char_T *param = "ETHERCAT_INFO";
    struct sync_manager *sm;
    char_T variable[100];
    real_T val;
    uint_T i;

    RETURN_ON_ERROR(get_numeric_field(S, param, ec_info, 0, 1,
                "VendorId", &val));
    slave->vendor_id = val;
    RETURN_ON_ERROR(get_numeric_field(S, param, ec_info, 0, 0,
                "RevisionNo", &val));
    slave->revision_no = val;
    RETURN_ON_ERROR(get_numeric_field(S, param, ec_info, 0, 1,
                "ProductCode", &val));
    slave->product_code = val;
    RETURN_ON_ERROR(get_string_field(S, param, ec_info, 0, 0,
                "Type", &slave->type));

    sm_field = mxGetField(ec_info, 0, "SyncManager");
    if (!sm_field)
        return 0;
    printf("%u %u\n", mxGetM(sm_field), mxGetN(sm_field));
    
    /* Count the number of fields */
    slave->sync_manager_count = mxGetNumberOfElements(sm_field);
    CHECK_CALLOC(S,slave->sync_manager_count, sizeof(struct sync_manager),
            slave->sync_manager);
    printf("Found SyncManager Count %u\n",slave->sync_manager_count);

    for (sm = slave->sync_manager; 
            sm != &slave->sync_manager[slave->sync_manager_count]; 
            sm++) 
        sm->index = ~sm->index;

    for (i = 0; i < slave->sync_manager_count; i++) {
        const mxArray *smX = mxGetFieldByNumber(sm_field, 0, i);
        const mxArray *pdoX;
        struct pdo *pdo;
        uint_T sm_index;
        uint_T pdo_idx;

        snprintf(variable, sizeof(variable), 
                "%s.SyncManager(%u)", param, i+1);
        printf("%s\n", variable);

        if (!smX) {
            snprintf(errmsg, sizeof(errmsg),
                    "Matlab internal error: SFunction parameter %s "
                    "does not exist.", variable);
            ssSetErrorStatus(S,errmsg);
            return -1;
        }

        RETURN_ON_ERROR(get_numeric_field(S, variable, smX, 0, 0,
                    "SmIndex", &val));
        sm_index = val;

        sm = slave->sync_manager; 
        do {
            if (sm == &slave->sync_manager[slave->sync_manager_count]) {
                ssSetErrorStatus(S,"FIXME");
                return -1;
            }
            if (sm->index == sm_index || sm->index == (uint16_T)~0U)
                break;
        } while(sm++);
        sm->index = sm_index;
        printf("found sm %u\n", sm_index);

        RETURN_ON_ERROR(get_numeric_field(S, variable, smX, 0, 0,
                    "SmDir", &val));
        sm->direction = val ? EC_DIR_INPUT : EC_DIR_OUTPUT;

        pdoX = mxGetField(smX, 0, "Pdo");

        if (!pdoX || !(sm->pdo_count = mxGetNumberOfElements(pdoX)))
            continue;
        CHECK_CALLOC(S, sm->pdo_count, sizeof(struct pdo), sm->pdo);

        for (pdo_idx = 0, pdo = sm->pdo; 
                pdo_idx < sm->pdo_count; pdo_idx++, pdo++) {
            const mxArray *pdo_entry = mxGetField(pdoX, pdo_idx, "Entry");
            uint_T entry_idx;
            struct pdo_entry *entry;

            snprintf(variable, sizeof(variable), 
                    "%s.SyncManager(%u).Pdo(%u)", param, i+1, pdo_idx+1);
            printf("%s\n", variable);

            RETURN_ON_ERROR(get_numeric_field(S, variable,  pdoX, 
                        pdo_idx, 1, "Index", &val));
            pdo->index = val;

            if (!pdo_entry || 
                    !(pdo->entry_count = mxGetNumberOfElements(pdo_entry)))
                continue;
            CHECK_CALLOC(S, pdo->entry_count, sizeof(struct pdo_entry),
                    pdo->entry);

            for (entry_idx = 0, entry = pdo->entry; 
                    entry_idx < pdo->entry_count; entry_idx++, entry++) {
                uint_T dummy_datatype;

                snprintf(variable, sizeof(variable), 
                        "%s.SyncManager(%u).Pdo(%u).Entry(%u)", 
                        param, i+1, pdo_idx+1, entry_idx+1);
                printf("%s\n", variable);

                RETURN_ON_ERROR(get_numeric_field(S, variable,
                            pdo_entry, entry_idx, 1, "BitLen", &val));
                entry->bitlen = val;

                RETURN_ON_ERROR(get_numeric_field(S, variable,
                            pdo_entry, entry_idx, 0, "Index", &val));
                entry->index = val;
                printf("\t\tPdo Entry Index #x%04x\n", entry->index);
                printf("\t\t\tBitlen %u\n", entry->bitlen);

                if (!entry->index)
                    continue;

                RETURN_ON_ERROR(get_numeric_field(S, variable,
                            pdo_entry, entry_idx, 1, "SubIndex", &val));
                entry->subindex = val;
                printf("\t\t\tSubIndex %u\n", entry->subindex);

                switch(get_numeric_field(S, variable,
                            pdo_entry, entry_idx, 0, "DataType", &val)) {
                    case 0:
                        entry->datatype = val;
                        break;
                    case 1:
                        entry->datatype = entry->bitlen;
                    default:
                        return -1;
                }
                RETURN_ON_ERROR(get_data_type(S, variable, "DataType",
                            entry->datatype, &dummy_datatype, 0, 0));
                printf("\t\t\tDataType %u %i\n", 
                        dummy_datatype, entry->datatype);
            }
        }
    }

    return 0;
}

int_T
get_sdo_config(SimStruct *S, struct ecat_slave *slave)
{
    const mxArray *sdo_config = ssGetSFcnParam(S,SDO_CONFIG);
    struct sdo_config *sdo_config_ptr;
    const char_T *param = "SDO_CONFIG";
    uint_T i;

    slave->sdo_config_count = mxGetM(sdo_config);

    CHECK_CALLOC(S,slave->sdo_config_count, sizeof(*slave->sdo_config),
            slave->sdo_config);

    for (i = 0, sdo_config_ptr = slave->sdo_config; 
            i < slave->sdo_config_count; i++, sdo_config_ptr++) {
        real_T val;

        RETURN_ON_ERROR(get_numeric_field(S, param,
                    sdo_config, i, 1, "Index", &val));
        sdo_config_ptr->index = val;

        RETURN_ON_ERROR(get_numeric_field(S, param,
                    sdo_config, i, 1, "SubIndex", &val));
        sdo_config_ptr->subindex = val;

        RETURN_ON_ERROR(get_numeric_field(S, param,
                    sdo_config, i, 1, "BitLen", &val));
        RETURN_ON_ERROR(get_data_type(S, param, "BitLen",
                    val, &sdo_config_ptr->datatype, 1, 1));

        RETURN_ON_ERROR(get_numeric_field(S, param,
                    sdo_config, i, 0, "Value", &val));
        sdo_config_ptr->value = val;
    }

    return 0;
}

int_T
find_pdo_entry(const struct ecat_slave *slave, uint16_T pdo_index,
        uint16_T pdo_entry_index, uint8_T pdo_entry_subindex, 
        uint_T *sm_direction, struct pdo **pdo, 
        struct pdo_entry **pdo_entry) 
{
    const struct sync_manager *sm;

    for (sm = slave->sync_manager; 
            sm != &slave->sync_manager[slave->sync_manager_count];
            sm++) {
        *sm_direction = sm->direction == EC_DIR_INPUT ? 1 : 0;
        for (*pdo = sm->pdo; *pdo != &sm->pdo[sm->pdo_count]; (*pdo)++) {
            if ((*pdo)->index != pdo_index)
                continue;
            for (*pdo_entry = (*pdo)->entry; 
                    *pdo_entry != &(*pdo)->entry[(*pdo)->entry_count]; 
                    (*pdo_entry)++) {
                if ((*pdo_entry)->index == pdo_entry_index
                        && (*pdo_entry)->subindex == pdo_entry_subindex)
                    return 0;
            }
        }
    }
    return -1;
}

int_T
get_ioport_config(SimStruct *S, struct ecat_slave *slave, 
        struct io_port **port_ptr, uint_T *port_count)
{
    const char_T * const param = 
        port_ptr == &slave->input_port ? "INPUT" : "OUTPUT";
    const mxArray * const io_port_spec =
        ssGetSFcnParam(S,port_ptr == &slave->input_port ? INPUT : OUTPUT);
    const uint_T port_direction = 
        port_ptr == &slave->input_port ? EC_DIR_INPUT : EC_DIR_OUTPUT;

    struct io_port *port;
    char_T variable[100];
    real_T val;
    uint_T port_idx = 0;

    if (port_ptr != &slave->output_port 
            && port_ptr != &slave->input_port) {
        ssSetErrorStatus(S,
                "Internal error: Invalid function argument supplied for io."
                );
        return -1;
    }

    if (!io_port_spec || !(*port_count = mxGetNumberOfElements(io_port_spec)))
        return 0;
    CHECK_CALLOC(S, *port_count, sizeof(struct io_port), *port_ptr);
    printf("%s %u\n", param, *port_count);

    for (port = *port_ptr; port != &(*port_ptr)[*port_count]; 
            port++, port_idx++) {
        const mxArray *mapped_pdo_spec = 
            mxGetField(io_port_spec, port_idx, "MappedPdo");
        struct mapped_pdo *mapped_pdo;
        uint_T map_idx;
        boolean_T struct_access;
        real_T *pdo_index_field;
        real_T *pdo_entry_index_field;
        real_T *pdo_entry_subindex_field;
        uint_T pdo_data_type;

        snprintf(variable, sizeof(variable), 
                "%s(%u).MappedPdo", param, port_idx+1);

        if (!mapped_pdo_spec) {
            snprintf(errmsg, sizeof(errmsg),
                    "SFunction parameter %s does not exist.", variable);
            ssSetErrorStatus(S,errmsg);
            return -1;
        }

        /* The field 'MappedPdo' is allowed to be a struct or a matrix.
         * Check what was specified */
        if (mxIsStruct(mapped_pdo_spec)) {
            /* Mapped pdos is defined using a structure array with
             * the fields 'Index', 'EntryIndex' and 'EntrySubIndex' */
            struct_access = 1;

            port->mapped_pdo_count = mxGetNumberOfElements(mapped_pdo_spec);
            if (!port->mapped_pdo_count) {
                snprintf(errmsg, sizeof(errmsg),
                        "SFunction parameter %s is empty.", variable);
                ssSetErrorStatus(S,errmsg);
                return -1;
            }

            /* Stop the compiler from complaining */
            pdo_index_field = pdo_entry_index_field = 
                pdo_entry_subindex_field = NULL;
        }
        else {
            /* Mapped pdos is defined using a M-by-3 array, where
             * the columns define the 'Index', 'EntryIndex' and 
             * 'EntrySubIndex' */
            struct_access = 0;

            port->mapped_pdo_count = mxGetM(mapped_pdo_spec);

            pdo_index_field = mxGetPr(mapped_pdo_spec);
            pdo_entry_index_field = 
                pdo_index_field + port->mapped_pdo_count;
            pdo_entry_subindex_field = 
                pdo_entry_index_field + port->mapped_pdo_count;

            if ( !mxIsNumeric(mapped_pdo_spec) 
                    || !port->mapped_pdo_count
                    || mxGetN(mapped_pdo_spec) != 3 
                    || !pdo_index_field) {
                snprintf(errmsg, sizeof(errmsg),
                        "SFunction parameter %s is not a M-by-3 "
                        "numeric array.", variable);
                ssSetErrorStatus(S,errmsg);
                return -1;
            }
        }
        CHECK_CALLOC(S, port->mapped_pdo_count, sizeof(struct mapped_pdo),
                port->mapped_pdo);

        snprintf(variable, sizeof(variable), "%s(%u)", param, port_idx+1);

        /* Get the list of pdo entries that are mapped to this port */
        for (map_idx = 0, mapped_pdo = port->mapped_pdo; 
                map_idx < port->mapped_pdo_count; map_idx++, mapped_pdo++) {
            uint16_T pdo_index, pdo_entry_index;
            uint8_T pdo_entry_subindex;
            uint_T sm_direction;
            char_T pdo_variable[100];

            snprintf(pdo_variable, sizeof(pdo_variable), 
                    "%s(%u).MappedPdo(%u)", param, port_idx+1, map_idx+1);

            if (struct_access) {
                RETURN_ON_ERROR(get_numeric_field(S, pdo_variable, 
                            mapped_pdo_spec, map_idx, 1, "PdoIndex", &val));
                pdo_index = val;

                RETURN_ON_ERROR(get_numeric_field(S, pdo_variable, 
                            mapped_pdo_spec, map_idx, 1, "PdoEntryIndex", 
                            &val));
                pdo_entry_index = val;

                RETURN_ON_ERROR(get_numeric_field(S, pdo_variable, 
                            mapped_pdo_spec, map_idx, 1, "PdoEntrySubIndex", 
                            &val));
                pdo_entry_subindex = val;
            }
            else {
                pdo_index          = *pdo_index_field++;
                pdo_entry_index    = *pdo_entry_index_field++;
                pdo_entry_subindex = *pdo_entry_subindex_field++;
            }

            /* Get the pdo and pdo entry pointers for the specified 
             * indices */
            if (find_pdo_entry(slave, 
                        pdo_index, pdo_entry_index, pdo_entry_subindex, 
                        &sm_direction, &mapped_pdo->pdo, 
                        &mapped_pdo->pdo_entry)) {
                snprintf(errmsg, sizeof(errmsg),
                        "SFunction parameter %s cannot be found.\n"
                        "Pdo Index = 0x%04X, Pdo Entry Index = 0x%04X, "
                        "Pdo Entry SubIndex = %u.",
                        pdo_variable,
                        pdo_index, pdo_entry_index, pdo_entry_subindex);
                ssSetErrorStatus(S,errmsg);
                return -1;
            }

            /* Check that the direction as specified in the sync manager
             * matches that of the port */
            if (port_direction != sm_direction) {
                snprintf(errmsg, sizeof(errmsg),
                        "The PDO specified by SFunction parameter %s "
                        "is an %s.\n"
                        "Cannot map this to an %s port.",
                        pdo_variable,
                        sm_direction == EC_DIR_INPUT ? "input" : "output",
                        param);
                ssSetErrorStatus(S,errmsg);
                return -1;
            }

            if (port->mapped_pdo[0].pdo_entry->datatype !=
                    mapped_pdo->pdo_entry->datatype) {
                ssSetErrorStatus(S,"Datatypes mixed FIXME");
                return -1;
            }

            if (!port->data_bit_len) {
                /* data_bit_len was not specified in the port definition.
                 * Set this to the data type specified in the Pdo Entry */
                port->data_bit_len = abs(mapped_pdo->pdo_entry->datatype);
            }

            if (mapped_pdo->pdo_entry->bitlen % port->data_bit_len) {
                ssSetErrorStatus(S,"Remainder FIXME");
                return -1;
            }

            port->width += 
                mapped_pdo->pdo_entry->bitlen / port->data_bit_len;
        }

        /* Make sure that the PDO has a supported data type */
        RETURN_ON_ERROR(get_data_type(S, variable, "MappedPdo(1)",
                    port->mapped_pdo[0].pdo_entry->datatype,
                    &pdo_data_type, 0, 0));

        /* If PdoDataType is specified, use it, otherwise it is guessed
         * by the DataBitLen spec below */
        switch((map_idx = get_numeric_field(S, variable, mapped_pdo_spec,
                    0, 0, "PdoDataType", &val))) {
            case 0:
                if (port->data_bit_len > (uint_T)val) {
                    ssSetErrorStatus(S, 
                            "Specified PdoDataType too small FIXME");
                    return -1;
                }

                RETURN_ON_ERROR(get_data_type(S, variable, "PdoDataType",
                            val, &port->pdo_data_type, 1, 0));
                break;
            case 1:
                /* Field was not available - clear the error */
                ssSetErrorStatus(S,NULL);

                /* Use the data type found above */
                port->pdo_data_type = pdo_data_type;
                break;
            default:
                return -1;
        }

        /* If PdoFullScale is specified, the port data type is double.
         * If port->pdo_full_scale == 0.0, it means that the port data
         * type is specified by PortBitLen */
        switch(get_numeric_field(S, variable, mapped_pdo_spec,
                    0, 1, "PdoFullScale", &val)) {
            case 0:
                port->pdo_full_scale = val;
                port->data_type = SS_DOUBLE;
                break;
            case 1:
                /* Field was not available - not an error */
                ssSetErrorStatus(S,NULL);

                /* Derive the data type from the data_bit_len */
                RETURN_ON_ERROR(get_data_type(S, variable, "PdoDataType",
                            port->data_bit_len, &port->data_type, 0, 0));
                break;
            default:
                return -1;
        }

        if (port->width > 1 && slave->max_port_width != port->width) {
            slave->mixed_port_widths = 1;
        }

        slave->max_port_width = max(port->width, slave->max_port_width);

        slave->pwork_count += port->width;
        if (port->data_bit_len % 8 || port->data_bit_len == 24) {
            slave->iwork_count += port->width;
            port->bitop = 1;
        }
    }
    return 0;
}

/* This function is used to operate on the allocated memory with a generic
 * operator. This function is used to fix or release allocated memory.
 */
void
slave_mem_op(struct ecat_slave *slave, void (*func)(void*))
{
    struct io_port *port;
    struct sync_manager *sm;

    (*func)(slave->type);
    (*func)(slave->sdo_config);

    for (sm = slave->sync_manager;
            sm != &slave->sync_manager[slave->sync_manager_count]; 
            sm++) {
        struct pdo *pdo;

        for (pdo = sm->pdo; pdo != &sm->pdo[sm->pdo_count]; pdo++)
            (*func)(pdo->entry);
        (*func)(sm->pdo);
    }
    (*func)(slave->sync_manager);

    for (port = slave->output_port;
            port != &slave->output_port[slave->output_port_count];
            port++) {
        (*func)(port->mapped_pdo);
        (*func)(port->gain_values);
        (*func)(port->gain_name);
        (*func)(port->offset_values);
        (*func)(port->offset_name);
        (*func)(port->filter_values);
        (*func)(port->filter_name);
    }
    for (port = slave->input_port;
            port != &slave->input_port[slave->input_port_count];
            port++) {
        (*func)(port->mapped_pdo);
        (*func)(port->gain_values);
        (*func)(port->gain_name);
    }

    (*func)(slave->output_port);
    (*func)(slave->input_port);

    (*func)(slave);
}

/*====================*
 * S-function methods *
 *====================*/

/* Function: mdlInitializeSizes =============================================
 * Abstract:
 *    The sizes information is used by Simulink to determine the S-function
 *    block's characteristics (number of inputs, outputs, states, etc.).
 */
static void mdlInitializeSizes(SimStruct *S)
{
    uint_T i;
    struct ecat_slave *slave;
    struct io_port *port;
    uint_T port_idx;

    ssSetNumSFcnParams(S, PARAM_COUNT);  /* Number of expected parameters */
    if (ssGetNumSFcnParams(S) != ssGetSFcnParamsCount(S)) {
        /* Return if number of expected != number of actual parameters */
        return;
    }

    for( i = 0; i < PARAM_COUNT; i++)
        ssSetSFcnParamTunable(S,i,SS_PRM_NOT_TUNABLE);

    /* allocate memory for slave structure */
    if (!(slave = mxCalloc(1,sizeof(*slave)))) {
        return;
    }
    ssSetUserData(S,slave);

    if (get_slave_address(S, slave)) return;
    if (get_ethercat_info(S, slave)) return;
    slave->layout_version = LAYOUT_VERSION;
    if (get_sdo_config(S, slave)) return;

    /* Process input ports */
    if (get_ioport_config(S, slave, 
                &slave->input_port, &slave->input_port_count))
        return;
    if (!ssSetNumInputPorts(S, slave->input_port_count))
        return;
    for (port_idx = 0, port = slave->input_port; 
            port_idx < slave->input_port_count; port_idx++, port++) {

        ssSetInputPortWidth(S, port_idx, port->width);

        if (!port->pdo_full_scale) {
            ssSetInputPortDataType(S, port_idx, port->data_type);
        }
        else {
            ssSetInputPortDataType(S, port_idx, DYNAMICALLY_TYPED);
        }
    }

    /* Process output ports */
    if (get_ioport_config(S, slave, 
                &slave->output_port, &slave->output_port_count))
        return;
    if (!ssSetNumOutputPorts(S, slave->output_port_count))
        return;
    for (port_idx = 0, port = slave->output_port; 
            port_idx < slave->output_port_count; port_idx++, port++) {

        ssSetOutputPortWidth(S, port_idx, port->width);

        ssSetOutputPortDataType(S, port_idx, port->data_type);
    }

    ssSetNumSampleTimes(S, 1);
    ssSetNumPWork(S, slave->pwork_count);
    ssSetNumIWork(S, slave->iwork_count);
    if (TSAMPLE) {
        ssSetNumDiscStates(S, slave->filter_count);
    } else {
        ssSetNumContStates(S, slave->filter_count);
    }

    /* Make the memory peristent, otherwise it is lost just before
     * mdlRTW is called. To ensure that the memory is released again,
     * even in case of failures, the option SS_OPTION_CALL_TERMINATE_ON_EXIT
     * has to be set */
    slave_mem_op(slave, mexMakeMemoryPersistent);

    ssSetOptions(S,
            SS_OPTION_WORKS_WITH_CODE_REUSE
            | SS_OPTION_RUNTIME_EXCEPTION_FREE_CODE
            | SS_OPTION_CALL_TERMINATE_ON_EXIT);
}

/* Function: mdlInitializeSampleTimes =======================================
 * Abstract:
 *    This function is used to specify the sample time(s) for your
 *    S-function. You must register the same number of sample times as
 *    specified in ssSetNumSampleTimes.
 */
static void mdlInitializeSampleTimes(SimStruct *S)
{
    ssSetSampleTime(S, 0, TSAMPLE);
    ssSetOffsetTime(S, 0, 0.0);
}

#define MDL_SET_OUTPUT_PORT_WIDTH
static void mdlSetOutputPortWidth(SimStruct *S, int_T port, int_T width)
{
    snprintf(errmsg, sizeof(errmsg),
            "Don't know why output port width should be set for port %u.",
            port+1);
    ssSetErrorStatus(S, errmsg);
    return;
}

#define MDL_SET_INPUT_PORT_WIDTH
static void mdlSetInputPortWidth(SimStruct *S, int_T port, int_T width)
{
#if 0
    struct ecat_slave *slave = ssGetUserData(S);
    struct io_port *input_spec = &slave->input_spec[port];

    if (width > input_spec->port_width) {
        snprintf(errmsg, sizeof(errmsg),
                "Maximum port width for input port %u is %u.\n"
                "Tried to set it to %i.",
                port+1, input_spec->port_width, width);
        ssSetErrorStatus(S, errmsg);
        return;
    }

    ssSetInputPortWidth(S, port, width);

    if (slave->port_width > 1) {
        if (width > 1 && slave->port_width != width) {
            slave->unequal_port_widths = 1;
        }
    }
    else {
        slave->port_width = width;
    }
#endif
}

/* This function is called when some ports are still DYNAMICALLY_SIZED
 * even after calling mdlSetInputPortWidth(). This occurs when input
 * ports are not connected. This function sets the port width for
 * these ports to 1 */
#define MDL_SET_DEFAULT_PORT_DIMENSION_INFO
static void mdlSetDefaultPortDimensionInfo(SimStruct *S)
{
#if 0
    struct ecat_slave *slave = ssGetUserData(S);
    struct io_port *input_spec;
    int_T port;

    for (input_spec = slave->input_spec, port = 0;
            input_spec != &slave->input_spec[slave->num_inputs];
            input_spec++, port++) {
        if (ssGetInputPortWidth(S, port) == DYNAMICALLY_SIZED) {
            ssSetInputPortWidth(S, port, input_spec->port_width);
        }
    }
#endif
}

#define MDL_SET_WORK_WIDTHS
static void mdlSetWorkWidths(SimStruct *S)
{
#if 0
    struct ecat_slave *slave = ssGetUserData(S);
    struct io_port *output_spec;
    struct io_port *input_spec;
    uint_T param_idx = 0;

    if (slave->unequal_port_widths) {
        int_T port;
        for (input_spec = slave->input_spec, port = 0;
                input_spec != &slave->input_spec[slave->num_inputs];
                input_spec++, port++) {
            ssSetInputPortRequiredContiguous(S, port, 1);
        }
    }

    ssSetNumRunTimeParams(S, slave->runtime_param_count);

    for (input_spec = slave->input_spec;
            input_spec != &slave->input_spec[slave->num_inputs];
            input_spec++) {
        if (input_spec->gain_count && input_spec->gain_name) {
            ssParamRec p = {
                .name = input_spec->gain_name,
                .nDimensions = 1,
                .dimensions = &input_spec->gain_count,
                .dataTypeId = SS_DOUBLE,
                .complexSignal = 0,
                .data = input_spec->gain_values,
                .dataAttributes = NULL,
                .nDlgParamIndices = 0,
                .dlgParamIndices = NULL,
                .transformed = RTPARAM_TRANSFORMED,
                .outputAsMatrix = 0,
            };
            ssSetRunTimeParamInfo(S, param_idx++, &p);
        }
    }

    for (output_spec = slave->output_spec;
            output_spec != &slave->output_spec[slave->num_outputs];
            output_spec++) {
        if (output_spec->gain_count && output_spec->gain_name) {
            ssParamRec p = {
                .name = output_spec->gain_name,
                .nDimensions = 1,
                .dimensions = &output_spec->gain_count,
                .dataTypeId = SS_DOUBLE,
                .complexSignal = 0,
                .data = output_spec->gain_values,
                .dataAttributes = NULL,
                .nDlgParamIndices = 0,
                .dlgParamIndices = NULL,
                .transformed = RTPARAM_TRANSFORMED,
                .outputAsMatrix = 0,
            };
            ssSetRunTimeParamInfo(S, param_idx++, &p);
        }
        if (output_spec->offset_count && output_spec->offset_name) {
            ssParamRec p = {
                .name = output_spec->offset_name,
                .nDimensions = 1,
                .dimensions = &output_spec->offset_count,
                .dataTypeId = SS_DOUBLE,
                .complexSignal = 0,
                .data = output_spec->offset_values,
                .dataAttributes = NULL,
                .nDlgParamIndices = 0,
                .dlgParamIndices = NULL,
                .transformed = RTPARAM_TRANSFORMED,
                .outputAsMatrix = 0,
            };
            ssSetRunTimeParamInfo(S, param_idx++, &p);
        }
        if (output_spec->filter_count && output_spec->filter_name) {
            ssParamRec p = {
                .name = output_spec->filter_name,
                .nDimensions = 1,
                .dimensions = &output_spec->filter_count,
                .dataTypeId = SS_DOUBLE,
                .complexSignal = 0,
                .data = output_spec->filter_values,
                .dataAttributes = NULL,
                .nDlgParamIndices = 0,
                .dlgParamIndices = NULL,
                .transformed = RTPARAM_TRANSFORMED,
                .outputAsMatrix = 0,
            };
            ssSetRunTimeParamInfo(S, param_idx++, &p);
        }
    }

    return;
#endif
}

/* Function: mdlOutputs =====================================================
 * Abstract:
 *    In this function, you compute the outputs of your S-function
 *    block. Generally outputs are placed in the output vector, ssGetY(S).
 */
static void mdlOutputs(SimStruct *S, int_T tid)
{
}

#define MDL_DERIVATIVES
static void mdlDerivatives(SimStruct *S)
{
    /* Required, otherwise Simulink complains if the filter is chosen
     * while in continuous sample time */
}

/* Function: mdlTerminate ===================================================
 * Abstract:
 *    In this function, you should perform any actions that are necessary
 *    at the termination of a simulation.  For example, if memory was
 *    allocated in mdlStart, this is the place to free it.
 */
static void mdlTerminate(SimStruct *S)
{
    struct ecat_slave *slave = ssGetUserData(S);

    slave_mem_op(slave, mxFree);
}

/* Create a string of the form ["one","two","three"]\0
 * from the strings passed as a array of pointers. Last element must be NULL.
 * */
char_T *createStrVect(const char_T *strings[])
{
    uint_T len = 0;
    const char_T **s;
    char_T *str_vect, *p;

    for (s = strings; *s; s++) {
        len += strlen(*s) + 3;
    }

    len += 3; /* For the square braces and trailing \0 */

    p = str_vect = mxCalloc(1, len);

    *p++ = '[';
    for (s = strings; *s; s++) {
        *p++ = '"';
        strcpy(p, *s);
        p += strlen(*s);
        *p++ = '"';
        *p++ = ',';
    }
    *--p = ']'; /* Overwrite last comma with ] */

    return str_vect;
}

#define MDL_RTW
static void mdlRTW(SimStruct *S)
{
#if 0
    struct ecat_slave *slave = ssGetUserData(S);
    uint_T master = ECMASTER;
    uint_T domain = ECDOMAIN;
    uint_T alias = ECALIAS;
    uint_T position = ECPOSITION;
    uint_T vendor = ECVENDOR;
    uint_T product = ECPRODUCT;
    uint_T layout = LAYOUT;
    uint_T port, i;
    uint32_T map_idx;
    struct io_spec *io_spec;
    uint_T gain_idx;
    uint_T offset_idx;
    uint_T filter_idx;

    uint32_T input_port_spec[4][slave->num_inputs];
    uint32_T input_map[2][slave->input_map_count];
    real_T   input_pdo_full_scale[slave->input_map_count];
    const char_T *input_gain_str;
    const char_T *input_gain_name_ptr[slave->num_inputs + 1];
    real_T input_gain[slave->static_input_gain_count];

    uint32_T output_port_spec[6][slave->num_outputs];
    uint32_T output_map[2][slave->output_map_count];
    real_T   output_pdo_full_scale[slave->output_map_count];
    const char_T *output_gain_str;
    const char_T *output_offset_str;
    const char_T *output_filter_str;
    const char_T *output_gain_name_ptr[slave->num_outputs + 1];
    const char_T *output_offset_name_ptr[slave->num_outputs + 1];
    const char_T *output_filter_name_ptr[slave->num_outputs + 1];
    real_T output_gain[slave->static_output_gain_count];
    real_T output_offset[slave->static_output_offset_count];
    real_T output_filter[slave->static_output_filter_count];

    gain_idx = map_idx = 0;
    for (io_spec = slave->input_spec, port = 0;
            io_spec != &slave->input_spec[slave->num_inputs];
            io_spec++, port++) {
        struct io_spec_map *map;

        input_port_spec[0][port]   = io_spec->pdo_data_type;
        input_port_spec[1][port]   = 
            (io_spec->bitmask ? 0x2 : 0)
            | (io_spec->raw ? 0x1 : 0);
        input_port_spec[2][port]   = io_spec->map_count;
        input_port_spec[3][port]   = io_spec->gain_count;
        input_pdo_full_scale[port] = io_spec->pdo_full_scale;
        input_gain_name_ptr[port]  = io_spec->gain_count
            ? io_spec->gain_name : "";

        for (i = 0; !io_spec->gain_name && i < io_spec->gain_count;
                i++) {
            input_gain[gain_idx++] = io_spec->gain_values[i];
        }

        for (map = io_spec->map;
                map != &io_spec->map[io_spec->map_count];
                map++, map_idx++) {
            input_map[0][map_idx] = map->pdo_info_idx;
            input_map[1][map_idx] = map->pdo_entry_idx;
        }
    }
    input_gain_name_ptr[slave->num_inputs] = NULL;
    input_gain_str = createStrVect(input_gain_name_ptr);

    gain_idx = offset_idx = filter_idx = map_idx = 0;
    for (io_spec = slave->output_spec, port = 0;
            io_spec != &slave->output_spec[slave->num_outputs];
            io_spec++, port++) {
        struct io_spec_map *map;

        output_port_spec[0][port]   = io_spec->pdo_data_type;
        output_port_spec[1][port]   = 
            (io_spec->bitmask ? 0x2 : 0)
            | (io_spec->raw ? 0x1 : 0);
        output_port_spec[2][port]   = io_spec->map_count;
        output_port_spec[3][port]   = io_spec->gain_count;
        output_port_spec[4][port]   = io_spec->offset_count;
        output_port_spec[5][port]   = io_spec->filter_count;
        output_pdo_full_scale[port] = io_spec->pdo_full_scale;
        output_gain_name_ptr[port]  = io_spec->gain_count
            ? io_spec->gain_name : "";
        output_offset_name_ptr[port] = io_spec->offset_count
            ? io_spec->offset_name : "";
        output_filter_name_ptr[port] = io_spec->filter_count
            ? io_spec->filter_name : "";

        for (i = 0; !io_spec->gain_name && i < io_spec->gain_count;
                i++) {
            output_gain[gain_idx++] = io_spec->gain_values[i];
        }
        for (i = 0; !io_spec->offset_name && i < io_spec->offset_count;
                i++) {
            output_offset[offset_idx++] = io_spec->offset_values[i];
        }
        for (i = 0; !io_spec->filter_name && i < io_spec->filter_count;
                i++) {
            output_filter[filter_idx++] = io_spec->filter_values[i];
        }

        for (map = io_spec->map;
                map != &io_spec->map[io_spec->map_count];
                map++, map_idx++) {
            output_map[0][map_idx] = map->pdo_info_idx;
            output_map[1][map_idx] = map->pdo_entry_idx;
        }
    }
    output_gain_name_ptr[slave->num_outputs] = NULL;
    output_offset_name_ptr[slave->num_outputs] = NULL;
    output_filter_name_ptr[slave->num_outputs] = NULL;
    output_gain_str = createStrVect(output_gain_name_ptr);
    output_offset_str = createStrVect(output_offset_name_ptr);
    output_filter_str = createStrVect(output_filter_name_ptr);

    if (!ssWriteRTWScalarParam(S, "MasterId", &master, SS_UINT32))
        return;
    if (!ssWriteRTWScalarParam(S, "DomainId", &domain, SS_UINT32))
        return;
    if (!ssWriteRTWScalarParam(S, "SlaveAlias", &alias, SS_UINT32))
        return;
    if (!ssWriteRTWScalarParam(S, "SlavePosition", &position, SS_UINT32))
        return;

    if (!ssWriteRTWStrParam(S, "ProductName", slave->product_name))
        return;
    if (!ssWriteRTWScalarParam(S, "VendorId", &vendor, SS_UINT32))
        return;
    if (!ssWriteRTWScalarParam(S, "ProductCode", &product, SS_UINT32))
        return;
    if (!ssWriteRTWScalarParam(S, "ConfigLayout", &layout, SS_UINT32))
        return;

    if (slave->sdo_config_len) { /* Transpose slave->sdo_config */
        uint32_T sdo_config[4][slave->sdo_config_len];
        uint_T i, j;

        for (i = 0; i < slave->sdo_config_len; i++) {
            for (j = 0; j < 4; j++) {
                sdo_config[j][i] = slave->sdo_config[i][j];
            }
        }

        if (!ssWriteRTW2dMatParam(S, "SdoConfig", sdo_config,
                    SS_UINT32, slave->sdo_config_len, 4))
            return;
    }
    if (slave->pdo_entry_info_len) { /* Transpose slave->pdo_entry_info */
        uint32_T pdo_entry_info[3][slave->pdo_entry_info_len];
        uint_T i;

        for (i = 0; i < slave->pdo_entry_info_len; i++) {
            pdo_entry_info[0][i] = slave->pdo_entry_info[i].index;
            pdo_entry_info[1][i] = slave->pdo_entry_info[i].subindex;
            pdo_entry_info[2][i] = slave->pdo_entry_info[i].bitlen;
        }

        if (!ssWriteRTW2dMatParam(S, "PdoEntryInfo", pdo_entry_info,
                    SS_UINT32, slave->pdo_entry_info_len, 3))
            return;
    }
    if (slave->pdo_info_len) { /* Transpose slave->pdo_info */
        uint32_T pdo_info[4][slave->pdo_info_len];
        uint_T i, j;

        for (i = 0; i < slave->pdo_info_len; i++) {
            for (j = 0; j < 4; j++) {
                pdo_info[j][i] = slave->pdo_info[i][j];
            }
        }

        if (!ssWriteRTW2dMatParam(S, "PdoInfo", pdo_info,
                    SS_UINT32, slave->pdo_info_len, 4))
            return;
    }

    if (slave->num_inputs) {
        if (!ssWriteRTW2dMatParam(S, "InputMap", input_map,
                    SS_UINT32, slave->input_map_count, 2))
            return;
        if (!ssWriteRTW2dMatParam(S, "InputPortSpec", input_port_spec,
                    SS_UINT32, slave->num_inputs, 4))
            return;
        if (!ssWriteRTWVectParam(S, "InputPDOFullScale",
                    input_pdo_full_scale, SS_DOUBLE, slave->num_inputs))
            return;
        if (!ssWriteRTWStrVectParam(S, "InputGainName", input_gain_str,
                    slave->num_inputs))
            return;
        if (!ssWriteRTWVectParam(S, "InputGain", input_gain,
                    SS_DOUBLE, slave->static_input_gain_count))
            return;
    }

    if (slave->num_outputs) {
        if (!ssWriteRTW2dMatParam(S, "OutputMap", output_map,
                    SS_UINT32, slave->output_map_count, 2))
            return;
        if (!ssWriteRTW2dMatParam(S, "OutputPortSpec", output_port_spec,
                    SS_UINT32, slave->num_outputs, 6))
            return;
        if (!ssWriteRTWVectParam(S, "OutputPDOFullScale",
                    output_pdo_full_scale, SS_DOUBLE, slave->num_outputs))
            return;
        if (!ssWriteRTWStrVectParam(S, "OutputGainName", output_gain_str,
                    slave->num_outputs))
            return;
        if (!ssWriteRTWStrVectParam(S, "OutputOffsetName", output_offset_str,
                    slave->num_outputs))
            return;
        if (!ssWriteRTWStrVectParam(S, "OutputFilterName", output_filter_str,
                    slave->num_outputs))
            return;
        if (!ssWriteRTWVectParam(S, "OutputGain", output_gain,
                    SS_DOUBLE, slave->static_output_gain_count))
            return;
        if (!ssWriteRTWVectParam(S, "OutputOffset", output_offset,
                    SS_DOUBLE, slave->static_output_offset_count))
            return;
        if (!ssWriteRTWVectParam(S, "OutputFilter", output_filter,
                    SS_DOUBLE, slave->static_output_filter_count))
            return;
    }

    if (!ssWriteRTWWorkVect(S, "IWork", 1, "BitOffset", slave->iwork_count))
        return;

    if (!ssWriteRTWWorkVect(S, "PWork", 1, "DataPtr",
                slave->output_map_count + slave->input_map_count))
        return;
#endif
}



/*======================================================*
 * See sfuntmpl_doc.c for the optional S-function methods *
 *======================================================*/

/*=============================*
 * Required S-function trailer *
 *=============================*/

#ifdef  MATLAB_MEX_FILE    /* Is this file being compiled as a MEX-file? */
#include "simulink.c"      /* MEX-file interface mechanism */
#else
#include "cg_sfun.h"       /* Code generation registration function */
#endif
