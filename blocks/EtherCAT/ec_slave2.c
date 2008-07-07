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

enum {
    ADDRESS = 0,
    ETHERCAT_INFO,
    LAYOUT_VERSION,
    SDO_CONFIG,
    PORT_SPEC,
    TSAMPLE,
    PARAM_COUNT
};

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
    } *sdo_config, *sdo_config_end;

    struct pdo {
        /* Pdo must be written to SlaveConfig */
        boolean_T exclude_from_config;
        unsigned int configured;

        uint16_t index;

        struct sync_manager *sm;

        boolean_T mandatory;
        uint_T *exclude, *exclude_end;
        struct pdo_entry {
            uint16_t index;
            uint8_t subindex;
            uint_T bitlen;
            int_T datatype;
        } *entry, *entry_end;
        unsigned int entry_count;
    };

    uint_T pdo_entry_count;

    struct sync_manager {
        uint16_t index;

        /* A list of pointers to pdo */
        struct pdo **pdo_ptr, **pdo_ptr_end;
    };

    struct pdo *tx_pdo, *tx_pdo_end;
    struct pdo *rx_pdo, *rx_pdo_end;
    struct sync_manager * input_sm, * input_sm_end;
    struct sync_manager *output_sm, *output_sm_end;
    /* FIXME: Move above entries into */
    struct {
        struct pdo *pdo, *pdo_end;
        struct sync_manager *sm, *sm_end;
    } input, output;

    /* Maximum port width of both input and output ports. This is a 
     * temporary value that is used to check whether the Simulink block
     * has mixed port widths */
    uint_T max_port_width;

    /* Set if the Simulink block has mixed port widths */
    boolean_T have_mixed_port_widths;

    struct io_port {
        uint_T direction;

        struct pdo_entry **pdo_entry;
        uint_T pdo_entry_count;

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
    } *io_port;
    uint_T io_port_count;

    /* Number of input and output ports */
    uint_T input_port_count;

    uint_T input_pdo_entry_count;
    uint_T output_pdo_entry_count;

    /* Save the index into slave->io_port[idx] for the inputs and outputs */
    struct io_port **input_port_ptr;
    struct io_port **output_port_ptr;

    /* IWork is needed to store the bit offsets when and IO requires
     * bit operations */
    uint_T iwork_count;

    /* PWork is used to store the address of the data */
    uint_T pwork_count;
    
    /* Runtime parameters are used to store parameters where a name was
     * supplied for gain_name, offset_name and filter_name. These
     * parameters are exported in the C-API */
    uint_T runtime_param_count;

    /* RWork  used to store parameters where no name was
     * supplied for gain_name, offset_name and filter_name. These
     * parameters are not exported in the C-API */
    uint_T rwork_count;

    uint_T filter_count;
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
        const mxArray *src, uint_T idx, boolean_T zero_allowed,
        const char_T *field_name, real_T *dest)
{
    const mxArray *field = mxGetField(src, idx, field_name);

    *dest = 0.0;

    if (!field) {
        err_no_num_field(S, variable, field_name);
        return 1;
    }
    if (!mxIsNumeric(field)) {
        printf("Expecting class double fot field %s.%s, but it has "
                "type %s\n", variable, field_name, mxGetClassName(field));
        err_no_num_field(S, variable, field_name);
        return 2;
    }

    *dest = mxGetScalar(field);

    if (!zero_allowed && !*dest) {
        err_no_zero_field(S, variable, field_name);
        return 3;
    }

    return 0;
}

#define CHECK_CALLOC(S,n,m,dest) \
    do {                                                                \
        if (!(n)) {                                                     \
                dest = NULL;                                            \
                break;                                                  \
        }                                                               \
        dest = mxCalloc((n),(m));                                       \
        if (!(dest)) {                                                  \
            ssSetErrorStatus((S), "calloc() failure; no memory left."); \
            return -1;                                                  \
        }                                                               \
    } while(0)

int_T
get_string_field(SimStruct *S, const char_T *variable,
        const mxArray *src, uint_T idx, 
        const char_T *field_name, 
        const char_T *dflt, /* << Default string if the Matlab string
                                  is not supplied or '' (empty). Return
                                  value is 0 in this case.
                                  Setting default to NULL or "" will cause
                                  the function to return -1 if the Matlab
                                  string is not available or ''.
                                  Setting default to (char_T*)1 will make
                                  this function return 0 with *dest = NULL
                                  */
        char_T **dest)
{
    const mxArray *field = mxGetField(src, idx, field_name);
    uint_T len = 0;

    if (!field || !mxIsChar(field)) {
        goto not_available;
    }

    len = mxGetN(field);

    if (!len) {
        goto not_available;
    }
    len++;

    CHECK_CALLOC(S,len,1,*dest);

    if (mxGetString(field, *dest, len)) {
        return err_no_str_field(S, variable, field_name);
    }

    return 0;

not_available:
    if (dflt == (char_T*)1)
        return 0;
    if (!dflt || !(len = strlen(dflt))) {
        *dest = NULL;
        if (!len) {
            return err_no_str_field(S, variable, field_name);
        }
        else {
            return err_no_empty_string_field(S, variable, field_name);
        }
    }
    len++;
    CHECK_CALLOC(S,len,1,*dest);
    strcpy(*dest, dflt);
    return 0;
}

#define RETURN_ON_ERROR(val)    \
    do {                        \
        int_T err = val;        \
        if (err < 0)            \
            return err;         \
    } while(0)

int_T
get_slave_address(SimStruct *S, struct ecat_slave *slave)
{
    const mxArray *address       = ssGetSFcnParam(S,ADDRESS);
    real_T val;

    RETURN_ON_ERROR(get_numeric_field(S, ADDRESS, address, 0, 1,
                "Master", &val));
    slave->master = val;
    RETURN_ON_ERROR(get_numeric_field(S, ADDRESS, address, 0, 1,
                "Domain", &val));
    slave->domain = val;
    RETURN_ON_ERROR(get_numeric_field(S, ADDRESS, address, 0, 1,
                "Alias", &val));
    slave->alias = val;
    RETURN_ON_ERROR(get_numeric_field(S, ADDRESS, address, 0, 1,
                "Position", &val));
    slave->position = val;
    return 0;
}

int_T
get_sync_manager(SimStruct *S, const char_T *context, const mxArray *ec_info,
        const char_T *sm_dir_spec,
        struct sync_manager **sm_ptr,
        struct sync_manager **sm_end_ptr )
{
    const mxArray *sm_info, *dir_sm_info;
    uint_T sm_count = 0, i;
    real_T *val = NULL;

    sm_info = mxGetField(ec_info, 0, "Sm");
    if (!sm_info)
        return 0;

    dir_sm_info = mxGetField(sm_info, 0, sm_dir_spec);
    if (!dir_sm_info) {
        printf("Warning: field Sm.%s is missing.\n", sm_dir_spec);
        return 0;
    }

    sm_count = mxGetNumberOfElements(dir_sm_info);
    if (!sm_count)
        return 0;

    val = mxGetPr(dir_sm_info);
    if (!mxIsNumeric(dir_sm_info) || !val) {
        snprintf(errmsg, sizeof(errmsg),
                "SFunction Parameter %s.%s is not a valid numeric vector.",
                context, sm_dir_spec);
        ssSetErrorStatus(S, errmsg);
        return -1;
    }

    /* Count the number of fields */
    CHECK_CALLOC(S,sm_count, sizeof(struct sync_manager), *sm_ptr);
    *sm_end_ptr = *sm_ptr;

    for (i = 0; i < sm_count; i++, (*sm_end_ptr)++) {
        (*sm_end_ptr)->index = *val++;
    }

    printf("%s SyncManagers:", sm_dir_spec);
    for (i = 0; i < sm_count; i++)
        printf(" %u", (*sm_ptr)[i]);
    printf("\n");
    
    return 0;
}

int_T
get_slave_pdo(SimStruct *S, const char_T *param, const mxArray *ec_info, 
        const char_T *field, 
        struct sync_manager *sm_start, struct sync_manager *sm_end,
        struct pdo **pdo_ptr, struct pdo **pdo_end_ptr)
{
    const mxArray *pdo_info;
    struct pdo *pdo;
    uint_T pdo_count = 0;
    uint_T sm_count = sm_end - sm_start;
    char_T context[100];
    real_T val;
    uint_T pdo_idx;
    uint_T i;

    pdo_info = mxGetField(ec_info, 0, field);
    if (!pdo_info || !(pdo_count = mxGetNumberOfElements(pdo_info)))
        return 0;

    CHECK_CALLOC(S, pdo_count, sizeof(struct pdo), pdo);
    *pdo_ptr = pdo;
    *pdo_end_ptr = pdo + pdo_count;

