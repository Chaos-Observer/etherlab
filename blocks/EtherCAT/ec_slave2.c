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
    } *sdo_config;
    uint_T sdo_config_count;

    struct sync_manager {
        uint16_t index;
        enum {EC_DIR_OUTPUT, EC_DIR_INPUT} direction;
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
    uint_T pdo_count;
    uint_T pdo_entry_count;

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
    RETURN_ON_ERROR(get_string_field(S, param, ec_info, 0,
                "Type", "Unspecified", &slave->type));

    sm_field = mxGetField(ec_info, 0, "SyncManager");
    if (!sm_field)
        return 0;
    
    /* Count the number of fields */
    slave->sync_manager_count = mxGetNumberOfElements(sm_field);
    CHECK_CALLOC(S,slave->sync_manager_count, sizeof(struct sync_manager),
            slave->sync_manager);

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

                RETURN_ON_ERROR(get_numeric_field(S, variable,
                            pdo_entry, entry_idx, 1, "BitLen", &val));
                entry->bitlen = val;

                RETURN_ON_ERROR(get_numeric_field(S, variable,
                            pdo_entry, entry_idx, 0, "Index", &val));
                entry->index = val;

                if (!entry->index)
                    continue;

                RETURN_ON_ERROR(get_numeric_field(S, variable,
                            pdo_entry, entry_idx, 1, "SubIndex", &val));
                entry->subindex = val;

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
            }
            slave->pdo_entry_count += pdo->entry_count;
        }
        slave->pdo_count += sm->pdo_count;
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
find_pdo_entry(const struct ecat_slave *slave,
        uint16_T pdo_entry_index, uint8_T pdo_entry_subindex, 
        uint_T port_direction, uint_T *port_direction_ptr,
        struct pdo_entry **pdo_entry) 
{
    const struct sync_manager *sm; 

