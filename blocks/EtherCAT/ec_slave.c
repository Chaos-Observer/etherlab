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
 * 9. Pdo Entry Info - N-by-3 numeric matrix
 *   A matrix of 3 columns describing the PDO entry information. It is used
 *   to configure the slave's PDO's. The columns have the following 
 *   functions:
 *    1 - Index
 *    2 - Subindex
 *    3 - Bit length: one of 1, 8, -8, 16, -16, 32, -32 mapping to
 *              bool_t, uint8_t, int8_t, uint16_t, int16_t, uint32_t, int32_t
 *
 * 10. PDO Information - N-by-4 numeric matrix
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
 * 11. SDO Configuration - N-by-4 numeric matrix
 *   A matrix with 4 columns that is used to configure a slave. The 
 *   configuration is transferred to the slave during initialisation.
 *   The columns have the following functions:
 *    1 - Index
 *    2 - Subindex
 *    3 - Data type: one of 8, 16, 32
 *    4 - Value
 *
 * 12. Block input port specification - Vector of a structure
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
 *   'full_scale_bits': scalar numeric value (optional)
 *                      If this field is not specified, the input port is
 *                      the "raw" PDO value. This also means that the port
 *                      has the same data type as specified in the data type
 *                      of the PDO.
 *                      If this field specified, the input port value
 *                      is multiplied by this value before writing it to
 *                      the PDO, i.e.
 *                      'PDO value' = input * 2^'full_scale_bits'
 *
 *                      Ideally this value is the maximum value 
 *                      that the PDO can assume, thus an input range of
 *                      [-1.0 .. 1.0> for signed PDO's and [0 .. 1.0> for
 *                      unsigned PDO's is mapped to the full PDO value range.
 *
 *                      This value can also be assigned to 0, meaning that
 *                      no scaling is performed (2^0 = 1)
 *
 *                      For example, a full_scale_bits = 15 would map the 
 *                      entire value range of a int16_t PDO data type
 *                      from -1.0 to 1.0
 *
 *   'gain':            numeric vector (optional)
 *                      An optional vector or scalar. If it is a vector,
 *                      it must have the same number of elements as there
 *                      are rows in 'pdo_map'. This field is only considered
 *                      when 'full_scale_bits' is specified.
 *                      If this field is specified, the value written to
 *                      the PDO is premultiplied before assignment.
 *                      i.e.
 *                      'PDO value' = 'gain' .* input * 'full_scale_bits'
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
 * 13. Block output port specification - Vector of a structure
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
 *   'full_scale_bits': scalar numeric value (optional)
 *                      If this field is not specified, the output port is
 *                      the "raw" PDO value. This also means that the port
 *                      has the same data type as specified in the data type
 *                      of the PDO.
 *                      If this field specified, the output port has data
 *                      type 'double_T'. The PDO value is then divided by
 *                      this value and written to the output port, i.e.
 *                      output = 'PDO value' / 2^'full_scale_bits'
 *
 *                      Ideally this value is the maximum value that the 
 *                      PDO can assume, thus mapping the PDO value range to 
 *                      [0 .. 1.0> for unsigned PDO data types and
 *                      [-1.0 .. 1.0> for signed PDO data types.
 *
 *                      This value can also be assigned to 0, meaning that
 *                      no scaling is performed (2^0 = 1)
 *
 *                      For example, a full_scale_bits = 15 would map the 
 *                      entire value range of a int16_t PDO data type
 *                      from -1.0 to 1.0
 *
 *   'gain':            numeric vector (optional)
 *                      An optional vector or scalar. If it is a vector,
 *                      it must have the same number of elements as there
 *                      are rows in 'pdo_map'. This field is only considered
 *                      when 'full_scale_bits' is specified.
 *                      If this field is specified, the PDO value is further
 *                      multiplied by it after applying 'full_scale_bits'.
 *                      i.e.
 *                      output = 'gain' .* 'PDO value' / 'full_scale_bits'
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
 *                      when 'full_scale_bits' is specified.
 *                      If this field is specified, it is added to the result
 *                      of applying 'full_scale_bits' and 'gain',
 *                      i.e.
 *                      output = 'gain' .* 'PDO value' / 'full_scale_bits'
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
 *                      when 'full_scale_bits' is specified.
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

#define S_FUNCTION_NAME  ec_slave
#define S_FUNCTION_LEVEL 2

#include "simstruc.h"

#define ECMASTER              mxGetScalar(ssGetSFcnParam(S,0))
#define ECDOMAIN              mxGetScalar(ssGetSFcnParam(S,1))
#define ECALIAS               mxGetScalar(ssGetSFcnParam(S,2))
#define ECPOSITION            mxGetScalar(ssGetSFcnParam(S,3))

#define PRODUCT_NAME                      ssGetSFcnParam(S,4)
#define ECVENDOR              mxGetScalar(ssGetSFcnParam(S,5))
#define ECPRODUCT             mxGetScalar(ssGetSFcnParam(S,6))
#define LAYOUT                mxGetScalar(ssGetSFcnParam(S,7))

#define PDO_ENTRY_INFO                    ssGetSFcnParam(S,8)
#define PDO_INFO                          ssGetSFcnParam(S,9)
#define SDO_CONFIG                        ssGetSFcnParam(S,10)

#define INPUT                             ssGetSFcnParam(S,11)
#define OUTPUT                            ssGetSFcnParam(S,12)

#define TSAMPLE               mxGetScalar(ssGetSFcnParam(S,13))
#define PARAM_COUNT                                        14

char errmsg[256];

struct ecat_slave {
    char *product_name;

    /* One record for every SDO configuration element:
     * 1: SDO Index
     * 2: SDO_Subindex
     * 3: DataType: SS_UINT8, SS_UINT16, SS_UINT32
     * 4: Value */
    uint32_T (*sdo_config)[4];
    uint_T   sdo_config_len;

    struct pdo_entry_info {
        uint_T index;
        uint_T subindex;
        int_T bitlen;
    } *pdo_entry_info;
    uint_T   pdo_entry_info_len;

    /* 1: 1 = Input, 0 = Output
     * 2: PdoIndex, 
     * 3: start row in PdoEntry, 
     * 4: number of PDdoEntry rows */
    uint32_T (*pdo_info)[4];
    uint_T   pdo_info_len;

    uint_T port_width;
    uint_T unequal_port_widths;

    struct io_spec {
        struct io_spec_map {
            uint32_T pdo_info_idx;
            uint32_T pdo_entry_idx;
        } *map;

        /* Data type after applying the mask. If Raw block IO is used, the
         * port will have this data type */
        uint_T data_type;

        /* The data type of the PDO. */
        uint_T pdo_data_type;

        /* Number of signals for the port
         * This is also the number of *map entries,
         * i.e. one PDO for every signal. 
         */
        uint_T port_width;

        /* The following specs are needed to determine whether the 
         * input/output value needs to be scaled before writing to 
         * the PDO. If pdo_bits != 0, the input is raw anyway and the 
         * following values are ignored.
         * If raw is set, the output is raw, i.e. no scaling is done
         * raw is bit coded:
         *      bit 1:   Raw output
         *      bit 2:   Bit masking is required, otherwise output is
         *               one of the native types u/int8 u/int16 u/int32
         *
         * If raw is unset, the value written to / read from the PDO
         * is scaled using gain and/or offset; see introduction concerning
         * full_scale_bits
         *
         * gain_values appears as an application parameter if supplied.
         */
        uint_T raw;
        uint_T full_scale_bits;

        uint_T  gain_count;
        char_T *gain_name;
        real_T *gain_values;

        uint_T  offset_count;
        char_T *offset_name;
        real_T *offset_values;

        uint_T  filter_count;
        char_T *filter_name;
        real_T *filter_values;

    } *output_spec, *input_spec;

    /* Number of input and output ports */
    uint_T num_inputs;
    uint_T num_outputs;

    /* Total count of input and output PDO's that are used */
    uint_T input_map_count;
    uint_T output_map_count;

    uint_T static_input_gain_count;
    uint_T static_output_gain_count;
    uint_T static_output_offset_count;
    uint_T static_output_filter_count;

    uint_T output_filter_count;
    uint_T runtime_param_count;

    uint_T iwork_count;
};

const char_T* 
get_data_type(int_T width, uint_T *data_type)
{
    if (!width) {
        return "Error: Data type with zero width not allowed.";
    } 
    else if (width < 0) {
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
                return "Error: Non byte-aligned signed data type chosen; "
                    "Allowed are -8, -16 or -32.";
                break;
        }
    }
    else if (width > 32) {
        return "Error: Unsigned data type must be in the range [1..32].";
    } 
    else if (width > 16) {
        *data_type = SS_UINT32;
    } 
    else if (width > 8) {
        *data_type = SS_UINT16;
    } 
    else if (width > 1) {
        *data_type = SS_UINT8;
    } else {
        *data_type = SS_BOOLEAN;
    }

    return NULL;
}

const char_T* 
get_pdo_bit_spec_and_dir(int_T *bit_len, uint_T *dir, 
        const struct ecat_slave *slave, uint_T pdo_info_idx, 
        uint_T pdo_entry_idx) 
{
    const char_T *err = NULL;

    if (pdo_info_idx >= slave->pdo_info_len) {
        return "pdo_info_idx exceeds matrix dimensions.";
    }

    if (pdo_entry_idx >= slave->pdo_info[pdo_info_idx][3]) {
        return "pdo_entry_idx exceeds bounds defined in pdo_info.";
    }

    /* The pdo_entry_idx passed as an argument to this function still has
     * to be offset by the PDO Entry base as specified in PDO Info */
    pdo_entry_idx += slave->pdo_info[pdo_info_idx][2];
    if (pdo_entry_idx >= slave->pdo_entry_info_len) {
        return "pdo_entry_idx exceeds matrix dimensions.";
    }

    *bit_len = slave->pdo_entry_info[pdo_entry_idx].bitlen;
    *dir = slave->pdo_info[pdo_info_idx][0];
    return err;
}
    
const char_T*
get_parameter_values(SimStruct* S, const mxArray* output, uint_T port,
        const char_T* field, uint_T *count, uint_T port_width, 
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
    const mxArray* full_scale_bits = mxGetField(src, port, "full_scale_bits");
    const mxArray* pdo_data_type = mxGetField(src, port, "pdo_data_type");
    const char_T* errfield = NULL;
    const char_T* err;
    const char_T* dir_str = port_dir ? "input" : "output";
    real_T *pdo_info_idx;
    real_T *pdo_entry_idx;
    uint_T i;
    int_T port_bit_len = 0;

    /* Check that the required fields for port are specified */
    if (!pdo_map) {
        snprintf(errmsg, sizeof(errmsg),
                "Required field 'pdo_map' in structure for %s port %u "
                "not defined.", dir_str, port+1);
        return errmsg;
    }

    /* Make sure matrices have the right dimensions */
    if (mxGetN(pdo_map) != 2 || !mxIsDouble(pdo_map)) {
        /* PDO map must have 2 columns */
        snprintf(errmsg, sizeof(errmsg),
                "Matrix specified for %s(%u).%s must be a numeric array "
                "with 2 columns.",
                dir_str, port+1, "pdo_map");
        return errmsg;
    }

    io_spec->port_width = mxGetM(pdo_map);

    pdo_info_idx = mxGetPr(pdo_map);
    pdo_entry_idx = pdo_info_idx + io_spec->port_width;

    io_spec->map = mxCalloc(io_spec->port_width, sizeof(*io_spec->map));
    if (!io_spec->map)
        return ssGetErrorStatus(S);
    *total_map_count += io_spec->port_width;
    io_spec->port_width = io_spec->port_width;

    for (i = 0; i < io_spec->port_width; i++) {
        int_T pdo_bit_len;
        uint_T pdo_dir;

        if ((err = get_pdo_bit_spec_and_dir(&pdo_bit_len, &pdo_dir, slave,  
                        pdo_info_idx[i], pdo_entry_idx[i]))) {
            snprintf(errmsg, sizeof(errmsg),
                    "Error occurred while determinig data type "
                    "of a PDO[%u] for %s(%u): %s", i, dir_str, port+1, err);
            return errmsg;
        }

        if (pdo_dir != port_dir) {
            snprintf(errmsg, sizeof(errmsg),
                    "PDO direction has wrong direction for %s(%u)",
                    dir_str, port+1);
            return errmsg;
        }

        /* If port_bit_len has been assigned already, check that the 
         * subsequent bit_len's have the same length */
        if (port_bit_len) {
            if (port_bit_len != pdo_bit_len) {
                snprintf(errmsg, sizeof(errmsg),
                        "Cannot mix different PDO data types for "
                        "%s(%u).", dir_str, port+1);
                return errmsg;
            }
        }
        else {
            port_bit_len = pdo_bit_len;

            if ((err = get_data_type(port_bit_len, &io_spec->data_type))) {
                snprintf(errmsg, sizeof(errmsg),
                        "An error occurred while determining the data type "
                        "specified by %s(%u).%s: %s",
                        dir_str, port+1, "pdo_data_type", err);
                return errmsg;
            }

            /* Test whether a custom PDO data type is specified for the port 
             * that is different from the default as derived from the type
             * specified by the bitlen of pdo_entry_info. */
            if (!pdo_data_type) {
                /* No custom PDO data type specified. 
                 * Use that of pdo_entry_info. */
                io_spec->pdo_data_type = io_spec->data_type == SS_BOOLEAN 
                    ? SS_UINT8 : io_spec->data_type;
            }
            else if (!(pdo_bit_len % 8)) {
                snprintf(errmsg, sizeof(errmsg),
                        "Only allowed to specify field %s(%u).%s when "
                        "the PDO does not have a native data type.",
                        dir_str, port+1, "pdo_data_type");
                return errmsg;
            }
            else {
                int_T user_data_type = mxGetScalar(pdo_data_type);

                if (!mxIsDouble(pdo_data_type)) {
                    snprintf(errmsg, sizeof(errmsg),
                            "Specified field %s(%u).%s must be a numeric "
                            "scalar.", dir_str, port+1, "pdo_data_type");
                    return errmsg;
                }

                if (abs(pdo_bit_len) > abs(user_data_type)) {
                    snprintf(errmsg, sizeof(errmsg),
                            "Data type specified by %s(%u).%s has %u bits. "
                            "Required is at least %u bits.",
                            dir_str, port+1, "pdo_data_type", 
                            abs(user_data_type), abs(pdo_bit_len));
                    return errmsg;
                }

                if (user_data_type % 8) {
                    snprintf(errmsg, sizeof(errmsg),
                            "Data type specified by %s(%u).%s has %u bits. "
                            "This must be a multiple of 8.",
                            dir_str, port+1, "pdo_data_type", 
                            abs(user_data_type));
                    return errmsg;
                }

                if ((err = get_data_type(
                                user_data_type, &io_spec->pdo_data_type))) {
                    snprintf(errmsg, sizeof(errmsg),
                            "An error occurred while determining the data type "
                            "specified by %s(%u).%s: %s",
                            dir_str, port+1, "pdo_data_type", err);
                    return errmsg;
                }
                printf("%s pdo_data_type specified %u %u %u\n", 
                        slave->product_name, user_data_type, 
                        io_spec->pdo_data_type, SS_UINT16);
            }
        }

        io_spec->map[i].pdo_info_idx = pdo_info_idx[i];
        io_spec->map[i].pdo_entry_idx = pdo_entry_idx[i];
    }

    if (port_bit_len % 8) {
        if (full_scale_bits) {
            snprintf(errmsg, sizeof(errmsg),
                    "Boolean signals cannot be scaled for %s(%u).",
                    dir_str, port+1);
            return errmsg;
        }
        *iwork_count += io_spec->port_width;

        /* Set bit 2 of raw to indicate that bit mask operations are
         * required */
        io_spec->raw |= 0x2;
    }

    if (!full_scale_bits) {
        /* Set bit 1 of raw to indicate that the output is raw */
        io_spec->raw |= 0x1;
        return NULL;
    }

    /* Output is double. This means that the source value must be scaled 
     * so that the source value range is mapped to [0..1] */
    if (!mxIsDouble(full_scale_bits)) {
        snprintf(errmsg, sizeof(errmsg),
                "Specified field %s(%u).%s must be a numeric scalar.",
                dir_str, port+1, "full_scale_bits");
        return errmsg;
    }
    io_spec->full_scale_bits = mxGetScalar(full_scale_bits);

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

/*====================*
 * S-function methods *
 *====================*/

/* Function: mdlInitializeSizes ===============================================
 * Abstract:
 *    The sizes information is used by Simulink to determine the S-function
 *    block's characteristics (number of inputs, outputs, states, etc.).
 */
static void mdlInitializeSizes(SimStruct *S)
{
    uint_T i, j;
    struct ecat_slave *slave;
    struct io_spec *output_spec;
    struct io_spec *input_spec;
    const mxArray *product_name = PRODUCT_NAME;
    uint_T len;
    
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

    len = mxGetN(product_name) + 1;
    slave->product_name = mxCalloc(len, 1);
    if (!slave->product_name)
        return;

    if (!mxIsChar(product_name) || 
            mxGetString(product_name, slave->product_name, len)) {
        ssSetErrorStatus(S, "Invalid product name parameter supplied.");
        return;
    }

    /* Fetch the PDO Entries from Simulink. This matrix has 3 columns, and 
     * describes which objects can be mapped */
    slave->pdo_entry_info_len = mxGetM(PDO_ENTRY_INFO);
    if (slave->pdo_entry_info_len) {
        real_T *pdo_entry_info_index = mxGetPr(PDO_ENTRY_INFO);
        real_T *pdo_entry_info_subindex =
            pdo_entry_info_index + slave->pdo_entry_info_len;
        real_T *pdo_entry_info_data_type =
            pdo_entry_info_subindex + slave->pdo_entry_info_len;

        if (!mxIsDouble(PDO_ENTRY_INFO) || mxGetN(PDO_ENTRY_INFO) != 3 
                || !slave->pdo_entry_info_len || !pdo_entry_info_index) {
            ssSetErrorStatus(S, "SFunction parameter pdo_entry must be "
                    "a n-by-3 numeric matrix");
            return;
        }

        slave->pdo_entry_info = mxCalloc(slave->pdo_entry_info_len,
                sizeof(*slave->pdo_entry_info));
        if (!slave->pdo_entry_info)
            return;

        for (i = 0; i < slave->pdo_entry_info_len; i++) {
            slave->pdo_entry_info[i].index = *pdo_entry_info_index++;
            slave->pdo_entry_info[i].subindex = *pdo_entry_info_subindex++;
            slave->pdo_entry_info[i].bitlen = *pdo_entry_info_data_type++;
        }
    }

    /* Fetch the PDO Info from Simulink. This matrix has 4 columns.
     * It describes the PDO mapping */
    slave->pdo_info_len = mxGetM(PDO_INFO);
    if (slave->pdo_info_len) {
        real_T* pdo_info_value = mxGetPr(PDO_INFO);
        if (!mxIsDouble(PDO_INFO) || mxGetN(PDO_INFO) != 4
                || !slave->pdo_info_len || !pdo_info_value) {
            ssSetErrorStatus(S, "SFunction parameter pdo_info must be "
                    "a n-by-4 numeric matrix");
            return;
        }

        slave->pdo_info = 
            mxCalloc(slave->pdo_info_len, sizeof(*slave->pdo_info));
        if (!slave->pdo_info)
            return;

        for (i = 0; i < slave->pdo_info_len; i++) {
            for (j = 0; j < 4; j++) {
                /* Note: mxArrays are stored column major! */
                slave->pdo_info[i][j] = 
                    pdo_info_value[i + j * slave->pdo_info_len];
            }
        }
    }

    /* Fetch the SDO configuration from Simulink. This matrix has 4 columns.
     * It describes slave configuration objects */
    slave->sdo_config_len = mxGetM(SDO_CONFIG);
    if (slave->sdo_config_len) {
        real_T *sdo_config_value = mxGetPr(SDO_CONFIG);

        if (!mxIsNumeric(SDO_CONFIG) || mxGetN(SDO_CONFIG) != 4
                || !sdo_config_value) {
            ssSetErrorStatus(S, "SFunction parameter sdo_config must be "
                    "a n-by-4 numeric matrix");
            return;
        }

        slave->sdo_config = mxCalloc(slave->sdo_config_len,
                    sizeof(*slave->sdo_config));
        if (!slave->sdo_config)
            return;

        for (i = 0; i < slave->sdo_config_len; i++) {
            const char_T *err;

            for (j = 0; j < 4; j++) {
                /* Note: mxArrays are stored column major! */
                slave->sdo_config[i][j] = 
                    sdo_config_value[i + j * slave->sdo_config_len];
            }
            if ((err = get_data_type( slave->sdo_config[i][2], 
                            &slave->sdo_config[i][2]))) {
                snprintf(errmsg, sizeof(errmsg),
                        "An error occurred while determining the data type "
                        "for SDO Config row %u: %s", i+1, err);
                ssSetErrorStatus(S, errmsg);
                return;
            }
        }
    }

    /* Specify the blocks outputs. These are defined in the 'output' struct
     * that was passed as an argument to this SFunction.
     *
     * There are as many outputs as there are elements in the 'output' struct.
     * The width of each output is specified by the number of PDO's that 
     * are mapped.
     * */
    slave->num_inputs = mxGetNumberOfElements(INPUT);
    if (slave->num_inputs) {
        uint_T port;

        /* Check that INPUT is a structure */
        if (!mxIsStruct(INPUT)) {
            ssSetErrorStatus(S, "SFunction parameter 'input' must be "
                    "specified as a structure.");
            return;
        }

        if (!ssSetNumInputPorts(S, slave->num_inputs)) return;

        slave->input_spec = 
            mxCalloc(slave->num_inputs, sizeof(*slave->input_spec));
        if (!slave->input_spec)
            return;

        /* Go through every input and determine its vector width. This
         * is equivalent to the number of PDOs that are mapped. At the
         * same time, count the total number of input PDOs that are mapped */
        for (port = 0, input_spec = slave->input_spec;
                input_spec != &slave->input_spec[slave->num_inputs]; 
                input_spec++, port++) {
            const char_T *err;

            if ((err = get_ioport_config(S, INPUT, input_spec, 
                            &slave->input_map_count, &slave->iwork_count,
                            slave, port, 1))) {
                ssSetErrorStatus(S, err);
                return;
            }

            ssSetInputPortWidth(S, port, DYNAMICALLY_SIZED);
            if (input_spec->raw) {
                ssSetInputPortDataType(S, port, input_spec->data_type);
            }
            else {
                ssSetInputPortDataType(S, port, DYNAMICALLY_TYPED);
            }

            if (input_spec->gain_count) {
                if (input_spec->gain_name) {
                    slave->runtime_param_count += input_spec->gain_count;
                } else {
                    slave->static_input_gain_count += input_spec->gain_count;
                }
            }
        }
    }

    /* Specify the blocks outputs. These are defined in the 'output' struct
     * that was passed as an argument to this SFunction.
     *
     * There are as many outputs as there are elements in the 'output' struct.
     * The width of each output is specified by the number of PDO's that 
     * are mapped.
     * */
    slave->num_outputs = mxGetNumberOfElements(OUTPUT);
    if (slave->num_outputs) {
        uint_T port;

        /* Check that OUTPUT is a structure */
        if (!mxIsStruct(OUTPUT)) {
            ssSetErrorStatus(S, "SFunction parameter 'output' must be "
                    "specified as a structure.");
            return;
        }

        if (!ssSetNumOutputPorts(S, slave->num_outputs)) return;

        slave->output_spec = 
            mxCalloc(slave->num_outputs, sizeof(*slave->output_spec));
        if (!slave->output_spec)
            return;

        /* Go through every output and determine its vector width. This
         * is equivalent to the number of PDOs that are mapped. At the
         * same time, count the total number of output PDOs that are mapped */
        for (port = 0, output_spec = slave->output_spec;
                output_spec != &slave->output_spec[slave->num_outputs]; 
                output_spec++, port++) {
            const char_T *err;

            if ((err = get_ioport_config(S, OUTPUT, output_spec, 
                            &slave->output_map_count, &slave->iwork_count,
                            slave, port, 0))) {
                ssSetErrorStatus(S, err);
                return;
            }
            /* Make sure that port widths are the same when > 1 */
            if (slave->port_width > 1) {
                if (output_spec->port_width > 1 
                        && slave->port_width != output_spec->port_width) {
                    slave->unequal_port_widths = 1;
                }
            }
            else {
                slave->port_width = output_spec->port_width;
            }


            /* port width is determined by now. Set it */
            ssSetOutputPortWidth(S, port, output_spec->port_width);

            if (output_spec->raw) {
                /* Raw output. */
                ssSetOutputPortDataType(S, port, output_spec->data_type);
            }
            else {
                /* Double output */
                ssSetOutputPortDataType(S, port, SS_DOUBLE);
            }

            if (output_spec->gain_count) {
                if (output_spec->gain_name) {
                    slave->runtime_param_count += output_spec->gain_count;
                } else {
                    slave->static_output_gain_count += output_spec->gain_count;
                }
            }

            if (output_spec->offset_count) {
                if (output_spec->offset_name) {
                    slave->runtime_param_count += output_spec->offset_count;
                } else {
                    slave->static_output_offset_count += 
                        output_spec->offset_count;
                }
            }

            if (output_spec->filter_count) {
                if (output_spec->filter_name) {
                    slave->runtime_param_count += output_spec->filter_count;
                } else {
                    slave->static_output_filter_count += 
                        output_spec->filter_count;
                }
                slave->output_filter_count += output_spec->port_width;
            }
        }
    }

    ssSetNumSampleTimes(S, 1);
    ssSetNumPWork(S, slave->output_map_count + slave->input_map_count);
    ssSetNumIWork(S, slave->iwork_count);
    if (slave->output_filter_count) {
        if (TSAMPLE) {
            ssSetNumDiscStates(S, slave->output_filter_count);
        } else {
            ssSetNumContStates(S, slave->output_filter_count);
        }
    }

    mexMakeMemoryPersistent(slave);
    mexMakeMemoryPersistent(slave->product_name);
    mexMakeMemoryPersistent(slave->pdo_entry_info);
    mexMakeMemoryPersistent(slave->pdo_info);
    mexMakeMemoryPersistent(slave->sdo_config);
    mexMakeMemoryPersistent(slave->output_spec);
    for (output_spec = slave->output_spec; 
            output_spec != &slave->output_spec[slave->num_outputs];
            output_spec++) {
        mexMakeMemoryPersistent(output_spec->map);
        mexMakeMemoryPersistent(output_spec->gain_values);
        mexMakeMemoryPersistent(output_spec->gain_name);
        mexMakeMemoryPersistent(output_spec->offset_values);
        mexMakeMemoryPersistent(output_spec->offset_name);
        mexMakeMemoryPersistent(output_spec->filter_values);
        mexMakeMemoryPersistent(output_spec->filter_name);
    }
    mexMakeMemoryPersistent(slave->input_spec);
    for (input_spec = slave->input_spec; 
            input_spec != &slave->input_spec[slave->num_inputs];
            input_spec++) {
        mexMakeMemoryPersistent(input_spec->map);
        mexMakeMemoryPersistent(input_spec->gain_values);
        mexMakeMemoryPersistent(input_spec->gain_name);
    }

    ssSetOptions(S, 
            SS_OPTION_WORKS_WITH_CODE_REUSE 
            | SS_OPTION_RUNTIME_EXCEPTION_FREE_CODE 
            | SS_OPTION_CALL_TERMINATE_ON_EXIT);
}

/* Function: mdlInitializeSampleTimes =========================================
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
    struct ecat_slave *slave = ssGetUserData(S);
    struct io_spec *input_spec = &slave->input_spec[port];

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
}

/* This function is called when some ports are still DYNAMICALLY_SIZED
 * even after calling mdlSetInputPortWidth(). This occurs when input
 * ports are not connected. This function sets the port width for
 * these ports to 1 */
#define MDL_SET_DEFAULT_PORT_DIMENSION_INFO
static void mdlSetDefaultPortDimensionInfo(SimStruct *S)
{
    struct ecat_slave *slave = ssGetUserData(S);
    struct io_spec *input_spec;
    int_T port;

    for (input_spec = slave->input_spec, port = 0;
            input_spec != &slave->input_spec[slave->num_inputs];
            input_spec++, port++) {
        if (ssGetInputPortWidth(S, port) == DYNAMICALLY_SIZED) {
            ssSetInputPortWidth(S, port, 1);
        }
    }
}

#define MDL_SET_WORK_WIDTHS
static void mdlSetWorkWidths(SimStruct *S)
{
    struct ecat_slave *slave = ssGetUserData(S);
    struct io_spec *output_spec;
    struct io_spec *input_spec;
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
}

/* Function: mdlOutputs =======================================================
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

/* Function: mdlTerminate =====================================================
 * Abstract:
 *    In this function, you should perform any actions that are necessary
 *    at the termination of a simulation.  For example, if memory was
 *    allocated in mdlStart, this is the place to free it.
 */
static void mdlTerminate(SimStruct *S)
{
    struct ecat_slave *slave = ssGetUserData(S);
    struct io_spec *output_spec;
    struct io_spec *input_spec;

    for (input_spec = slave->input_spec; 
            input_spec != &slave->input_spec[slave->num_inputs]; 
            input_spec++) {
        mxFree(input_spec->gain_values);
        mxFree(input_spec->gain_name);
        mxFree(input_spec->map);
    }
    for (output_spec = slave->output_spec; 
            output_spec != &slave->output_spec[slave->num_outputs]; 
            output_spec++) {
        mxFree(output_spec->filter_values);
        mxFree(output_spec->filter_name);
        mxFree(output_spec->offset_values);
        mxFree(output_spec->offset_name);
        mxFree(output_spec->gain_values);
        mxFree(output_spec->gain_name);
        mxFree(output_spec->map);
    }
    mxFree(slave->output_spec);
    mxFree(slave->sdo_config);
    mxFree(slave->pdo_info);
    mxFree(slave->pdo_entry_info);
    mxFree(slave->product_name);
    mxFree(slave);
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
    struct io_spec *output_spec;
    struct io_spec *input_spec;
    uint_T gain_idx;
    uint_T offset_idx;
    uint_T filter_idx;

    uint32_T input_port_spec[4][slave->num_inputs];
    uint32_T input_map[2][slave->input_map_count];
    const char_T *input_gain_str;
    const char_T *input_gain_name_ptr[slave->num_inputs + 1];
    real_T input_gain[slave->static_input_gain_count];

    uint32_T output_port_spec[6][slave->num_outputs];
    uint32_T output_map[2][slave->output_map_count];
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
    for (input_spec = slave->input_spec, port = 0;
            input_spec != &slave->input_spec[slave->num_inputs];
            input_spec++, port++) {
        struct io_spec_map *map;

        input_port_spec[0][port] = input_spec->pdo_data_type;
        input_port_spec[1][port] = input_spec->raw;
        input_port_spec[2][port] = input_spec->full_scale_bits;
        input_port_spec[3][port] = input_spec->gain_count;
        input_gain_name_ptr[port] = input_spec->gain_count
            ? input_spec->gain_name : "";

        for (i = 0; !input_spec->gain_name && i < input_spec->gain_count;
                i++) {
            input_gain[gain_idx++] = input_spec->gain_values[i];
        }

        for (map = input_spec->map; 
                map != &input_spec->map[input_spec->port_width]; 
                map++, map_idx++) {
            input_map[0][map_idx] = map->pdo_info_idx;
            input_map[1][map_idx] = map->pdo_entry_idx;
        }
    }
    input_gain_name_ptr[slave->num_inputs] = NULL;
    input_gain_str = createStrVect(input_gain_name_ptr);

    gain_idx = offset_idx = filter_idx = map_idx = 0;
    for (output_spec = slave->output_spec, port = 0; 
            output_spec != &slave->output_spec[slave->num_outputs]; 
            output_spec++, port++) {
        struct io_spec_map *map;

        output_port_spec[0][port] = output_spec->pdo_data_type;
        output_port_spec[1][port] = output_spec->raw;
        output_port_spec[2][port] = output_spec->full_scale_bits;
        output_port_spec[3][port] = output_spec->gain_count > 0;
        output_port_spec[4][port] = output_spec->offset_count > 0;
        output_port_spec[5][port] = output_spec->filter_count > 0;
        output_gain_name_ptr[port] = output_spec->gain_count
            ? output_spec->gain_name : "";
        output_offset_name_ptr[port] = output_spec->offset_count
            ? output_spec->offset_name : "";
        output_filter_name_ptr[port] = output_spec->filter_count
            ? output_spec->filter_name : "";

        for (i = 0; !output_spec->gain_name && i < output_spec->gain_count;
                i++) {
            output_gain[gain_idx++] = output_spec->gain_values[i];
        }
        for (i = 0; !output_spec->offset_name && i < output_spec->offset_count;
                i++) {
            output_offset[offset_idx++] = output_spec->offset_values[i];
        }
        for (i = 0; !output_spec->filter_name && i < output_spec->filter_count;
                i++) {
            output_filter[filter_idx++] = output_spec->filter_values[i];
        }

        for (map = output_spec->map;
                map != &output_spec->map[output_spec->port_width]; 
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

    { /* Transpose slave->sdo_config */
        uint32_T sdo_config[4][slave->sdo_config_len];
        uint_T i, j;

        for (i = 0; i < slave->sdo_config_len; i++) {
            for (j = 0; j < 4; j++) {
                sdo_config[j][i] = slave->sdo_config[i][j];
            }
        }

        if (slave->sdo_config_len) {
            if (!ssWriteRTW2dMatParam(S, "SdoConfig", sdo_config, 
                        SS_UINT32, slave->sdo_config_len, 4))
                return;
        }
    }
    { /* Transpose slave->pdo_entry_info */
        uint32_T pdo_entry_info[3][slave->pdo_entry_info_len];
        uint_T i;

        for (i = 0; i < slave->pdo_entry_info_len; i++) {
            pdo_entry_info[0][i] = slave->pdo_entry_info[i].index;
            pdo_entry_info[1][i] = slave->pdo_entry_info[i].subindex;
            pdo_entry_info[2][i] = abs(slave->pdo_entry_info[i].bitlen);
        }

        if (!ssWriteRTW2dMatParam(S, "PdoEntryInfo", pdo_entry_info, 
                    SS_UINT32, slave->pdo_entry_info_len, 3))
            return;
    }
    { /* Transpose slave->pdo_info */
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