    for (pdo_idx = 0, pdo = *pdo_ptr; 
            pdo_idx < pdo_count; pdo_idx++, pdo++) {
        struct sync_manager *sm;
        const mxArray *exclude_info;
        uint_T exclude_count;
        const mxArray *pdo_entry_info;
        uint_T entry_info_count;

        snprintf(context, sizeof(context), "%s(%u)", field, pdo_idx+1);

        RETURN_ON_ERROR(get_numeric_field(S, context, pdo_info, pdo_idx, 1,
                    "Index", &val));
        pdo->index = val;

        exclude_info = mxGetField(pdo_info, pdo_idx, "Exclude");
        exclude_count = 
            exclude_info ? mxGetNumberOfElements(exclude_info) : 0;
        if (exclude_count) {
            real_T *val = mxGetPr(exclude_info);

            if (!mxIsNumeric(exclude_info) || !val) {
                snprintf(errmsg, sizeof(errmsg),
                        "Field %s.Exclude is not a valid numeric array.",
                        context);
                ssSetErrorStatus(S, errmsg);
                return -1;
            }

            CHECK_CALLOC(S, exclude_count, sizeof(uint_T),
                    pdo->exclude);
            pdo->exclude_end = pdo->exclude + exclude_count;

            for (i = 0; i < exclude_count; i++) {
                pdo->exclude[i] = val[i];
            }
        }

        /* Get the SyncManager that this pdo is mapped to.
         * Not specifying the SyncManager is not a good idea, 
         * Although it is possible to assume the right SyncManager if
         * there is only one to choose from. */
        switch (get_numeric_field(S, context, pdo_info, pdo_idx, 1,
                    "Sm", &val)) {
            case 0:
                if (val < 0.0) {
                    printf("Warning: %s.Sm was not valid (= %f): ");
                    if (sm_count == 1) {
                        printf( "Since there is only 1 SyncManager to "
                                "choose from, it will be used.\n",
                                context, val);
                        pdo->sm = sm_start;
                    }
                    else {
                        printf("Skipping SyncManager assignment\n");
                        pdo->sm = NULL;
                        break;
                    }
                }

                for (sm = sm_start; sm != sm_end; sm++) {
                    if (sm->index == (uint16_t)val) {
                        pdo->sm = sm;
                        break;
                    }
                }
                if (!pdo->sm) {
                    snprintf(errmsg, sizeof(errmsg),
                            "SyncManager index %f as specified in %s.Sm "
                            "does not exist.", val, context);
                    ssSetErrorStatus(S, errmsg);
                    return -1;
                }
                break;
            case 1:
                /* Field was not found */
                if (sm_count != 1) {
                    return -1;
                }
                printf("Warning: %s.Sm was not specified. Since there is "
                        "only 1 SyncManager to choose from, it is used.\n",
                        context);
                ssSetErrorStatus(S, NULL);
                pdo->sm = sm_start;
                break;
            default:
                return -1;
        }

        if (pdo->sm)
            pdo->sm->pdo_ptr_end++;

        switch (get_numeric_field(S, context, pdo_info, pdo_idx, 1,
                    "Mandatory", &val)) {
            case 0:
                pdo->mandatory = val;
                break;
            case 1:
                pdo->mandatory = 0;
                ssSetErrorStatus(S,NULL);
                break;
            default:
                return -1;
        }

        pdo_entry_info = mxGetField(pdo_info, pdo_idx, "Entry");
        entry_info_count = 
            pdo_entry_info ? mxGetNumberOfElements(pdo_entry_info) : 0;
        if (entry_info_count) {
            struct pdo_entry *pdo_entry;
            uint_T entry_idx;

            snprintf(context, sizeof(context),
                    "%s(%u).Entry", field, pdo_idx+1); 

            if (!mxIsStruct(pdo_entry_info)) {
                snprintf(errmsg, sizeof(errmsg),
                        "Field %s is not a Matlab structure.", context);
                ssSetErrorStatus(S, errmsg);
                return -1;
            }

            CHECK_CALLOC(S, entry_info_count, sizeof(struct pdo_entry), 
                    pdo_entry);
            pdo->entry     = pdo_entry;
            pdo->entry_end = pdo_entry + entry_info_count;

            for (entry_idx = 0; entry_idx < entry_info_count; 
                    entry_idx++, pdo_entry++) {
                int_T dummy_datatype;

                snprintf(context, sizeof(context), "%s(%u).Entry(%u)", 
                        field, pdo_idx+1, entry_idx+1); 

                RETURN_ON_ERROR(get_numeric_field(S, context, 
                            pdo_entry_info, entry_idx, 1, "Index", &val));
                pdo_entry->index = val;
                RETURN_ON_ERROR(get_numeric_field(S, context, 
                            pdo_entry_info, entry_idx, 1, "BitLen", &val));
                pdo_entry->bitlen = val;

                if (!pdo_entry->index)
                    continue;

                RETURN_ON_ERROR(get_numeric_field(S, context, 
                            pdo_entry_info, entry_idx, 0, "SubIndex", &val));
                pdo_entry->subindex = val;

                RETURN_ON_ERROR(get_numeric_field(S, context, 
                            pdo_entry_info, entry_idx, 0, "DataType", &val));
                pdo_entry->datatype = val;
                RETURN_ON_ERROR(get_data_type(S, context, "DataType",
                            pdo_entry->datatype, &dummy_datatype, 0, 0));
                /*
                switch(get_numeric_field(S, variable,
                            pdo_entry, entry_idx, 1, "DataType", &val)) {
                    case 0:
                        entry->datatype = val;
                        break;
                    case 1:
                        entry->datatype = entry->bitlen;
                    default:
                        return -1;
                }
                */
            }
        }
    }

    for (pdo = *pdo_ptr; pdo != *pdo_end_ptr; pdo++) {
        struct pdo_entry *pdo_entry;

        printf("%s Index #x%04X Sm %u Mandatory %u: \n",
                field, pdo->index, pdo->sm->index, pdo->mandatory);
        if (pdo->exclude) {
            uint_T *excl;

            printf("\t Exclude: ");
            for (excl = pdo->exclude; excl != pdo->exclude_end; excl++) {
                printf(" #x%04X", *excl);
            }
            printf("\n");
        }

        for (pdo_entry = pdo->entry; pdo_entry != pdo->entry_end; 
                pdo_entry++) {
            printf("\t Entry Index #x%04X Subindex %u, "
                    "BitLen %u, DataType %u\n",
                    pdo_entry->index, pdo_entry->subindex,
                    pdo_entry->bitlen, pdo_entry->datatype);
        }
        if (pdo->entry == pdo->entry_end) {
            printf("\t has no Entries\n");
        }
    }


    return 0;
}