    for (sm = slave->sync_manager; 
            sm != &slave->sync_manager[slave->sync_manager_count];
            sm++) {
        struct pdo *pdo; 

        /* If port_direction_ptr is not set, make sure that port direction
         * and sync-manager direction are not the same
         * Take care, sync-manager direction 1 means input for the master,
         * corresponding to a TxPdo, connected to s Simulink output port. */
        if (!port_direction_ptr && 
                (port_direction && sm->direction == EC_DIR_INPUT))
            continue;


        for (pdo = sm->pdo; pdo != &sm->pdo[sm->pdo_count]; pdo++) {
            for (*pdo_entry = pdo->entry; 
                    *pdo_entry != &pdo->entry[pdo->entry_count]; 
                    (*pdo_entry)++) {
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

    *pdo_entry = NULL;
    return -1;
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
    uint_T port_idx = 0;

    if (!port_spec || 
            !(slave->io_port_count = mxGetNumberOfElements(port_spec)))
        return 0;
    CHECK_CALLOC(S, slave->io_port_count, sizeof(struct io_port), 
            slave->io_port);

    for (port = slave->io_port; 
            port != &slave->io_port[slave->io_port_count];
            port++, port_idx++) {
        const mxArray *pdo_entry_spec = 
            mxGetField(port_spec, port_idx, "PdoEntry");
        struct pdo_entry **pdo_entry_ptr;
        uint_T pdo_entry_idx;
        boolean_T struct_access;
        real_T *pdo_entry_index_field;
        real_T *pdo_entry_subindex_field;
        uint_T pdo_data_type;

        snprintf(variable, sizeof(variable), 
                "%s(%u).PdoEntry", param, port_idx+1);

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
        else {
            /* Mapped pdos is defined using a M-by-3 array, where
             * the columns define the 'Index', 'EntryIndex' and 
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
        CHECK_CALLOC(S, port->pdo_entry_count, sizeof(struct pdo_entry*),
                port->pdo_entry);

        snprintf(variable, sizeof(variable), "%s(%u)", param, port_idx+1);

        /* Get the list of pdo entries that are mapped to this port */
        for (pdo_entry_idx = 0, pdo_entry_ptr = port->pdo_entry; 
                pdo_entry_idx < port->pdo_entry_count; 
                pdo_entry_idx++, pdo_entry_ptr++) {
            uint16_T pdo_entry_index;
            uint8_T pdo_entry_subindex;
            char_T pdo_variable[100];

            snprintf(pdo_variable, sizeof(pdo_variable), 
                    "%s(%u).PdoEntry(%u)", 
                    param, port_idx+1, pdo_entry_idx+1);

            if (struct_access) {
                RETURN_ON_ERROR(get_numeric_field(S, pdo_variable, 
                            pdo_entry_spec, pdo_entry_idx, 1, 
                            "PdoEntryIndex", &val));
                pdo_entry_index = val;

                RETURN_ON_ERROR(get_numeric_field(S, pdo_variable, 
                            pdo_entry_spec, pdo_entry_idx, 1, 
                            "PdoEntrySubIndex", &val));
                pdo_entry_subindex = val;
            }
            else {
                pdo_entry_index    = *pdo_entry_index_field++;
                pdo_entry_subindex = *pdo_entry_subindex_field++;
            }

            /* Get the pdo and pdo entry pointers for the specified 
             * indices */
            if (find_pdo_entry(slave, pdo_entry_index, pdo_entry_subindex, 
                        port->direction,
                        pdo_entry_index ? &port->direction : NULL,
                        pdo_entry_ptr)) {
                if (pdo_entry_index) {
                    snprintf(errmsg, sizeof(errmsg),
                            "Cannot find Pdo Entry specified by "
                            "SFunction parameter %s.\n"
                            "Looking for Pdo direction = %s, "
                            "Pdo Entry Index = 0x%04X, "
                            "Pdo Entry SubIndex = %u.",
                            pdo_variable,
                            port->direction ? "TxPdo" : "RxPdo",
                            pdo_entry_index, pdo_entry_subindex);
                }
                else {
                    snprintf(errmsg, sizeof(errmsg),
                            "Cannot find Pdo Entry specified by "
                            "SFunction parameter %s.\n"
                            "Looking for Pdo direction = %s, "
                            "Pdo Entry Index = 0x%04X, "
                            "Pdo Entry SubIndex = %u.",
                            pdo_variable,
                            port->direction ? "TxPdo" : "RxPdo",
                            pdo_entry_index, pdo_entry_subindex);
                }
                ssSetErrorStatus(S,errmsg);
                return -1;
            }

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
                    port_idx, 0, "PdoDataType", &val))) {
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
        if (port->pdo_data_type) {
            port->pdo_data_type = SS_UINT8;
        }

        /* If PdoFullScale is specified, the port data type is double.
         * If port->pdo_full_scale == 0.0, it means that the port data
         * type is specified by PortBitLen */
        switch(get_numeric_field(S, variable, port_spec,
                    port_idx, 1, "PdoFullScale", &val)) {
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

        if (port->data_bit_len % 8 || port->data_bit_len == 24) {
            port->bitop = 1;
        }

        if (port->pdo_full_scale) {
            RETURN_ON_ERROR(port->gain_count = get_parameter_values(S, 
                        variable, port_spec, port_idx, "Gain", 
                        port->width, &port->gain_values, 
                        &port->gain_name));

            /* No offset and filter for input ports */
            if (!port->direction) {
                RETURN_ON_ERROR(port->offset_count = get_parameter_values(S, 
                            variable, port_spec, port_idx, "Offset", 
                            port->width, &port->offset_values, 
                            &port->offset_name));
                RETURN_ON_ERROR(port->filter_count = get_parameter_values(S, 
                            variable, port_spec, port_idx, "Filter", 
                            port->width, &port->filter_values, 
                            &port->filter_name));
                slave->filter_count += port->filter_count ? port->width : 0;
            }
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

    for (sm = slave->sync_manager;
            sm != &slave->sync_manager[slave->sync_manager_count]; 
            sm++) {
        struct pdo *pdo;

        for (pdo = sm->pdo; pdo != &sm->pdo[sm->pdo_count]; pdo++)
            (*func)(pdo->entry);
        (*func)(sm->pdo);
    }

    (*func)(slave->input_port_ptr);
    (*func)(slave->output_port_ptr);
    (*func)(slave->io_port);
    (*func)(slave->type);
    (*func)(slave->sdo_config);
    (*func)(slave->sync_manager);

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

    if (width > 1 && slave->max_port_width != width) {
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
    slave->layout_version = mxGetScalar(ssGetSFcnParam(S,LAYOUT_VERSION));
    if (get_sdo_config(S, slave)) return;

    if (get_ioport_config(S, slave)) 
        return;
    output_port_count = slave->io_port_count - slave->input_port_count;

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
    uint_T output_port_count = slave->io_port_count - slave->input_port_count;

    for (port_idx = 0; port_idx < slave->input_port_count; port_idx++) {
        if (ssGetInputPortWidth(S, port_idx) == DYNAMICALLY_SIZED) {
            ssSetInputPortWidth(S, port_idx, 
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
    enum {
        PdoEntryIndex = 0, 
        PdoEntrySubIndex, 
        PdoEntryLength, 
        PdoEntryDType,
        PdoEntryPWork, 
        PdoEntryIWork,
        PdoEntryMax
    };
    enum {
        PortSpecPWork = 0,
        PortSpecDType,
        PortSpecBitLen, 
        PortSpecIWork,
        PortSpecGainCount,
        PortSpecGainRWorkIdx,
        PortSpecOffsetCount,
        PortSpecOffsetRWorkIdx,
        PortSpecFilterCount,
        PortSpecFilterIdx,
        PortSpecFilterRWorkIdx,
        PortSpecMax
    };

    if (!ssWriteRTWScalarParam(S, 
                "MasterId", &slave->master, SS_UINT32))              return;
    if (!ssWriteRTWScalarParam(S, 
                "DomainId", &slave->domain, SS_UINT32))              return;
    if (!ssWriteRTWScalarParam(S, 
                "SlaveAlias", &slave->alias, SS_UINT32))             return;
    if (!ssWriteRTWScalarParam(S, 
                "SlavePosition", &slave->position, SS_UINT32))       return;
    if (!ssWriteRTWStrParam(S, 
                "ProductName", slave->type))                         return;
    if (!ssWriteRTWScalarParam(S, 
                "VendorId", &slave->vendor_id, SS_UINT32))           return;
    if (!ssWriteRTWScalarParam(S, 
                "ProductCode", &slave->product_code, SS_UINT32))     return;
    if (!ssWriteRTWScalarParam(S, 
                "LayoutVersion", &slave->layout_version, SS_UINT32)) return;

    if (slave->sdo_config_count) { /* Transpose slave->sdo_config */
        enum {SdoConfigIndex = 0,
            SdoConfigSubIndex,
            SdoConfigDataType,
            SdoConfigValue,
            SdoConfigMax
        };
        uint32_T sdo_config[SdoConfigMax][slave->sdo_config_count];
        uint_T i;

        for (i = 0; i < slave->sdo_config_count; i++) {
            sdo_config[0][i] = slave->sdo_config[i].index;
            sdo_config[1][i] = slave->sdo_config[i].subindex;
            sdo_config[2][i] = slave->sdo_config[i].datatype;
            sdo_config[3][i] = slave->sdo_config[i].value;
        }

        if (!ssWriteRTW2dMatParam(S, "SdoConfig", sdo_config,
                    SS_UINT32, slave->sdo_config_count, SdoConfigMax))
            return;
    }

    if (slave->sync_manager_count) {
        enum {
            SM_Index = 0,
            SM_Direction,
            SM_PdoCount,
            SM_Max
        };
        enum {
            PdoEntryIndex = 0,
            PdoEntrySubIndex,
            PdoEntryBitLen,
            PdoEntryMax
        };
        enum {
            PdoInfo_PdoIndex = 0,
            PdoInfo_PdoEntryCount,
            PdoInfo_Max
        };
        uint32_T   sync_manager[SM_Max][slave->sync_manager_count];
        uint32_T       pdo_info[PdoInfo_Max][slave->pdo_count];
        uint32_T pdo_entry_info[PdoEntryMax][slave->pdo_entry_count];

        uint_T sm_idx = 0, pdo_idx = 0, pdo_entry_idx = 0;

        struct sync_manager *sm;

        for (sm = slave->sync_manager;
                sm != &slave->sync_manager[slave->sync_manager_count];
                sm++, sm_idx++) {
            struct pdo *pdo;

            sync_manager[0][sm_idx] = sm->index;
            sync_manager[1][sm_idx] = sm->direction == EC_DIR_INPUT;
            sync_manager[2][sm_idx] = sm->pdo_count;

            for (pdo = sm->pdo;
                    pdo != &sm->pdo[sm->pdo_count];
                    pdo++, pdo_idx++) {
                struct pdo_entry *pdo_entry;

                pdo_info[0][pdo_idx] = pdo->index;
                pdo_info[1][pdo_idx] = pdo->entry_count;

                for (pdo_entry = pdo->entry; 
                        pdo_entry != &pdo->entry[pdo->entry_count];
                        pdo_entry++, pdo_entry_idx++) {

                    pdo_entry_info[0][pdo_entry_idx] = pdo_entry->index;
                    pdo_entry_info[1][pdo_entry_idx] = pdo_entry->subindex;
                    pdo_entry_info[2][pdo_entry_idx] = pdo_entry->bitlen;
                }
            }
        }

        if (slave->pdo_entry_count) {
            if (!ssWriteRTW2dMatParam(S, "PdoEntryInfo", pdo_entry_info,
                        SS_UINT32, slave->pdo_entry_count, PdoEntryMax))
                return;
        }
        if (slave->pdo_count) {
            if (!ssWriteRTW2dMatParam(S, "PdoInfo", pdo_info,
                        SS_UINT32, slave->pdo_count, PdoInfo_Max))
                return;
        }

        /* Don't need to check for slave->sync_manager_count here,
         * was checked already */
        if (!ssWriteRTW2dMatParam(S, "SyncManager", sync_manager,
                    SS_UINT32, slave->sync_manager_count, SM_Max))
            return;
    }

    if (input_port_count) {
        int32_T pdo_entry[PdoEntryMax][slave->input_pdo_entry_count];
        int32_T input_port_spec[PortSpecMax][input_port_count];
        real_T pdo_full_scale[input_port_count];
        uint_T pdo_entry_idx = 0;
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

            input_port_spec[0][port_idx] = pwork_index;
            input_port_spec[1][port_idx] = port->pdo_data_type;
            input_port_spec[2][port_idx] =
                port->bitop ? port->data_bit_len : 0;
            input_port_spec[3][port_idx] = iwork_index;

            if (port->gain_count) {
                input_port_spec[4][port_idx] = port->gain_count;
                if (port->gain_name) {
                    input_port_spec[5][port_idx] = -1;
                    input_gain_name_ptr[port_idx] = port->gain_name;
                }
                else {
                    memcpy(rwork_vector + rwork_index, port->gain_values,
                            sizeof(*rwork_vector) * port->gain_count);
                    input_port_spec[5][port_idx] = rwork_index;
                    rwork_index += port->gain_count;
                    input_gain_name_ptr[port_idx] = port->gain_count == 1
                        ? NULL : "<rwork>/NonTuneableParam";
                }
            }

            if (port->offset_count) {
                input_port_spec[6][port_idx] = port->offset_count;
                if (port->offset_name) {
                    input_port_spec[7][port_idx] = -1;
                    input_offset_name_ptr[port_idx] = port->offset_name;
                }
                else {
                    memcpy(rwork_vector + rwork_index, port->offset_values,
                            sizeof(*rwork_vector) * port->offset_count);
                    input_port_spec[7][port_idx] = rwork_index;
                    rwork_index += port->offset_count;
                    input_offset_name_ptr[port_idx] = port->offset_count == 1
                        ? NULL : "<rwork>/NonTuneableParam";
                }
            }

            if (port->filter_count) {
                input_port_spec[8][port_idx] = port->filter_count;
                input_port_spec[9][port_idx] = filter_index;
                filter_index += port->filter_count;
                if (port->filter_name) {
                    input_port_spec[10][port_idx] = -1;
                    input_filter_name_ptr[port_idx] = port->filter_name;
                }
                else {
                    memcpy(rwork_vector + rwork_index, port->filter_values,
                            sizeof(*rwork_vector) * port->filter_count);
                    input_port_spec[10][port_idx] = rwork_index;
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

                pdo_entry[PdoEntryIndex][pdo_entry_idx] =
                    (*pdo_entry_ptr)->index;
                pdo_entry[PdoEntrySubIndex][pdo_entry_idx] =
                    (*pdo_entry_ptr)->subindex;
                pdo_entry[PdoEntryLength][pdo_entry_idx] = vector_length;
                pdo_entry[PdoEntryDType][pdo_entry_idx] = 
                    port->pdo_data_type;
                pdo_entry[PdoEntryPWork][pdo_entry_idx] = pwork_index;
                pdo_entry[PdoEntryIWork][pdo_entry_idx] = 
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


        if (!ssWriteRTW2dMatParam(S, "InputPdoEntry", pdo_entry,
                    SS_INT32, slave->input_pdo_entry_count, PdoEntryMax))
            return;

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
        int32_T pdo_entry[PdoEntryMax][slave->output_pdo_entry_count];
        int32_T output_port_spec[PortSpecMax][output_port_count];
        real_T pdo_full_scale[output_port_count];
        uint_T pdo_entry_idx = 0;
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

            output_port_spec[0][port_idx] = pwork_index;
            output_port_spec[1][port_idx] = port->pdo_data_type;
            output_port_spec[2][port_idx] =
                port->bitop ? port->data_bit_len : 0;
            output_port_spec[3][port_idx] = iwork_index;

            if (port->gain_count) {
                output_port_spec[4][port_idx] = port->gain_count;
                if (port->gain_name) {
                    output_port_spec[5][port_idx] = -1;
                    output_gain_name_ptr[port_idx] = port->gain_name;
                }
                else {
                    memcpy(rwork_vector + rwork_index, port->gain_values,
                            sizeof(*rwork_vector) * port->gain_count);
                    output_port_spec[5][port_idx] = rwork_index;
                    rwork_index += port->gain_count;
                    output_gain_name_ptr[port_idx] = port->gain_count == 1
                        ? NULL : "<rwork>/NonTuneableParam";
                }
            }

            if (port->offset_count) {
                output_port_spec[6][port_idx] = port->offset_count;
                if (port->offset_name) {
                    output_port_spec[7][port_idx] = -1;
                    output_offset_name_ptr[port_idx] = port->offset_name;
                }
                else {
                    memcpy(rwork_vector + rwork_index, port->offset_values,
                            sizeof(*rwork_vector) * port->offset_count);
                    output_port_spec[7][port_idx] = rwork_index;
                    rwork_index += port->offset_count;
                    output_offset_name_ptr[port_idx] = port->offset_count == 1
                        ? NULL : "<rwork>/NonTuneableParam";
                }
            }

            if (port->filter_count) {
                output_port_spec[8][port_idx] = port->filter_count;
                output_port_spec[9][port_idx] = filter_index;
                filter_index += port->filter_count;
                if (port->filter_name) {
                    output_port_spec[10][port_idx] = -1;
                    output_filter_name_ptr[port_idx] = port->filter_name;
                }
                else {
                    memcpy(rwork_vector + rwork_index, port->filter_values,
                            sizeof(*rwork_vector) * port->filter_count);
                    output_port_spec[10][port_idx] = rwork_index;
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

                pdo_entry[PdoEntryIndex][pdo_entry_idx] =
                    (*pdo_entry_ptr)->index;
                pdo_entry[PdoEntrySubIndex][pdo_entry_idx] =
                    (*pdo_entry_ptr)->subindex;
                pdo_entry[PdoEntryLength][pdo_entry_idx] = vector_length;
                pdo_entry[PdoEntryDType][pdo_entry_idx] = 
                    port->pdo_data_type;
                pdo_entry[PdoEntryPWork][pdo_entry_idx] = pwork_index;
                pdo_entry[PdoEntryIWork][pdo_entry_idx] = 
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


        if (!ssWriteRTW2dMatParam(S, "OutputPdoEntry", pdo_entry,
                    SS_INT32, slave->output_pdo_entry_count, PdoEntryMax))
            return;

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