int_T
get_ethercat_info(SimStruct *S, struct ecat_slave *slave)
{
    const mxArray *ec_info = ssGetSFcnParam(S,ETHERCAT_INFO);
    struct sync_manager *sm;
    struct pdo *pdo;
    const char_T *param = "ETHERCAT_INFO";
    real_T val;

    if (!mxIsStruct(ec_info))
        return 0;

    /***********************
     * Get Slave address
     ***********************/
    RETURN_ON_ERROR(get_numeric_field(S, param, ec_info, 0, 0,
                "VendorId", &val));
    slave->vendor_id = val;
    RETURN_ON_ERROR(get_numeric_field(S, param, ec_info, 0, 1,
                "RevisionNo", &val));
    slave->revision_no = val;
    RETURN_ON_ERROR(get_numeric_field(S, param, ec_info, 0, 0,
                "ProductCode", &val));
    slave->product_code = val;
    RETURN_ON_ERROR(get_string_field(S, param, ec_info, 0,
                "Type", "Unspecified", &slave->type));

    /***********************
     * Get SyncManager setup
     ***********************/
    RETURN_ON_ERROR(get_sync_manager(S, param, ec_info, "Input",
                &slave->input_sm, &slave->input_sm_end));
    RETURN_ON_ERROR(get_sync_manager(S, param, ec_info, "Output",
                &slave->output_sm, &slave->output_sm_end));

    /***********************
     * Pdo Setup
     ***********************/
    RETURN_ON_ERROR(get_slave_pdo(S, param, ec_info, "TxPdo",
                slave->input_sm, slave->input_sm_end,
                &slave->tx_pdo, &slave->tx_pdo_end));
    RETURN_ON_ERROR(get_slave_pdo(S, param, ec_info, "RxPdo",
                slave->output_sm, slave->output_sm_end,
                &slave->rx_pdo, &slave->rx_pdo_end));

    /* While reading in all the PDO's, the pdo_ptr_end of the corresponding
     * SyncManager was incremented as well. Now that it is known how
     * many PDO's a SyncManager has, memory for a set of PDO pointers
     * can be allocated. */
    for (sm = slave->input_sm; sm != slave->input_sm_end; sm++) {
        uint_T pdo_count = sm->pdo_ptr_end - sm->pdo_ptr;

        CHECK_CALLOC(S, pdo_count, sizeof(struct pdo*), sm->pdo_ptr);
        sm->pdo_ptr_end = sm->pdo_ptr;
    }
    for (sm = slave->output_sm; sm != slave->output_sm_end; sm++) {
        uint_T pdo_count = sm->pdo_ptr_end - sm->pdo_ptr;

        CHECK_CALLOC(S, pdo_count, sizeof(struct pdo*), sm->pdo_ptr);
        sm->pdo_ptr_end = sm->pdo_ptr;
    }

    /* Now go through the tx_pdo and rx_pdo list again and setup the 
     * SyncManager's pdo pointers */
    for (pdo = slave->tx_pdo; pdo != slave->tx_pdo_end; pdo++) {
        if (pdo->sm) {
            *pdo->sm->pdo_ptr_end++ = pdo;
        }
    }
    for (pdo = slave->rx_pdo; pdo != slave->rx_pdo_end; pdo++) {
        if (pdo->sm) {
            *pdo->sm->pdo_ptr_end++ = pdo;
        }
    }

    return 0;

#if 0
    for (i = 0; i < slave->sync_manager_count; i++) {
        const mxArray *smX = mxGetFieldByNumber(sm_field, 0, i);
        const mxArray *pdoX;
        struct pdo *pdo;
        uint_T sm_index;
        uint_T pdo_idx;

        snprintf(variable, sizeof(variable), 
                "%s.SyncManager(%u)", param, i+1);

        if (!smX) {
            snprintf(errmsg, sizeof(errmsg),
                    "Matlab internal error: SFunction parameter %s "
                    "does not exist.", variable);
            ssSetErrorStatus(S,errmsg);
            return -1;
        }

        RETURN_ON_ERROR(get_numeric_field(S, variable, smX, 0, 1,
                    "SmIndex", &val));
        sm_index = val;

        sm = slave->sync_manager; 
        do {
            if (sm == &slave->sync_manager[slave->sync_manager_count]) {
                snprintf(errmsg, sizeof(errmsg),
                        "Internal error: index overrun while looking for "
                        "a free SyncManager index %u as specified in "
                        "%s.SmIndex.", sm_index, variable);
                ssSetErrorStatus(S,errmsg);
                return -1;
            }
            else if (sm->index == sm_index) {
                snprintf(errmsg, sizeof(errmsg),
                        "Error while processing %s.SmIndex (= %u): "
                        "Cannot multiply specify SyncManager Indices",
                        variable, sm_index);
                ssSetErrorStatus(S,errmsg);
                return -1;
            }
            else if (sm->index == (uint16_T)~0U) {
                break;
            }
        } while(sm++);
        sm->index = sm_index;

        RETURN_ON_ERROR(get_numeric_field(S, variable, smX, 0, 1,
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

            RETURN_ON_ERROR(get_numeric_field(S, variable,  pdoX, 
                        pdo_idx, 0, "Index", &val));
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

                RETURN_ON_ERROR(get_numeric_field(S, variable,
                            pdo_entry, entry_idx, 0, "BitLen", &val));
                entry->bitlen = val;

                RETURN_ON_ERROR(get_numeric_field(S, variable,
                            pdo_entry, entry_idx, 1, "Index", &val));
                entry->index = val;

                if (!entry->index)
                    continue;

                RETURN_ON_ERROR(get_numeric_field(S, variable,
                            pdo_entry, entry_idx, 0, "SubIndex", &val));
                entry->subindex = val;

                switch(get_numeric_field(S, variable,
                            pdo_entry, entry_idx, 1, "DataType", &val)) {
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
            }
            slave->pdo_entry_count += pdo->entry_count;
        }
        slave->pdo_count += sm->pdo_count;
    }
#endif

    return 0;
}

int_T
get_sdo_config(SimStruct *S, struct ecat_slave *slave)
{
    const mxArray *sdo_config = ssGetSFcnParam(S,SDO_CONFIG);
    struct sdo_config *sdo_config_ptr;
    const char_T *param = "SDO_CONFIG";
    char_T context[100];
    uint_T i;
    uint_T sdo_config_count;

    sdo_config_count = mxGetM(sdo_config);

    CHECK_CALLOC(S,sdo_config_count, sizeof(*slave->sdo_config),
            slave->sdo_config);
    slave->sdo_config_end = slave->sdo_config + sdo_config_count;

    for (i = 0, sdo_config_ptr = slave->sdo_config; 
            i < sdo_config_count; i++, sdo_config_ptr++) {
        real_T val;
        snprintf(context, sizeof(context),
                "%s(%u)", param, i+1);

        RETURN_ON_ERROR(get_numeric_field(S, context,
                    sdo_config, i, 0, "Index", &val));
        sdo_config_ptr->index = val;

        RETURN_ON_ERROR(get_numeric_field(S, context,
                    sdo_config, i, 1, "SubIndex", &val));
        sdo_config_ptr->subindex = val;

        RETURN_ON_ERROR(get_numeric_field(S, context,
                    sdo_config, i, 0, "BitLen", &val));
        RETURN_ON_ERROR(get_data_type(S, context, "BitLen",
                    val, &sdo_config_ptr->datatype, 1, 1));

        RETURN_ON_ERROR(get_numeric_field(S, context,
                    sdo_config, i, 1, "Value", &val));
        sdo_config_ptr->value = val;
    }

    return 0;
}

int_T
find_pdo_entry(const struct ecat_slave *slave,
        uint16_T pdo_index,
        uint16_T pdo_entry_index, uint8_T pdo_entry_subindex, 
        struct pdo **pdo_ptr, struct pdo *pdo_end, 
        struct pdo_entry **pdo_entry_ptr)
{
    for (; *pdo_ptr != pdo_end; (*pdo_ptr)++) {
        if (pdo_index && (*pdo_ptr)->index != pdo_index)
            continue;

        if ((*pdo_ptr)->exclude_from_config) {
            if (pdo_index) {
                printf("Looking for a PdoEntry in PDO Index #x%04X that "
                        "was explicitly excluded\n");
            }
            continue;
        }

        for (*pdo_entry_ptr = (*pdo_ptr)->entry;
                *pdo_entry_ptr != (*pdo_ptr)->entry_end; 
                (*pdo_entry_ptr)++) {
            if ((*pdo_entry_ptr)->index == pdo_entry_index 
                    && (*pdo_entry_ptr)->subindex == pdo_entry_subindex) {
                return 0;
            }
        }
    }
    *pdo_ptr = NULL;
    *pdo_entry_ptr = NULL;
    return -1;
#if 0
    const struct sync_manager *sm; 

    /*
    printf("Looking for %04x %u, dir %u\n", 
            pdo_entry_index, pdo_entry_subindex, port_direction);
    */
    for (sm = slave->sync_manager; 
            sm != &slave->sync_manager[slave->sync_manager_count];
            sm++) {
        struct pdo *pdo; 

        /* If port_direction_ptr is not set, make sure that port direction
         * and sync-manager direction are not the same
         * Take care, sync-manager direction 1 means input for the master,
         * corresponding to a TxPdo, connected to s Simulink 
         * output port (port_direction = 0). */
        if (!port_direction_ptr && 
                (port_direction && sm->direction == EC_DIR_INPUT)) {
    /*
            printf("Wrong direction\n");
    */
            continue;
        }


        for (pdo = sm->pdo; pdo != &sm->pdo[sm->pdo_count]; pdo++) {
            for (*pdo_entry = pdo->entry; 
                    *pdo_entry != &pdo->entry[pdo->entry_count]; 
                    (*pdo_entry)++) {
    /*
                printf("Found   for %04x %u, dir %u\n", 
                        (*pdo_entry)->index, (*pdo_entry)->subindex, 
                        sm->direction);
    */
                if ((*pdo_entry)->index == pdo_entry_index
                        && (*pdo_entry)->subindex == pdo_entry_subindex) {
                    if (port_direction_ptr)
                        *port_direction_ptr = 
                            sm->direction == EC_DIR_INPUT ? 0 : 1;
                    return 0;
                }
            }
        }
    }

    /*
    printf("Not found.........\n");
    */

    *pdo_entry = NULL;
    return -1;
#endif
}

int_T
get_parameter_values(SimStruct* S, const char_T *parent_variable,
        const mxArray* port_spec, uint_T port,
        const char_T* field, uint_T port_width, real_T **dest,
        char_T **name)
{
    const mxArray* src = mxGetField(port_spec, port, field);
    char_T variable_context[100];
    char_T name_field[sizeof(field) + 10];
    uint_T count;
    real_T *values;

    snprintf(variable_context, sizeof(variable_context), 
            "%s.%s", parent_variable, field);

    if (!src || !(count = mxGetNumberOfElements(src)))
        return 0;

    if (!mxIsNumeric(src)) {
        snprintf(errmsg, sizeof(errmsg),
                "SFunction parameter %s not a numeric vector.",
                variable_context);
        ssSetErrorStatus(S, errmsg);
        return -1;
    }

    if (count != 1 && count != port_width) {
        snprintf(errmsg, sizeof(errmsg),
                "SFunction parameter %s does not match the port size.\n"
                "It can either be a scalar or a vector with %u elements.",
                variable_context, port_width);
        ssSetErrorStatus(S, errmsg);
        return -1;
    }

    *dest = mxCalloc(count, sizeof(real_T));
    if (!*dest) {
        snprintf(errmsg, sizeof(errmsg),
                "Ran out of memory while processing "
                "SFunction parameter %s.\n",
                variable_context);
        ssSetErrorStatus(S, errmsg);
        return -1;
    }

    values = mxGetPr(src);
    if (!values) {
        snprintf(errmsg, sizeof(errmsg),
                "An internal error occurred while processing "
                "SFunction parameter %s.\n"
                "Expecting a numeric scalar or vector with %u elements.",
                variable_context, port_width);
        ssSetErrorStatus(S, errmsg);
        return -1;
    }

    memcpy(*dest, values, sizeof(real_T)*count);

    snprintf(name_field, sizeof(name_field), "%sName", field);

    RETURN_ON_ERROR(get_string_field(S, parent_variable, 
                port_spec, port, name_field, (char_T*)1, name));

    return count;
}

int_T
get_ioport_config(SimStruct *S, struct ecat_slave *slave)
{
    const mxArray * const port_spec = ssGetSFcnParam(S, PORT_SPEC);
    const char_T *param = "IO_SPEC";

    struct io_port *port;
    char_T variable[100];
    real_T val;
    uint_T io_spec_idx, io_spec_count;

    if (!port_spec || 
            !(io_spec_count = mxGetNumberOfElements(port_spec)))
        return 0;
    CHECK_CALLOC(S, io_spec_count, sizeof(struct io_port), 
            slave->io_port);

    port = slave->io_port;

    for (io_spec_idx = 0; io_spec_idx < io_spec_count; io_spec_idx++) {
        const mxArray *pdo_entry_spec = 
            mxGetField(port_spec, io_spec_idx, "PdoEntry");
        struct pdo_entry **pdo_entry_ptr;
        struct pdo **pdo_ptr, *pdo_end;
        uint_T pdo_entry_idx;
        boolean_T struct_access;
        real_T *pdo_entry_index_field;
        real_T *pdo_entry_subindex_field;
        uint_T pdo_data_type;
        const char_T *pdo_direction_string;

        snprintf(variable, sizeof(variable), 
                "%s(%u).PdoEntry", param, io_spec_idx+1);

        if (!pdo_entry_spec) {
            snprintf(errmsg, sizeof(errmsg),
                    "SFunction parameter %s does not exist.", variable);
            ssSetErrorStatus(S,errmsg);
            return -1;
        }

        /* The field 'PdoEntry' is allowed to be a struct or a matrix.
         * Check what was specified */
        if (mxIsStruct(pdo_entry_spec)) {
            /* Mapped pdos is defined using a structure array with
             * the fields 'Index', 'EntryIndex' and 'EntrySubIndex' */
            struct_access = 1;

            port->pdo_entry_count = mxGetNumberOfElements(pdo_entry_spec);
            if (!port->pdo_entry_count) {
                snprintf(errmsg, sizeof(errmsg),
                        "SFunction parameter %s is empty.", variable);
                ssSetErrorStatus(S,errmsg);
                return -1;
            }

            /* Stop the compiler from complaining */
            pdo_entry_index_field = pdo_entry_subindex_field = NULL;
        }
        else if (mxGetNumberOfElements(pdo_entry_spec)) {
            /* Mapped pdos is defined using a M-by-2 array, where
             * the columns define the 'EntryIndex' and 
             * 'EntrySubIndex' */
            struct_access = 0;

            port->pdo_entry_count = mxGetM(pdo_entry_spec);

            pdo_entry_index_field = mxGetPr(pdo_entry_spec);
            pdo_entry_subindex_field = 
                pdo_entry_index_field + port->pdo_entry_count;

            if ( !mxIsNumeric(pdo_entry_spec) 
                    || !port->pdo_entry_count
                    || mxGetN(pdo_entry_spec) != 2 
                    || !pdo_entry_index_field) {
                snprintf(errmsg, sizeof(errmsg),
                        "SFunction parameter %s is not a M-by-2 "
                        "numeric array.", variable);
                ssSetErrorStatus(S,errmsg);
                return -1;
            }
        }
        else {
            continue;
        }

        if (port->direction) {
            pdo_ptr = &slave->rx_pdo;
            pdo_end = slave->rx_pdo_end;
            pdo_direction_string = "TxPdo";
        }
        else {
            pdo_ptr = &slave->tx_pdo;
            pdo_end = slave->tx_pdo_end;
            pdo_direction_string = "RxPdo";
        }

        CHECK_CALLOC(S, port->pdo_entry_count, sizeof(struct pdo_entry*),
                port->pdo_entry);

        snprintf(variable, sizeof(variable), "%s(%u)", param, io_spec_idx+1);

        /* Get the list of pdo entries that are mapped to this port */
        for (pdo_entry_idx = 0, pdo_entry_ptr = port->pdo_entry; 
                pdo_entry_idx < port->pdo_entry_count; 
                pdo_entry_idx++, pdo_entry_ptr++) {
            uint16_T pdo_index = 0;
            uint16_T pdo_entry_index;
            uint8_T pdo_entry_subindex;
            char_T pdo_variable[100];

            snprintf(pdo_variable, sizeof(pdo_variable), 
                    "%s(%u).PdoEntry(%u)", 
                    param, io_spec_idx+1, pdo_entry_idx+1);

            if (struct_access) {
                RETURN_ON_ERROR(get_numeric_field(S, pdo_variable, 
                            pdo_entry_spec, pdo_entry_idx, 0, 
                            "PdoEntryIndex", &val));
                pdo_entry_index = val;

                RETURN_ON_ERROR(get_numeric_field(S, pdo_variable, 
                            pdo_entry_spec, pdo_entry_idx, 0, 
                            "PdoEntrySubIndex", &val));
                pdo_entry_subindex = val;
            }
            else {
                pdo_entry_index    = *pdo_entry_index_field++;
                pdo_entry_subindex = *pdo_entry_subindex_field++;
            }

            /* Get the pdo and pdo entry pointers for the specified 
             * indices */
    /*
            printf("%s pdo_entry_index %x direction %u\n", 
                    slave->type,
                    pdo_entry_index, port->direction);
    */

            if (find_pdo_entry(slave, pdo_index, 
                        pdo_entry_index, pdo_entry_subindex, 
                        pdo_ptr, pdo_end, pdo_entry_ptr)) {
                if (pdo_entry_index) {
                    snprintf(errmsg, sizeof(errmsg),
                            "Cannot find Pdo Entry specified by "
                            "SFunction parameter %s.\n"
                            "Looking for Pdo direction = %s, "
                            "Pdo Entry Index = 0x%04X, "
                            "Pdo Entry SubIndex = %u.",
                            pdo_variable, pdo_direction_string,
                            pdo_entry_index, pdo_entry_subindex);
                }
                else {
                    snprintf(errmsg, sizeof(errmsg),
                            "Cannot find Pdo Entry specified by "
                            "SFunction parameter %s.\n"
                            "Looking for Pdo direction = %s, "
                            "Pdo Entry Index = 0x%04X, "
                            "Pdo Entry SubIndex = %u.",
                            pdo_variable, pdo_direction_string,
                            pdo_entry_index, pdo_entry_subindex);
                }
                ssSetErrorStatus(S,errmsg);
                return -1;
            }
            (*pdo_ptr)->configured++;

            if (!pdo_entry_idx) {
                if (port->direction) {
                    slave->input_port_count++;
                }
            }

            if (port->pdo_entry[0]->datatype !=
                    (*pdo_entry_ptr)->datatype) {
                ssSetErrorStatus(S,"Datatypes mixed FIXME");
                return -1;
            }

            if (!port->data_bit_len) {
                /* data_bit_len was not specified in the port definition.
                 * Set this to the data type specified in the Pdo Entry */
                port->data_bit_len = abs((*pdo_entry_ptr)->datatype);
            }

            if ((*pdo_entry_ptr)->bitlen % port->data_bit_len) {
                ssSetErrorStatus(S,"Remainder FIXME");
                return -1;
            }

            port->width += (*pdo_entry_ptr)->bitlen / port->data_bit_len;
        }

        if (port->direction)
            slave->input_pdo_entry_count  += port->pdo_entry_count;
        else
            slave->output_pdo_entry_count += port->pdo_entry_count;

        /* Make sure that the PDO has a supported data type */
        RETURN_ON_ERROR(get_data_type(S, variable, "MappedPdo(1)",
                    port->pdo_entry[0]->datatype,
                    &pdo_data_type, 0, 0));

        /* If PdoDataType is specified, use it, otherwise it is guessed
         * by the DataBitLen spec below */
        switch((pdo_entry_idx = get_numeric_field(S, variable, port_spec,
                    io_spec_idx, 1, "PdoDataType", &val))) {
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
        if (port->pdo_data_type == SS_BOOLEAN)
            port->pdo_data_type = SS_UINT8;

        /* If PdoFullScale is specified, the port data type is double.
         * If port->pdo_full_scale == 0.0, it means that the port data
         * type is specified by PortBitLen */
        switch(get_numeric_field(S, variable, port_spec,
                    io_spec_idx, 0, "PdoFullScale", &val)) {
            case 0:
                port->pdo_full_scale = val;
                port->data_type = SS_DOUBLE;
                break;
            case 1:
                /* Field was not available - not an error */
                ssSetErrorStatus(S,NULL);
                port->data_type = pdo_data_type;
                break;
            default:
                return -1;
        }

        if (port->data_bit_len % 8 || port->data_bit_len == 24) {
            port->bitop = 1;
        }

        if (port->pdo_full_scale) {
            RETURN_ON_ERROR(port->gain_count = get_parameter_values(S, 
                        variable, port_spec, io_spec_idx, "Gain", 
                        port->width, &port->gain_values, 
                        &port->gain_name));

            /* No offset and filter for input ports */
            if (!port->direction) {
                RETURN_ON_ERROR(port->offset_count = get_parameter_values(S, 
                            variable, port_spec, io_spec_idx, "Offset", 
                            port->width, &port->offset_values, 
                            &port->offset_name));
                RETURN_ON_ERROR(port->filter_count = get_parameter_values(S, 
                            variable, port_spec, io_spec_idx, "Filter", 
                            port->width, &port->filter_values, 
                            &port->filter_name));
                slave->filter_count += port->filter_count ? port->width : 0;
            }
        }

        slave->io_port_count++;
        port++;
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
    struct pdo *pdo;

    for (port = slave->io_port;
            port != &slave->io_port[slave->io_port_count];
            port++) {
        (*func)(port->pdo_entry);
        (*func)(port->gain_values);
        (*func)(port->gain_name);
        (*func)(port->offset_values);
        (*func)(port->offset_name);
        (*func)(port->filter_values);
        (*func)(port->filter_name);
    }


    for (pdo = slave->tx_pdo; pdo != slave->tx_pdo_end; pdo++)
        (*func)(pdo->entry);
    for (pdo = slave->rx_pdo; pdo != slave->rx_pdo_end; pdo++)
        (*func)(pdo->entry);

    for (sm = slave->input_sm; sm != slave->input_sm_end; sm++) {
        (*func)(sm->pdo_ptr);
    }
    for (sm = slave->output_sm; sm != slave->output_sm_end; sm++) {
        (*func)(sm->pdo_ptr);
    }

    (*func)(slave->tx_pdo);
    (*func)(slave->input_sm);
    (*func)(slave->input_port_ptr);

    (*func)(slave->rx_pdo);
    (*func)(slave->output_sm);
    (*func)(slave->output_port_ptr);

    (*func)(slave->io_port);
    (*func)(slave->type);
    (*func)(slave->sdo_config);

    (*func)(slave);
}

int_T
set_io_port_width(SimStruct *S, struct ecat_slave *slave, 
    uint_T port_idx, uint_T width)
{
    struct io_port *port = &slave->io_port[port_idx];
    const char_T * direction = port->direction ? "INPUT" : "OUTPUT";

    if (width > port->width) {
        snprintf(errmsg, sizeof(errmsg),
                "Maximum port width for %s port %u is %u.\n"
                "Tried to set it to %i.",
                direction,
                port_idx+1, port->width, width);
        ssSetErrorStatus(S, errmsg);
        return -1;
    }

    if (width > 1 && slave->max_port_width > 1 
            && slave->max_port_width != width) {
        slave->have_mixed_port_widths = 1;
    }

    slave->max_port_width = max(width, slave->max_port_width);

    slave->pwork_count += width;

    if (port->bitop) {
        slave->iwork_count += width;
    }

    if (port->gain_count) {
        if (port->gain_name)
            slave->runtime_param_count += width;
        else {
            slave->rwork_count += width;
        }
    }

    if (port->offset_count) {
        if (port->offset_name)
            slave->runtime_param_count += width;
        else {
            slave->rwork_count += width;
        }
    }

    if (port->filter_count) {
        if (port->filter_name)
            slave->runtime_param_count += width;
        else {
            slave->rwork_count += width;
        }
    }

    return 0;
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
    struct io_port *port, **input_port_ptr, **output_port_ptr;
    uint_T output_port_count;
    struct pdo *pdo;

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

    if (get_slave_address(S, slave)) return;
    if (get_ethercat_info(S, slave)) return;
    slave->layout_version = mxGetScalar(ssGetSFcnParam(S,LAYOUT_VERSION));
    if (get_sdo_config(S, slave)) return;

    if (get_ioport_config(S, slave)) 
        return;
    output_port_count = slave->io_port_count - slave->input_port_count;
    for (pdo = slave->rx_pdo; pdo != slave->rx_pdo_end; pdo++) 
        slave->pdo_entry_count += 
            pdo->configured ? (pdo->entry_end - pdo->entry) : 0;
    for (pdo = slave->tx_pdo; pdo != slave->tx_pdo_end; pdo++) 
        slave->pdo_entry_count += 
            pdo->configured ? (pdo->entry_end - pdo->entry) : 0;

    /* Process input ports */
    if (!ssSetNumInputPorts(S, slave->input_port_count))
        return;
    if (!ssSetNumOutputPorts(S, output_port_count))
        return;

    slave->input_port_ptr =
        mxCalloc(slave->input_port_count, sizeof(struct io_port*));
    slave->output_port_ptr = 
        mxCalloc(output_port_count, sizeof(struct io_port*));
    if ((!slave->input_port_ptr && slave->input_port_count)
            || (!slave->output_port_ptr && output_port_count)) {
        ssSetErrorStatus(S,"Out of memory");
        return;
    }

    input_port_ptr  = slave->input_port_ptr;
    output_port_ptr = slave->output_port_ptr;

    for ( port = slave->io_port;
            port != &slave->io_port[slave->io_port_count];
            port++) {

        if (port->direction) {
            /* Input port */
            ssSetInputPortWidth(S, input_port_ptr - slave->input_port_ptr, 
                    DYNAMICALLY_SIZED);
            ssSetInputPortDataType(S, input_port_ptr - slave->input_port_ptr, 
                    ( port->pdo_full_scale 
                      ? DYNAMICALLY_TYPED : port->data_type));

            *input_port_ptr++ = port;
        }
        else {
            /* Output port */
            ssSetOutputPortWidth(S, 
                    output_port_ptr - slave->output_port_ptr, 
                    port->width);
            ssSetOutputPortDataType(S, 
                    output_port_ptr - slave->output_port_ptr, 
                    port->data_type);

            if (set_io_port_width(S, slave,
                        port - slave->io_port, port->width))
                return;

            *output_port_ptr++ = port;
        }
    }

    ssSetNumSampleTimes(S, 1);

    if (mxGetScalar(ssGetSFcnParam(S,TSAMPLE))) {
        ssSetNumDiscStates(S, slave->filter_count);
    } else {
        ssSetNumContStates(S, slave->filter_count);
    }

    /* Make the memory peristent, otherwise it is lost just before
     * mdlRTW is called. To ensure that the memory is released again,
     * even in case of failures, the option SS_OPTION_CALL_TERMINATE_ON_EXIT
     * has to be set */
    slave_mem_op(slave, mexMakeMemoryPersistent);
    ssSetUserData(S,slave);

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
    ssSetSampleTime(S, 0, mxGetScalar(ssGetSFcnParam(S,TSAMPLE)));
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
    if (!slave)
        return;

    ssSetInputPortWidth(S, port, width);

    set_io_port_width(S, slave, 
            slave->input_port_ptr[port] - slave->io_port, width);
}

/* This function is called when some ports are still DYNAMICALLY_SIZED
 * even after calling mdlSetInputPortWidth(). This occurs when input
 * ports are not connected. This function sets the port width for
 * these ports to 1 */
#define MDL_SET_DEFAULT_PORT_DIMENSION_INFO
static void mdlSetDefaultPortDimensionInfo(SimStruct *S)
{
    struct ecat_slave *slave = ssGetUserData(S);
    int_T port_idx;
    uint_T output_port_count;

    if (!slave)
        return;

    output_port_count = slave->io_port_count - slave->input_port_count;

    for (port_idx = 0; port_idx < slave->input_port_count; port_idx++) {
        if (ssGetInputPortWidth(S, port_idx) == DYNAMICALLY_SIZED) {
            ssSetInputPortWidth(S, port_idx, 
                    slave->input_port_ptr[port_idx]->width);
            set_io_port_width(S, slave, port_idx, 
                    slave->input_port_ptr[port_idx]->width);
        }
    }

    for (port_idx = 0; port_idx < output_port_count; port_idx++) {
        if (ssGetOutputPortWidth(S, port_idx) == DYNAMICALLY_SIZED) {
            ssSetOutputPortWidth(S, port_idx, 
                    slave->output_port_ptr[port_idx]->width);
        }
    }
}

#define MDL_SET_WORK_WIDTHS
static void mdlSetWorkWidths(SimStruct *S)
{
    struct ecat_slave *slave = ssGetUserData(S);
    struct io_port *port;
    uint_T param_idx = 0;
    uint_T input_port_idx = 0;

    if (!slave)
        return;

    ssSetNumPWork(S, slave->pwork_count);
    ssSetNumIWork(S, slave->iwork_count);
    ssSetNumRWork(S, slave->rwork_count);

    if (slave->have_mixed_port_widths) {
        int_T port_idx = 0;
        for (port_idx = 0; port_idx < slave->input_port_count; port_idx++) {
            ssSetInputPortRequiredContiguous(S, port_idx, 1);
        }
    }

    ssSetNumRunTimeParams(S, slave->runtime_param_count);

    for (port = slave->io_port;
            port != &slave->io_port[slave->io_port_count];
            port++) {

        /* Skip not connected input ports */
        if (port->direction  
                && !ssGetInputPortConnected(S, input_port_idx++))
            continue;

        if (port->gain_count && port->gain_name) {
            ssParamRec p = {
                .name = port->gain_name,
                .nDimensions = 1,
                .dimensions = &port->gain_count,
                .dataTypeId = SS_DOUBLE,
                .complexSignal = 0,
                .data = port->gain_values,
                .dataAttributes = NULL,
                .nDlgParamIndices = 0,
                .dlgParamIndices = NULL,
                .transformed = RTPARAM_TRANSFORMED,
                .outputAsMatrix = 0,
            };
            ssSetRunTimeParamInfo(S, param_idx++, &p);
        }

        if (port->offset_count && port->offset_name) {
            ssParamRec p = {
                .name = port->offset_name,
                .nDimensions = 1,
                .dimensions = &port->offset_count,
                .dataTypeId = SS_DOUBLE,
                .complexSignal = 0,
                .data = port->offset_values,
                .dataAttributes = NULL,
                .nDlgParamIndices = 0,
                .dlgParamIndices = NULL,
                .transformed = RTPARAM_TRANSFORMED,
                .outputAsMatrix = 0,
            };
            ssSetRunTimeParamInfo(S, param_idx++, &p);
        }

        if (port->filter_count && port->filter_name) {
            ssParamRec p = {
                .name = port->filter_name,
                .nDimensions = 1,
                .dimensions = &port->filter_count,
                .dataTypeId = SS_DOUBLE,
                .complexSignal = 0,
                .data = port->filter_values,
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

    if (!slave)
        return;

    slave_mem_op(slave, mxFree);
}

/* Create a string of the form ["one","two","three"]\0
 * from the strings passed as a array of pointers. Last element must be NULL.
 * */
char_T *createStrVect(const char_T *strings[], uint_T count)
{
    uint_T len = 0, i;
    const char_T **s;
    char_T *str_vect, *p;

    for (i = 0, s = strings; i < count; i++, s++) {
        len += *s ? strlen(*s) + 3 : 3;
    }
    len += 3; /* For the square braces and trailing \0 */

    p = str_vect = mxCalloc(len, 1);

    *p++ = '[';
    for (i = 0, s = strings; i < count; i++, s++) {
        *p++ = '"';
        if (*s)
            strcpy(p, *s);
        p += *s ? strlen(*s) : 0;
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
    uint_T input_port_count = slave->input_port_count;
    uint_T output_port_count = slave->io_port_count - slave->input_port_count;
    uint_T pwork_index = 0;
    uint_T iwork_index = 0;
    uint_T rwork_index = 0;
    uint_T filter_index = 0;
    real_T rwork_vector[slave->rwork_count];
    uint_T sync_manager_count = 0
        + slave->input_sm_end - slave->input_sm
        + slave->output_sm_end - slave->output_sm;
    uint_T pdo_count = 0
        + slave->tx_pdo_end - slave->tx_pdo
        + slave->rx_pdo_end - slave->rx_pdo;

    /* General assignments of array indices that form the basis for
     * the S-Function <-> TLC communication
     * DO NOT CHANGE THESE without updating the TLC ec_test2.tlc
     * as well */
    enum {
        PortSpecPWork = 0,		/* 0 */
        PortSpecDType,                  /* 1 */
        PortSpecBitLen, 		/* 2 */
        PortSpecIWork,                  /* 3 */
        PortSpecGainCount,              /* 4 */
        PortSpecGainRWorkIdx,           /* 5 */
        PortSpecOffsetCount,            /* 6 */
        PortSpecOffsetRWorkIdx,         /* 7 */
        PortSpecFilterCount,            /* 8 */
        PortSpecFilterIdx,              /* 9 */
        PortSpecFilterRWorkIdx,         /* 10 */
        PortSpecMax                     /* 11 */
    };
    enum {
        SM_Index = 0,
        SM_Direction,
        SM_PdoCount,
        SM_Max
    };
    enum {
        PdoEI_Index = 0,
        PdoEI_SubIndex,
        PdoEI_BitLen,
        PdoEI_Max
    };
    enum {
        PdoInfo_PdoIndex = 0,
        PdoInfo_PdoEntryCount,
        PdoInfo_Max
    };
    enum {
        SdoConfigIndex = 0,       /* 0 */
        SdoConfigSubIndex,          /* 1 */
        SdoConfigDataType,          /* 2 */
        SdoConfigValue,             /* 3 */
        SdoConfigMax                /* 4 */
    };
    enum {
        PdoEntryIndex = 0, 		/* 0 */
        PdoEntrySubIndex, 		/* 1 */
        PdoEntryLength, 		/* 2 */
        PdoEntryDir,                    /* 3 */
        PdoEntryDType,                  /* 4 */
        PdoEntryBitLen,                 /* 5 */
        PdoEntryPWork,                  /* 6 */
        PdoEntryIWork,                  /* 7 */
        PdoEntryMax                     /* 8 */
    };

    uint_T pdo_entry_count = slave->input_pdo_entry_count 
        + slave->output_pdo_entry_count;
    int32_T mapped_pdo_entry[PdoEntryMax][pdo_entry_count];
    uint_T pdo_entry_idx = 0;

    if (!ssWriteRTWScalarParam(S, 
                "MasterId", &slave->master, SS_UINT32))              return;
    if (!ssWriteRTWScalarParam(S, 
                "DomainId", &slave->domain, SS_UINT32))              return;
    if (!ssWriteRTWScalarParam(S, 
                "SlaveAlias", &slave->alias, SS_UINT32))             return;
    if (!ssWriteRTWScalarParam(S, 
                "SlavePosition", &slave->position, SS_UINT32))       return;
    if (!ssWriteRTWStrParam(S, 
                "ProductName", slave->type ? slave->type : ""))      return;
    if (!ssWriteRTWScalarParam(S, 
                "VendorId", &slave->vendor_id, SS_UINT32))           return;
    if (!ssWriteRTWScalarParam(S, 
                "ProductCode", &slave->product_code, SS_UINT32))     return;
    if (!ssWriteRTWScalarParam(S, 
                "RevisionNo", &slave->revision_no, SS_UINT32))       return;
    if (!ssWriteRTWScalarParam(S, 
                "LayoutVersion", &slave->layout_version, SS_UINT32)) return;

    if (slave->sdo_config) { /* Transpose slave->sdo_config */
        uint_T sdo_config_count = slave->sdo_config_end - slave->sdo_config;
        uint32_T sdo_config_out[SdoConfigMax][sdo_config_count];
        struct sdo_config *sdo_config;
        uint_T idx;

        for (sdo_config = slave->sdo_config, idx = 0;
                sdo_config != slave->sdo_config_end;
                sdo_config++, idx++) {
            sdo_config_out[SdoConfigIndex][idx] = sdo_config->index;
            sdo_config_out[SdoConfigSubIndex][idx] = sdo_config->subindex;
            sdo_config_out[SdoConfigDataType][idx] = sdo_config->datatype;
            sdo_config_out[SdoConfigValue][idx] = sdo_config->value;
        }

        if (!ssWriteRTW2dMatParam(S, "SdoConfig", sdo_config_out,
                    SS_UINT32, sdo_config_count, SdoConfigMax))
            return;
    }

    if (sync_manager_count) {
        uint32_T   sync_manager[SM_Max][sync_manager_count];
        uint32_T       pdo_info[PdoInfo_Max][pdo_count];
        uint32_T pdo_entry_info[PdoEI_Max][slave->pdo_entry_count];

        uint_T sm_idx = 0, pdo_idx = 0, pdo_entry_idx = 0;

        struct sync_manager *sm;

        for (sm = slave->output_sm; 
                sm != slave->output_sm_end; sm++, sm_idx++) {
            struct pdo **pdo_ptr;

            sync_manager[SM_Index][sm_idx] = sm->index;
            sync_manager[SM_Direction][sm_idx] = 1;

            for (pdo_ptr = sm->pdo_ptr; pdo_ptr != sm->pdo_ptr_end;
                    pdo_ptr++, pdo_idx++) {
                struct pdo_entry *pdo_entry;

                if (!(*pdo_ptr)->configured)
                    continue;

                sync_manager[SM_PdoCount][sm_idx]++;

                pdo_info[PdoInfo_PdoIndex][pdo_idx] = 
                    (*pdo_ptr)->index;
                pdo_info[PdoInfo_PdoEntryCount][pdo_idx] = 
                    (*pdo_ptr)->entry_count;

                for (pdo_entry = (*pdo_ptr)->entry; 
                        pdo_entry != (*pdo_ptr)->entry_end;
                        pdo_entry++, pdo_entry_idx++) {

                    pdo_entry_info[PdoEI_Index][pdo_entry_idx] = 
                        pdo_entry->index;
                    pdo_entry_info[PdoEI_SubIndex][pdo_entry_idx] =
                        pdo_entry->subindex;
                    pdo_entry_info[PdoEI_BitLen][pdo_entry_idx] =
                        pdo_entry->bitlen;
                }
            }
        }
        for (sm = slave->input_sm; 
                sm != slave->input_sm_end; sm++, sm_idx++) {
            struct pdo **pdo_ptr;

            sync_manager[SM_Index][sm_idx] = sm->index;
            sync_manager[SM_Direction][sm_idx] = 1;

            for (pdo_ptr = sm->pdo_ptr; pdo_ptr != sm->pdo_ptr_end;
                    pdo_ptr++, pdo_idx++) {
                struct pdo_entry *pdo_entry;

                if (!(*pdo_ptr)->configured)
                    continue;

                sync_manager[SM_PdoCount][sm_idx]++;

                pdo_info[PdoInfo_PdoIndex][pdo_idx] = 
                    (*pdo_ptr)->index;
                pdo_info[PdoInfo_PdoEntryCount][pdo_idx] = 
                    (*pdo_ptr)->entry_count;

                for (pdo_entry = (*pdo_ptr)->entry; 
                        pdo_entry != (*pdo_ptr)->entry_end;
                        pdo_entry++, pdo_entry_idx++) {

                    pdo_entry_info[PdoEI_Index][pdo_entry_idx] = 
                        pdo_entry->index;
                    pdo_entry_info[PdoEI_SubIndex][pdo_entry_idx] =
                        pdo_entry->subindex;
                    pdo_entry_info[PdoEI_BitLen][pdo_entry_idx] =
                        pdo_entry->bitlen;
                }
            }
        }

        if (slave->pdo_entry_count) {
            if (!ssWriteRTW2dMatParam(S, "PdoEntryInfo", pdo_entry_info,
                        SS_UINT32, slave->pdo_entry_count, PdoEntryMax))
                return;
        }
        if (pdo_count) {
            if (!ssWriteRTW2dMatParam(S, "PdoInfo", pdo_info,
                        SS_UINT32, pdo_count, PdoInfo_Max))
                return;
        }

        /* Don't need to check for slave->sync_manager_count here,
         * was checked already */
        if (!ssWriteRTW2dMatParam(S, "SyncManager", sync_manager,
                    SS_UINT32, sync_manager_count, SM_Max))
            return;
    }

    if (input_port_count) {
        int32_T input_port_spec[PortSpecMax][input_port_count];
        real_T pdo_full_scale[input_port_count];
        uint_T port_idx;
        const char_T *input_gain_name_ptr[input_port_count];
        const char_T *input_offset_name_ptr[input_port_count];
        const char_T *input_filter_name_ptr[input_port_count];
        const char_T *input_gain_str;
        const char_T *input_offset_str;
        const char_T *input_filter_str;

        memset(  input_gain_name_ptr, 0, sizeof(  input_gain_name_ptr));
        memset(input_offset_name_ptr, 0, sizeof(input_offset_name_ptr));
        memset(input_filter_name_ptr, 0, sizeof(input_filter_name_ptr));

        memset(input_port_spec, 0, sizeof(input_port_spec));

        for (port_idx = 0; port_idx < input_port_count; port_idx++) {
            struct io_port *port = slave->input_port_ptr[port_idx];
            struct pdo_entry **pdo_entry_ptr;

            input_port_spec[PortSpecPWork][port_idx] = pwork_index;

            /* Remap the pdo_data_type to uint8_T if it is boolean_T */
            input_port_spec[PortSpecDType][port_idx] =
                port->pdo_data_type == SS_BOOLEAN 
                ? SS_UINT8 : port->pdo_data_type;

            input_port_spec[PortSpecBitLen][port_idx] =
                port->bitop ? port->data_bit_len : 0;
            input_port_spec[PortSpecIWork][port_idx] = iwork_index;

            if (port->gain_count) {
                input_port_spec[PortSpecGainCount][port_idx] =
                    port->gain_count;
                if (port->gain_name) {
                    input_port_spec[PortSpecGainRWorkIdx][port_idx] = -1;
                    input_gain_name_ptr[port_idx] = port->gain_name;
                }
                else {
                    memcpy(rwork_vector + rwork_index, port->gain_values,
                            sizeof(*rwork_vector) * port->gain_count);
                    input_port_spec[PortSpecGainRWorkIdx][port_idx] =
                        rwork_index;
                    rwork_index += port->gain_count;
                    input_gain_name_ptr[port_idx] = port->gain_count == 1
                        ? NULL : "<rwork>/NonTuneableParam";
                }
            }

            if (port->offset_count) {
                input_port_spec[PortSpecOffsetCount][port_idx] =
                    port->offset_count;
                if (port->offset_name) {
                    input_port_spec[PortSpecOffsetRWorkIdx][port_idx] = -1;
                    input_offset_name_ptr[port_idx] = port->offset_name;
                }
                else {
                    memcpy(rwork_vector + rwork_index, port->offset_values,
                            sizeof(*rwork_vector) * port->offset_count);
                    input_port_spec[PortSpecOffsetRWorkIdx][port_idx] =
                        rwork_index;
                    rwork_index += port->offset_count;
                    input_offset_name_ptr[port_idx] = port->offset_count == 1
                        ? NULL : "<rwork>/NonTuneableParam";
                }
            }

            if (port->filter_count) {
                input_port_spec[PortSpecFilterCount][port_idx] =
                    port->filter_count;
                input_port_spec[PortSpecFilterIdx][port_idx] = filter_index;
                filter_index += port->filter_count;
                if (port->filter_name) {
                    input_port_spec[PortSpecFilterRWorkIdx][port_idx] = -1;
                    input_filter_name_ptr[port_idx] = port->filter_name;
                }
                else {
                    memcpy(rwork_vector + rwork_index, port->filter_values,
                            sizeof(*rwork_vector) * port->filter_count);
                    input_port_spec[PortSpecFilterRWorkIdx][port_idx] =
                        rwork_index;
                    rwork_index += port->filter_count;
                    input_filter_name_ptr[port_idx] = port->filter_count == 1
                        ? NULL : "<rwork>/NonTuneableParam";
                }
            }

            pdo_full_scale[port_idx] = port->pdo_full_scale;

            for (pdo_entry_ptr = port->pdo_entry;
                    pdo_entry_ptr != &port->pdo_entry[port->pdo_entry_count];
                    pdo_entry_ptr++) {
                uint_T vector_length =
                    (*pdo_entry_ptr)->bitlen / port->data_bit_len;

                mapped_pdo_entry[PdoEntryIndex][pdo_entry_idx] =
                    (*pdo_entry_ptr)->index;
                mapped_pdo_entry[PdoEntrySubIndex][pdo_entry_idx] =
                    (*pdo_entry_ptr)->subindex;
                mapped_pdo_entry[PdoEntryDir][pdo_entry_idx] = 1;
                mapped_pdo_entry[PdoEntryLength][pdo_entry_idx] = 
                    vector_length;
                mapped_pdo_entry[PdoEntryDType][pdo_entry_idx] = 
                    port->pdo_data_type;
                mapped_pdo_entry[PdoEntryBitLen][pdo_entry_idx] = 
                    port->data_bit_len;
                mapped_pdo_entry[PdoEntryPWork][pdo_entry_idx] = pwork_index;
                mapped_pdo_entry[PdoEntryIWork][pdo_entry_idx] = 
                    port->bitop ? iwork_index : -1;

                pwork_index += vector_length;
                if (port->bitop)
                    iwork_index += vector_length;

                pdo_entry_idx++;
            }

        }

        input_gain_str = createStrVect(input_gain_name_ptr, 
                input_port_count);
        input_offset_str = createStrVect(input_offset_name_ptr,
                input_port_count);
        input_filter_str = createStrVect(input_filter_name_ptr,
                input_port_count);

        if (!ssWriteRTW2dMatParam(S, "InputPort", input_port_spec,
                    SS_INT32, input_port_count, PortSpecMax))
            return;
        if (!ssWriteRTWVectParam(S, "InputPDOFullScale",
                    pdo_full_scale, SS_DOUBLE, input_port_count))
            return;
        if (!ssWriteRTWStrVectParam(S, "InputGainName", input_gain_str,
                    input_port_count))
            return;
        if (!ssWriteRTWStrVectParam(S, "InputOffsetName", input_offset_str,
                    input_port_count))
            return;
        if (!ssWriteRTWStrVectParam(S, "InputFilterName", input_filter_str,
                    input_port_count))
            return;
    }

    if (output_port_count) {
        int32_T output_port_spec[PortSpecMax][output_port_count];
        real_T pdo_full_scale[output_port_count];
        uint_T port_idx;
        const char_T *output_gain_name_ptr[output_port_count];
        const char_T *output_offset_name_ptr[output_port_count];
        const char_T *output_filter_name_ptr[output_port_count];
        const char_T *output_gain_str;
        const char_T *output_offset_str;
        const char_T *output_filter_str;

        memset(  output_gain_name_ptr, 0, sizeof(  output_gain_name_ptr));
        memset(output_offset_name_ptr, 0, sizeof(output_offset_name_ptr));
        memset(output_filter_name_ptr, 0, sizeof(output_filter_name_ptr));

        memset(output_port_spec, 0, sizeof(output_port_spec));

        for (port_idx = 0; port_idx < output_port_count; port_idx++) {
            struct io_port *port = slave->output_port_ptr[port_idx];
            struct pdo_entry **pdo_entry_ptr;

            output_port_spec[PortSpecPWork][port_idx] = pwork_index;
            output_port_spec[PortSpecDType][port_idx] = 
                port->pdo_data_type == SS_BOOLEAN 
                ? SS_UINT8 : port->pdo_data_type;
            output_port_spec[PortSpecBitLen][port_idx] =
                port->bitop ? port->data_bit_len : 0;
            output_port_spec[PortSpecIWork][port_idx] = iwork_index;

            if (port->gain_count) {
                output_port_spec[PortSpecGainCount][port_idx] =
                    port->gain_count;
                if (port->gain_name) {
                    output_port_spec[PortSpecGainRWorkIdx][port_idx] = -1;
                    output_gain_name_ptr[port_idx] = port->gain_name;
                }
                else {
                    memcpy(rwork_vector + rwork_index, port->gain_values,
                            sizeof(*rwork_vector) * port->gain_count);
                    output_port_spec[PortSpecGainRWorkIdx][port_idx] =
                        rwork_index;
                    rwork_index += port->gain_count;
                    output_gain_name_ptr[port_idx] = port->gain_count == 1
                        ? NULL : "<rwork>/NonTuneableParam";
                }
            }

            if (port->offset_count) {
                output_port_spec[PortSpecOffsetCount][port_idx] =
                    port->offset_count;
                if (port->offset_name) {
                    output_port_spec[PortSpecOffsetRWorkIdx][port_idx] = -1;
                    output_offset_name_ptr[port_idx] = port->offset_name;
                }
                else {
                    memcpy(rwork_vector + rwork_index, port->offset_values,
                            sizeof(*rwork_vector) * port->offset_count);
                    output_port_spec[PortSpecOffsetRWorkIdx][port_idx] =
                        rwork_index;
                    rwork_index += port->offset_count;
                    output_offset_name_ptr[port_idx] = port->offset_count == 1
                        ? NULL : "<rwork>/NonTuneableParam";
                }
            }

            if (port->filter_count) {
                output_port_spec[PortSpecFilterCount][port_idx] =
                    port->filter_count;
                output_port_spec[PortSpecFilterIdx][port_idx] = filter_index;
                filter_index += port->filter_count;
                if (port->filter_name) {
                    output_port_spec[PortSpecFilterRWorkIdx][port_idx] = -1;
                    output_filter_name_ptr[port_idx] = port->filter_name;
                }
                else {
                    memcpy(rwork_vector + rwork_index, port->filter_values,
                            sizeof(*rwork_vector) * port->filter_count);
                    output_port_spec[PortSpecFilterRWorkIdx][port_idx] =
                        rwork_index;
                    rwork_index += port->filter_count;
                    output_filter_name_ptr[port_idx] = port->filter_count == 1
                        ? NULL : "<rwork>/NonTuneableParam";
                }
            }

            pdo_full_scale[port_idx] = port->pdo_full_scale;

            for (pdo_entry_ptr = port->pdo_entry;
                    pdo_entry_ptr != &port->pdo_entry[port->pdo_entry_count];
                    pdo_entry_ptr++) {
                uint_T vector_length =
                    (*pdo_entry_ptr)->bitlen / port->data_bit_len;

                mapped_pdo_entry[PdoEntryIndex][pdo_entry_idx] =
                    (*pdo_entry_ptr)->index;
                mapped_pdo_entry[PdoEntrySubIndex][pdo_entry_idx] =
                    (*pdo_entry_ptr)->subindex;
                mapped_pdo_entry[PdoEntryDir][pdo_entry_idx] = 0;
                mapped_pdo_entry[PdoEntryLength][pdo_entry_idx] =
                    vector_length;
                mapped_pdo_entry[PdoEntryDType][pdo_entry_idx] = 
                    port->pdo_data_type;
                mapped_pdo_entry[PdoEntryBitLen][pdo_entry_idx] = 
                    port->data_bit_len;
                mapped_pdo_entry[PdoEntryPWork][pdo_entry_idx] = pwork_index;
                mapped_pdo_entry[PdoEntryIWork][pdo_entry_idx] = 
                    port->bitop ? iwork_index : -1;

                pwork_index += vector_length;
                if (port->bitop)
                    iwork_index += vector_length;

                pdo_entry_idx++;
            }

        }

        output_gain_str = createStrVect(output_gain_name_ptr, 
                output_port_count);
        output_offset_str = createStrVect(output_offset_name_ptr,
                output_port_count);
        output_filter_str = createStrVect(output_filter_name_ptr,
                output_port_count);


        if (!ssWriteRTW2dMatParam(S, "OutputPort", output_port_spec,
                    SS_INT32, output_port_count, PortSpecMax))
            return;
        if (!ssWriteRTWVectParam(S, "OutputPDOFullScale",
                    pdo_full_scale, SS_DOUBLE, output_port_count))
            return;
        if (!ssWriteRTWStrVectParam(S, "OutputGainName", output_gain_str,
                    output_port_count))
            return;
        if (!ssWriteRTWStrVectParam(S, "OutputOffsetName", output_offset_str,
                    output_port_count))
            return;
        if (!ssWriteRTWStrVectParam(S, "OutputFilterName", output_filter_str,
                    output_port_count))
            return;
    }

    if (pdo_entry_count) {
        if (!ssWriteRTW2dMatParam(S, "MappedPdoEntry", mapped_pdo_entry,
                    SS_INT32, pdo_entry_count, PdoEntryMax))
            return;
    }

    if (!ssWriteRTWScalarParam(S, 
                "FilterCount", &slave->filter_count, SS_UINT32))
        return;

    if (!ssWriteRTWVectParam(S, "RWorkValues",
                rwork_vector, SS_DOUBLE, slave->rwork_count))
        return;

    if (!ssWriteRTWWorkVect(S, "IWork", 1, "BitOffset", slave->iwork_count))
        return;
    if (!ssWriteRTWWorkVect(S, "PWork", 1, "DataPtr", slave->pwork_count))
        return;
    if (!ssWriteRTWWorkVect(S, "RWork", 1, "NonTuneableParam", 
                slave->rwork_count))
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
