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
    SDO_CONFIG,
    PORT_SPEC,
    DEBUG,
    TSAMPLE,
    PARAM_COUNT
};

char errmsg[256];

#define abs(x) ((x) >= 0 ? (x) : -(x))
#define max(x,y) ((x) >= (y) ? (x) : (y))

struct ecat_slave {
    SimStruct *S;
    unsigned int debug_level;

    /* Slave address data */
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
     * (vendor_id, product_code, layout_version) share the same EtherCAT Info
     * data in the generated RTW code, meaning that only one data set
     * for every unique key exists. */
    uint32_T pdo_configuration_variant;

    /* Most slaves have a simple PDO layout with no mutual exclusions.
     * These instances may all share the same configuration layout and
     * thus save memory.
     * Complex slaves have a myriad of PDO and PDO Entry assignments
     * so that it is impossible to share the configuration with other
     * slaves. These slaves will set their unique_config flag so that
     * a separate configuration is generated for every slave.
     *
     * Note that this does not affect SDO configuration. This is
     * unique for every slave by nature
     */
    boolean_T unique_config;

    struct sdo_config {
        uint16_T index;
        uint8_T subindex;

        /* Data type. One of SS_UINT8, SS_UINT16, SS_UINT32 */
        uint_T datatype;

        /* Configuration value */
        uint_T value;
    } *sdo_config, *sdo_config_end;

    struct pdo {
        uint16_t index;

        struct sync_manager *sm;

        boolean_T direction;
        boolean_T mandatory;
        boolean_T virtual;

        /* A list of PDO Indices to exclude if this PDO is selected */
        uint_T *exclude_index, *exclude_index_end;

        struct pdo_entry {
            uint16_t index;
            uint8_t subindex;
            uint_T bitlen;
            int_T datatype;
        } *entry, *entry_end;

        /* include_in_config:
         *      = +1 Pdo must be written to SlaveConfig
         *      = -1 Pdo must not be written to SlaveConfig
         *      = 0 - depending on use count and mandatory status
         */
        boolean_T exclude_from_config;

        uint_T use_count;
    } *pdo, *pdo_end;

    /* This value is needed right at the end when the values have
     * to be written by mdlRTW */
    uint_T pdo_entry_count;
    uint_T pdo_count;

    struct sync_manager {
        uint16_t index;

        enum sm_direction {EC_SM_NONE, EC_SM_VIRTUAL,
            EC_SM_INPUT, EC_SM_OUTPUT, EC_SM_MAX} direction;
        boolean_T virtual;

        /* A list of pointers to pdo */
        struct pdo **pdo_ptr, **pdo_ptr_end;
    } *sync_manager, *sync_manager_end;

    uint_T output_sm_count;
    uint_T input_sm_count;

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
    } *io_port, *io_port_end;

    /* Number of input and output ports */
    uint_T input_port_count;
    uint_T output_port_count;

    uint_T input_pdo_entry_count;
    uint_T output_pdo_entry_count;

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

char_T msg[512];
const char_T *path;

#undef printf

void
intro(const struct ecat_slave *slave, unsigned int indent,
        const char_T* parent_context, const char_T *context)
{
    if (path != slave->S->path) {
        mexPrintf("====== EtherCAT slave %s =======\n",
                slave->S->path);
        path = slave->S->path;
    }
    if (context) {
        unsigned int i;
        for (i = 0; i < indent; i++)
            mexPrintf("  ");
        mexPrintf("- ");
        if (strlen(context))
            mexPrintf("%s.%s: ", parent_context, context);
    }
}

void __attribute__((format (printf, 5, 6)))
pr_info(const struct ecat_slave *slave,
        const char_T *parent_context, const char_T *context,
        unsigned int indent, const char_T *fmt, ...)
{
    va_list ap;

    if (slave->debug_level < 1)
        return;

    va_start(ap, fmt);
    intro(slave, indent, parent_context, context);
    vprintf(fmt, ap);
    va_end(ap);
}

void __attribute__((format (printf, 5, 6)))
pr_debug(const struct ecat_slave *slave,
        const char_T *parent_context, const char_T *context,
        unsigned int indent, const char_T *fmt, ...)
{
    va_list ap;

    if (slave->debug_level < 2)
        return;

    va_start(ap, fmt);
    intro(slave, indent, parent_context, context);
    vprintf(fmt, ap);
    va_end(ap);
}

void __attribute__((format (printf, 5, 6)))
pr_error(const struct ecat_slave *slave,
        const char_T *parent_context, const char_T *context,
        unsigned int line_no, const char_T *fmt, ...)
{
    va_list ap;
    uint_T len;

    va_start(ap, fmt);
    len = snprintf(msg, sizeof(msg),
            (slave->debug_level < 2
             ?  "\nVariable context: %s.%s\n\n"
             :  "\nVariable context: %s.%s (Line %u)\n\n"),
            parent_context, context, line_no);
    len = vsnprintf(msg + len, sizeof(msg) - len, fmt, ap);
    ssSetErrorStatus(slave->S, msg);
    va_end(ap);
}

void __attribute__((format (printf, 5, 6)))
pr_warn(const struct ecat_slave *slave,
        const char_T *parent_context, const char_T *context,
        unsigned int line_no, const char_T *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    printf( (slave->debug_level < 2
                ? ("WARNING for EtherCAT slave '%s', "
                    "Config variable: %s.%s\n    ")
                : ("WARNING for EtherCAT slave '%s', "
                    "Config variable: %s.%s (Line %u)\n    ")),
            slave->S->path, parent_context, context, line_no);
    vprintf(fmt, ap);
    va_end(ap);
}

#define printf mexPrintf

int_T
get_data_type(const struct ecat_slave *slave,
        const char_T *p_ctxt, const char_T *context, uint_T line_no,
        int_T width, uint_T *data_type, boolean_T only_aligned,
        boolean_T only_unsigned)
{
    const char_T *reason = "Only byte aligned data types allowed";

    if (!width) {
        reason =  "No data type corresponds to width zero";
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
                reason = "Only values -8, -16 or -32 is allowed for "
                    "specifying signed data types";
                goto out;
                break;
        }
    }
    else if (width > 32) {
        reason = "Unsigned data type must be in the range [1..32].";
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
    pr_error(slave, p_ctxt, context, line_no,
            "Error determinig data type of symbol %i: %s",
            width, reason);
    return -1;
}

int_T
get_numeric_field(const struct ecat_slave *slave,
        const char_T *p_ctxt, unsigned int line_no,
        const mxArray *src, uint_T idx, boolean_T zero_allowed,
        boolean_T missing_allowed,
        const char_T *field_name, real_T *dest)
{
    const mxArray *field;

    if (!src) {
        pr_error(slave, p_ctxt, field_name, line_no,
                "Required source variable does not exist.");
        return -1;
    }

    field = mxGetField(src, idx, field_name);

    if (!field || !mxGetNumberOfElements(field)) {
        if (missing_allowed) {
            return 1;
        }
        else {
            pr_error(slave, p_ctxt, field_name, line_no,
                    "Required numeric field does not exist.");
            return -1;
        }
    }

    if (!mxIsNumeric(field)) {
        pr_error(slave, p_ctxt, field_name, line_no,
                "Expecting a numeric field, but it has %s",
                mxGetClassName(field));
        return -2;
    }

    *dest = mxGetScalar(field);

    if (!*dest && !zero_allowed) {
        pr_error(slave, p_ctxt, field_name, line_no,
                "Value is not allowed to be zero");
        return -3;
    }

    return 0;
}

#define CHECK_CALLOC(S,n,m,dest) \
    do {                                                                \
        if (!(n)) {                                                     \
                dest = NULL;                                            \
                break;                                                  \
        }                                                               \
      /*printf("Allocating %u bytes on line %u\n", (n)*(m), __LINE__);*/\
        dest = mxCalloc((n),(m));                                       \
        if (!(dest)) {                                                  \
            ssSetErrorStatus((S), "calloc() failure; no memory left."); \
            return -1;                                                  \
        }                                                               \
    } while(0)

int_T
get_string_field(const struct ecat_slave *slave,
        const char_T *p_ctxt, unsigned int line_no,
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

    CHECK_CALLOC(slave->S, len, 1, *dest);

    if (mxGetString(field, *dest, len)) {
        pr_error(slave, p_ctxt, field_name, line_no, "Expected a string");
        return -1;
    }

    return 0;

not_available:
    if (dflt == (char_T*)1)
        return 0;
    if (!dflt || !(len = strlen(dflt))) {
        *dest = NULL;
        if (!len) {
            pr_error(slave, p_ctxt, field_name, line_no, "Default string is empty");
            return -1;
        }
        else {
            pr_error(slave, p_ctxt, field_name, line_no,
                    "String is not allowed to be empty ('')");
            return -1;
        }
    }
    len++;
    CHECK_CALLOC(slave->S, len, 1, *dest);
    strcpy(*dest, dflt);
    return 0;
}

#define RETURN_ON_ERROR(val)    \
    do {                        \
        int_T err = (val);        \
        if (err < 0)            \
            return err;         \
    } while(0)

int_T
get_slave_address(struct ecat_slave *slave)
{
    SimStruct *S = slave->S;
    const char_T *ctxt = "ADDRESS";
    const mxArray *address       = ssGetSFcnParam(S, ADDRESS);
    real_T val;

    pr_debug(slave, NULL, NULL, 0,
            "--------------- Slave Address -----------------\n");
    RETURN_ON_ERROR(get_numeric_field(slave, ctxt, __LINE__, address,
                0, 1, 0, "Master", &val));
    slave->master = val;
    RETURN_ON_ERROR(get_numeric_field(slave, ctxt, __LINE__, address,
                0, 1, 0, "Domain", &val));
    slave->domain = val;
    RETURN_ON_ERROR(get_numeric_field(slave, ctxt, __LINE__, address,
                0, 1, 0, "Alias", &val));
    slave->alias = val;
    RETURN_ON_ERROR(get_numeric_field(slave, ctxt, __LINE__, address,
                0, 1, 0, "Position", &val));
    slave->position = val;

    pr_debug(slave, NULL, "", 1,
            "Master %u, Domain %u, Alias %u, Position %u\n",
            slave->master, slave->domain, slave->alias, slave->position);

    return 0;
}

int_T
get_sync_manager(SimStruct *S, const char_T *p_ctxt,
        unsigned int indent_level, const mxArray *ec_info,
        struct ecat_slave *slave)
{
    const mxArray *sm_info;
    struct sync_manager *sm;
    uint_T sm_idx;

    pr_debug(slave, NULL, NULL, 0,
            "--------------- SyncManager Configuration ------------\n");

    if (!ec_info)
        return 0;

    /* Check the number of devices */
    switch (mxGetNumberOfElements(ec_info)) {
        case 0:
            pr_error(slave,"Device count", "", __LINE__,
                    "EtherCATInfo structure does not have any devices");
            return -1;
        case 1:
            break;
        default:
            pr_warn(slave,"Device count", "", __LINE__,
                    "EtherCATInfo structure has %u devices. "
                    "Using first device\n",
                    mxGetNumberOfElements(ec_info));
    }

    sm_info = mxGetField(ec_info, 0, "Sm");
    if (!sm_info || !(sm_idx = mxGetNumberOfElements(sm_info))) {
        pr_debug(slave, p_ctxt, "Sm", indent_level,
                "SyncManager is empty or not specified.\n");
        return 0;
    }
    pr_debug(slave, p_ctxt, "Sm", indent_level,
            "SyncManager count = %u\n", sm_idx);

    CHECK_CALLOC(S, sm_idx, sizeof(struct sync_manager),
            slave->sync_manager);
    slave->sync_manager_end = slave->sync_manager + sm_idx;

    for (sm = slave->sync_manager, sm_idx = 0;
            sm != slave->sync_manager_end; sm++, sm_idx++) {
        int_T rc;
        unsigned int control_byte;
        real_T val;

        RETURN_ON_ERROR(rc = get_numeric_field(slave, p_ctxt, __LINE__, sm_info, sm_idx,
                    1, 1, "Virtual", &val));

        pr_debug(slave, NULL, "", indent_level+1,
                "Sm Index %u ", sm_idx);

        sm->index = sm_idx;

        if (!rc && val) {
            pr_debug(slave, NULL, NULL, 0,
                    "is virtual\n");
            sm->virtual = 1;
            sm->direction = EC_SM_VIRTUAL;

            /* Apparantly virtual Sm does not have a Control Byte */
            continue;
        }

        RETURN_ON_ERROR(rc = get_numeric_field(slave, p_ctxt, __LINE__, sm_info, sm_idx,
                    1, 1, "ControlByte", &val));

        if (rc && !sm->virtual) {
            /* SyncManager does not have a ControlByte and is not Virtual.
             * This is invalid, so ignore this SyncManager */
            pr_info(slave, NULL, NULL, 0,
                    "SyncManager %u does not have a ControlByte "
                    "and is not Virtual. Skipping...\n", sm_idx);

            continue;
        }

        control_byte = val;
        pr_debug(slave, NULL, NULL, 0,
                "has ControlByte %3u - ", control_byte);

        if (control_byte & 0x03) {
            pr_debug(slave, NULL, NULL, 0, "Not an IO SyncManager\n");
            continue;
        }

        switch (control_byte & 0x0C) {
            case 0:
                pr_debug(slave, NULL, NULL, 0, "Type input\n");
                sm->direction = EC_SM_INPUT;
                slave->input_sm_count++;
                break;
            case 4:
                pr_debug(slave, NULL, NULL, 0, "Type output\n");
                sm->direction = EC_SM_OUTPUT;
                slave->output_sm_count++;
                break;
            default:
                memset(sm, 0, sizeof(*sm));
                pr_info(slave, NULL, NULL, 0, "Unknown Type; Skipping...\n");
        }
    }

    pr_info(slave, NULL, NULL, 0,
            "Found %u SyncManagers:",
            slave->sync_manager_end - slave->sync_manager);
    for (sm = slave->sync_manager, sm_idx = 0;
            sm != slave->sync_manager_end; sm++, sm_idx++) {
        pr_info(slave, NULL, NULL, 0,
                " %u (%s),", sm_idx,
                (sm->direction == EC_SM_OUTPUT
                || sm->direction == EC_SM_INPUT)
                ? (sm->direction == EC_SM_OUTPUT ? "output" : "input")
                : (sm->direction == EC_SM_VIRTUAL ? "virtual" : "ignored"));
    }
    pr_info(slave, NULL, NULL, 0, "\n");

    return 0;
}

struct sync_manager *
check_pdo_syncmanager(const struct ecat_slave *slave,
        const char_T *p_ctxt, const char_T *ctxt,
        const struct pdo *pdo, unsigned int sm_idx) 
{
    unsigned int sm_idx_max = slave->sync_manager_end - slave->sync_manager;
    struct sync_manager *sm;
    unsigned int expected_sm_dir;

    if (sm_idx >= sm_idx_max) {
        pr_error(slave, p_ctxt, ctxt, __LINE__,
                "SyncManager index %u is not available",
                sm_idx);
        return NULL;
    }
    sm = &slave->sync_manager[sm_idx];

    expected_sm_dir = pdo->direction ? EC_SM_OUTPUT : EC_SM_INPUT;

    if (sm->virtual != pdo->virtual) {
        /* Error Virtual is different */
        pr_error(slave, p_ctxt, ctxt, __LINE__,
                "Conflict in virtual status: "
                "SyncManager %u %s virtual whereas Pdo #x%04X %s "
                "virtual",
                sm->index, sm->virtual ? "is" : "is not",
                pdo->index, pdo->virtual ? "is" : "is not");
        return NULL;
    }

    if (sm->virtual) {
        /* Actually don't really know whether the above is
         * corrent >:-). Report an error here so that it has to
         * be checked in future */
        pr_error(slave, p_ctxt, ctxt, __LINE__,
                "Configuration of virtual SyncManagers has not "
                "been verified yet. Contact support");
        return NULL;

        if (sm->direction == EC_SM_INPUT
                || sm->direction == EC_SM_OUTPUT) {
            if (sm->direction != expected_sm_dir) {
                /* conflict in virtual directions */
                pr_error(slave, p_ctxt, ctxt, __LINE__,
                        "Virtual SyncManager %u has direction %s. "
                        "Assigning a %sPdo to this SyncManager "
                        "is not possible",
                        sm->index,
                        (sm->direction == EC_SM_INPUT
                         ? "input" : "output"),
                        pdo->direction ? "Rx" : "Tx");
                return NULL;
            }
        }
        else {
            /* Direction of virtual syncmanager has not been
             * chosen yet - set it now */
            sm->direction = expected_sm_dir;
        }
    }
    else if (sm->direction != EC_SM_INPUT
            && sm->direction != EC_SM_OUTPUT) {
        /* Incorrect sm direction specified */
        pr_error(slave, p_ctxt, ctxt, __LINE__,
                "SyncManager %u is neither an input nor an output "
                "SyncManager. Choose another one",
                sm->index);
        return NULL;
    }
    else if (sm->direction != expected_sm_dir) {
        /* Incorrect sm direction specified */
        pr_error(slave, p_ctxt, ctxt, __LINE__,
                "SyncManager %u is an %s SyncManager. "
                "Assigning a %sPdo #x%04X to this SyncManager "
                "is not possible",
                sm->index,
                (sm->direction == EC_SM_INPUT
                 ? "input" : "output"),
                pdo->direction ? "Rx" : "Tx",
                pdo->index);
        return NULL;
    }
    return sm;

}


int_T
get_slave_pdo(struct ecat_slave *slave,
        const char_T *p_ctxt, unsigned int indent_level,
        const mxArray *pdo_info, unsigned int pdo_count,
        unsigned int pdo_dir, const char_T *dir_str)
{
    SimStruct *S = slave->S;
    unsigned int level;
    struct pdo *pdo;
    char_T context[100];
    real_T val;
    uint_T pdo_idx;
    uint_T i;

    pr_debug(slave, NULL, NULL, 0, "--------------- %s.%s ------------\n",
            p_ctxt, dir_str);

    if (!pdo_count) {
        pr_debug(slave, p_ctxt, dir_str, indent_level,
                "Pdo has no elements\n");
        return 0;
    }

    if (!mxIsStruct(pdo_info)) {
        pr_error(slave, p_ctxt, dir_str, __LINE__,
                "mxArray is not a structure\n");
        return -1;
    }

    for (pdo_idx = 0, pdo = slave->pdo_end;
            pdo_idx < pdo_count; pdo_idx++, pdo++) {
        const mxArray *exclude_info;
        uint_T exclude_count;
        const mxArray *pdo_entry_info;
        uint_T entry_info_count;
        int_T rc;

        level = indent_level+1;

        snprintf(context, sizeof(context), "%s(%u)", dir_str, pdo_idx+1);

        RETURN_ON_ERROR(get_numeric_field(slave, p_ctxt, __LINE__, pdo_info, pdo_idx,
                    1, 0, "Index", &val));
        pdo->index = val;
        pdo->direction = pdo_dir;

        pr_debug(slave, p_ctxt, context, indent_level,
                "Found %s Index #x%04X\n", dir_str, pdo->index);

        val = 0;
        RETURN_ON_ERROR(get_numeric_field(slave, p_ctxt, __LINE__,
                    pdo_info, pdo_idx, 1, 1, "Virtual", &val));
        pdo->virtual = val > 0.0;

        switch (get_numeric_field(slave, p_ctxt, __LINE__, pdo_info, pdo_idx, 1, 1,
                    "Mandatory", &val)) {
            case 0:
                pdo->mandatory = val;
                break;
            case 1:
                pdo->mandatory = 0;
                break;
            default:
                return -1;
        }

        /* Get the SyncManager that this pdo is mapped to.
         * Not specifying the SyncManager is not a good idea,
         * Although it is possible to assume the right SyncManager if
         * there is only one to choose from. */
        RETURN_ON_ERROR(rc = get_numeric_field(slave, p_ctxt, __LINE__,
                    pdo_info, pdo_idx, 1, 1, "Sm", &val));
        if (rc) {
            pr_debug(slave, NULL, "", indent_level, "SM not specified\n");
        }
        else {
            pdo->sm = check_pdo_syncmanager(slave, NULL, "", pdo, val);
            if (!pdo->sm)
                return -1;
        }

        pr_debug(slave, NULL, "", indent_level+1,
                "Attributes: Sm='%i' Virtual='%u', Mandatory='%u'\n",
                pdo->sm ? pdo->sm->index : -1, 
                pdo->virtual, pdo->mandatory);

        exclude_info = mxGetField(pdo_info, pdo_idx, "Exclude");
        if (exclude_info
                && (exclude_count = mxGetNumberOfElements(exclude_info))) {
            real_T *val = mxGetPr(exclude_info);

            snprintf(context, sizeof(context),
                    "%s(%u).Exclude", dir_str, pdo_idx+1);

            if (!val || !mxIsNumeric(exclude_info)) {
                pr_error(slave, p_ctxt, context, __LINE__,
                        "Field Exclude is not a valid numeric array.");
                return -1;
            }

            CHECK_CALLOC(S, exclude_count, sizeof(uint_T),
                    pdo->exclude_index);
            pdo->exclude_index_end = pdo->exclude_index + exclude_count;

            pr_debug(slave, NULL, "", indent_level, "  Exclude PDOS:");
            for (i = 0; i < exclude_count; i++) {
                pdo->exclude_index[i] = val[i];
                pr_debug(slave, NULL, NULL, 0,
                        " #x%04X", pdo->exclude_index[i]);
            }
            pr_debug(slave, NULL, NULL, 0, "\n");
        }

        pdo_entry_info = mxGetField(pdo_info, pdo_idx, "Entry");
        entry_info_count =
            pdo_entry_info ? mxGetNumberOfElements(pdo_entry_info) : 0;
        if (entry_info_count) {
            struct pdo_entry *pdo_entry;
            uint_T entry_idx;

            snprintf(context, sizeof(context),
                    "%s(%u).Entry", dir_str, pdo_idx+1);

            if (!mxIsStruct(pdo_entry_info)) {
                pr_error(slave, p_ctxt, context, __LINE__,
                        "This field is not a Matlab structure");
                return -1;
            }

            CHECK_CALLOC(S, entry_info_count, sizeof(struct pdo_entry),
                    pdo_entry);
            pdo->entry     = pdo_entry;
            pdo->entry_end = pdo_entry + entry_info_count;

            for (entry_idx = 0; entry_idx < entry_info_count;
                    entry_idx++, pdo_entry++) {
                int_T dummy_datatype;

                snprintf(context, sizeof(context),
                        "%s(%u).Entry(%u)", dir_str, pdo_idx+1, entry_idx+1);

                RETURN_ON_ERROR(get_numeric_field(slave, p_ctxt, __LINE__,
                            pdo_entry_info, entry_idx, 1, 0, "Index", &val));
                pdo_entry->index = val;
                RETURN_ON_ERROR(get_numeric_field(slave, p_ctxt, __LINE__,
                            pdo_entry_info, entry_idx, 1, 0, "BitLen", &val));
                pdo_entry->bitlen = val;

                if (!pdo_entry->index) {
                    pr_debug(slave, NULL, "", indent_level+1,
                            "Found PDO Entry Index #x0000,                "
                            "BitLen %u   (spacer)  \n",
                            pdo_entry->bitlen);
                    continue;
                }

                RETURN_ON_ERROR(get_numeric_field(slave, p_ctxt, __LINE__,
                            pdo_entry_info, entry_idx, 0, 0, "SubIndex",
                            &val));
                pdo_entry->subindex = val;

                RETURN_ON_ERROR(get_numeric_field(slave, p_ctxt, __LINE__,
                            pdo_entry_info, entry_idx, 0, 0, "DataType",
                            &val));
                pdo_entry->datatype = val;
                RETURN_ON_ERROR(get_data_type(slave, p_ctxt, context, __LINE__,
                            pdo_entry->datatype, &dummy_datatype, 0, 0));
                pr_debug(slave, NULL, "", indent_level+1,
                        "Found PDO Entry Index #x%04X, SubIndex #x%02X, "
                        "BitLen %u, DataType %i\n",
                        pdo_entry->index, pdo_entry->subindex,
                        pdo_entry->bitlen, pdo_entry->datatype);
            }
        }
    }

    slave->pdo_end = pdo;

    return 0;
}

int_T
get_ethercat_info(struct ecat_slave *slave)
{
    SimStruct *S = slave->S;
    const mxArray *ec_info = ssGetSFcnParam(S, ETHERCAT_INFO);
    const mxArray
        *vendor = NULL,
        *descriptions = NULL,
        *devices = NULL,
        *device = NULL,
        *type = NULL;
    const mxArray *rx_pdo, *tx_pdo;
    uint_T rx_pdo_count, tx_pdo_count;
    const char_T *context;
    real_T val;

    if (!mxIsStruct(ec_info))
        return 0;

    /***********************
     * Get Slave address
     ***********************/
    do {
        if (!(vendor       = mxGetField( ec_info,      0, "Vendor")))
            break;
        if (!(descriptions = mxGetField( ec_info,      0, "Descriptions")))
            break;
        if (!(devices      = mxGetField( descriptions, 0, "Devices")))
            break;
        if (!(device       = mxGetField( devices,      0, "Device")))
            break;
        if (!(type         = mxGetField( device,       0, "Type")))
            break;
    } while(0);

    pr_debug(slave, NULL, NULL, 0,
            "--------------- Parsing EtherCATInfo ------------\n");

    context = "EtherCATInfo.Vendor";
    RETURN_ON_ERROR(get_numeric_field(slave, context, __LINE__, vendor, 0, 0, 0,
                "Id", &val));
    slave->vendor_id = val;

    context = "EtherCATInfo.Descriptions.Devices.Device.Type";
    RETURN_ON_ERROR(get_numeric_field(slave, context, __LINE__, type, 0, 0, 0,
                "ProductCode", &val));
    slave->product_code = val;
    RETURN_ON_ERROR(get_numeric_field(slave, context, __LINE__, type, 0, 1, 0,
                "RevisionNo", &val));
    slave->revision_no = val;
    RETURN_ON_ERROR(get_string_field(slave, context, __LINE__, type, 0,
                "Type", "Unspecified", &slave->type));

    pr_debug(slave, NULL, NULL, 0,
            "  VendorId %u\n", slave->vendor_id);
    pr_debug(slave, NULL, NULL, 0,
            "  ProductCode #x%08X, RevisionNo #x%08X, Type '%s'\n",
            slave->product_code, slave->revision_no, slave->type);

    /***********************
     * Get SyncManager setup
     ***********************/
    context = "EtherCATInfo.Descriptions.Devices.Device";
    RETURN_ON_ERROR(get_sync_manager(S, context, 1, device, slave));

    /***********************
     * Pdo Setup
     ***********************/
    tx_pdo = mxGetField(device, 0, "TxPdo");
    rx_pdo = mxGetField(device, 0, "RxPdo");

    tx_pdo_count = tx_pdo ? mxGetNumberOfElements(tx_pdo) : 0;
    rx_pdo_count = rx_pdo ? mxGetNumberOfElements(rx_pdo) : 0;

    CHECK_CALLOC(S, tx_pdo_count + rx_pdo_count,
            sizeof(struct pdo), slave->pdo);
    slave->pdo_end = slave->pdo;

    RETURN_ON_ERROR(get_slave_pdo(slave, context, 1, rx_pdo, rx_pdo_count, 1, "RxPdo"));
    RETURN_ON_ERROR(get_slave_pdo(slave, context, 1, tx_pdo, tx_pdo_count, 0, "TxPdo"));

    return 0;
}

int_T
get_sdo_config(SimStruct *S, struct ecat_slave *slave)
{
    const mxArray *sdo_config = ssGetSFcnParam(S, SDO_CONFIG);
    struct sdo_config *sdo_config_ptr;
    const char_T *param = "SDO_CONFIG";
    char_T context[100];
    uint_T i;
    uint_T sdo_config_count;

pr_debug(slave, NULL, NULL, 0,
        "--------------- Parsing SDO Configuration ------------\n");

    sdo_config_count = mxGetM(sdo_config);

    CHECK_CALLOC(S, sdo_config_count, sizeof(*slave->sdo_config),
            slave->sdo_config);
    slave->sdo_config_end = slave->sdo_config + sdo_config_count;

    for (i = 0, sdo_config_ptr = slave->sdo_config;
            i < sdo_config_count; i++, sdo_config_ptr++) {
        real_T val;
        snprintf(context, sizeof(context), "%s(%u)", param, i+1);

        RETURN_ON_ERROR(get_numeric_field(slave, context, __LINE__,
                    sdo_config, i, 0, 0, "Index", &val));
        sdo_config_ptr->index = val;

        RETURN_ON_ERROR(get_numeric_field(slave, context, __LINE__,
                    sdo_config, i, 1, 0, "SubIndex", &val));
        sdo_config_ptr->subindex = val;

        RETURN_ON_ERROR(get_numeric_field(slave, context, __LINE__,
                    sdo_config, i, 0, 0, "BitLen", &val));
        RETURN_ON_ERROR(get_data_type(slave, context, "BitLen", __LINE__,
                    val, &sdo_config_ptr->datatype, 1, 1));

        RETURN_ON_ERROR(get_numeric_field(slave, context, __LINE__,
                    sdo_config, i, 1, 0, "Value", &val));
        sdo_config_ptr->value = val;
    }

    pr_info(slave, NULL, NULL, 0,
            "Found %u SDO configuration options\n", sdo_config_count);

    return 0;
}

struct pdo *
find_pdo_entry(const struct ecat_slave *slave,
        const char_T *p_ctxt, const char_T *ctxt,
        unsigned int line_no, unsigned int indent,
        uint16_T pdo_entry_index, uint8_T pdo_entry_subindex,
        struct pdo_entry **pdo_entry_ptr)
{
    struct pdo *pdo, *found_pdo = NULL;
    struct pdo_entry *pdo_entry;
    *pdo_entry_ptr = NULL;

    for (pdo = slave->pdo; pdo != slave->pdo_end; pdo++) {
        for (pdo_entry = pdo->entry;
                pdo_entry != pdo->entry_end; pdo_entry++) {
            if (pdo_entry->index == pdo_entry_index
                    && pdo_entry->subindex == pdo_entry_subindex) {
                if (pdo->exclude_from_config)
                    found_pdo = pdo;
                else
                    goto found;
            }
        }
    }

    if (found_pdo) {
        pr_error(slave, p_ctxt, ctxt, line_no,
                "PDO Entry Index #x%04X, " "PDO Entry Subindex #x%02X "
                "was found at Pdo #x%04X, but this Pdo is explicitly "
                "excluded by user configuration.",
                pdo_entry_index, pdo_entry_subindex, found_pdo->index);
    }
    else {
        pr_error(slave, p_ctxt, ctxt, line_no,
                "PDO Entry Index #x%04X, "
                "PDO Entry Subindex #x%02X was not found.",
                pdo_entry_index, pdo_entry_subindex);
    }
    return NULL;

found:
    *pdo_entry_ptr = pdo_entry;
    return pdo;
}

int_T
get_parameter_values(const struct ecat_slave *slave,
        const char_T *p_ctxt,
        const mxArray* port_spec, uint_T port,
        const char_T* field, uint_T port_width, real_T **dest,
        char_T **name)
{
    const mxArray* src = mxGetField(port_spec, port, field);
    char_T name_field[sizeof(field) + 10];
    uint_T count;
    real_T *values;

    if (!src || !(count = mxGetNumberOfElements(src)))
        return 0;

    if (!mxIsNumeric(src)) {
        pr_error(slave, p_ctxt, field, __LINE__,
                "Parameter is not a numeric vector");
        return -1;
    }

    if (count != 1 && count != port_width) {
        pr_error(slave, p_ctxt, field, __LINE__,
                "Specified vector length %u does not match that of the "
                "port %u. It must either be a scalar or a vector of the "
                "same length", count, port_width);
        return -1;
    }

    CHECK_CALLOC(slave->S, count, sizeof(real_T), *dest);

    values = mxGetPr(src);
    if (!values) {
        pr_error(slave, p_ctxt, field, __LINE__,
                "An internal error occurred while retrieving the  "
                "values of the parameter");
        return -1;
    }

    memcpy(*dest, values, sizeof(real_T)*count);

    snprintf(name_field, sizeof(name_field), "%sName", field);

    RETURN_ON_ERROR(get_string_field(slave, field, __LINE__,
                port_spec, port, name_field, (char_T*)1, name));

    return count;
}

int_T
get_pdo_config(struct ecat_slave *slave,
        const char_T *p_ctxt, unsigned int p_indent, const mxArray *iospec)
{
    const mxArray *pdo_spec = mxGetField(iospec, 0, "Pdo");
    const mxArray *sm_assign;
    const mxArray *pdo_select;
    real_T val = 0.0;
    int_T rc;
    uint_T count, i;
    char_T ctxt[100];

    pr_debug(slave, NULL, NULL, 0,
        "--------------- Custom slave configuration ------------\n");


    if (!pdo_spec) {
        pr_info(slave, NULL, NULL, 0,
                "No custom specification found\n");
        return 0;
    }

    snprintf(ctxt, sizeof(ctxt), "%s.Pdo", p_ctxt);

    val = 0.0;
    RETURN_ON_ERROR(rc = get_numeric_field(slave, ctxt, __LINE__, pdo_spec, 0, 1, 1,
                "Variant", &val));
    slave->pdo_configuration_variant = val;
    if (!rc) {
        pr_debug(slave, ctxt, "Variant", 1, "Configuration variant %u\n",
                slave->pdo_configuration_variant);
    }

    val = 0.0;
    RETURN_ON_ERROR(get_numeric_field(slave, ctxt, __LINE__, pdo_spec, 0, 1, 1,
                "Unique", &val));
    slave->unique_config = val != 0.0;
    if (slave->unique_config) {
        pr_debug(slave, ctxt, "Unique", 1,
                "The configuration for this slave is unique and will not "
                "be shared with others\n");
    }

    sm_assign = mxGetField(pdo_spec, 0, "SmAssignment");
    if (sm_assign && (count = mxGetM(sm_assign))) {
        real_T *pdo_val, *sm_idx_val;
        uint_T i;

        pr_debug(slave, ctxt, "SmAssignment", 1,
                "Found specification for %u Pdo Indices\n", count);

        if (!mxIsNumeric(sm_assign)
                || mxGetN(sm_assign) != 2
                || !(pdo_val = mxGetPr(sm_assign))) {
            pr_error(slave, ctxt, "SmAssignment", __LINE__,
                    "Value is not numeric M-by-2 array");
            return -1;
        }
        sm_idx_val = pdo_val + count;

        for (i = 0; i < count; i++) {
            uint_T pdo_idx = pdo_val[i];
            struct sync_manager *sm;
            struct pdo *pdo;

            snprintf(ctxt, sizeof(ctxt),
                    "Pdo.SmAssignment(%u) = [#x%04X, %i]", i+1,
                    (int_T)pdo_val[i], (int_T)sm_idx_val[i]);
            pr_debug(slave, NULL, "", 2,
                    "Trying to reassign PDO #x%04X to SyncManager %u\n",
                    (int_T)pdo_val[i], (int_T)sm_idx_val[i]);

            if (pdo_val[i] <= 0.0 && sm_idx_val[i] < 0.0) {
                pr_error(slave, p_ctxt, ctxt, __LINE__,
                        "Negative values not allowed in SyncManager "
                        "assignment specifiction");
                return -1;
            }

            for (pdo = slave->pdo; pdo != slave->pdo_end; pdo++) {
                if (pdo->index == pdo_idx)
                    break;
            }

            if (pdo == slave->pdo_end) {
                /* The pdo specified is not found */
                pr_error(slave, p_ctxt, ctxt, __LINE__,
                        "The Pdo index #x%04X was not found\n", pdo_idx);
                return -1;
            }

            sm = check_pdo_syncmanager(slave, p_ctxt, 
                    ctxt, pdo, sm_idx_val[i]);
            if (!sm)
                return -1;

            if (pdo->sm == sm) {
                /* The pdo was already specified */
                pr_warn(slave, p_ctxt, ctxt, __LINE__,
                        "The PDO #x%04X has already been assigned to "
                        "SyncManager %u previously. "
                        "Remove it from this manual configuration list\n",
                        pdo->index, sm->index);
            }
            else if (pdo->sm) {
                /* conflict in spec sm is already assigned */
                pr_warn(slave, p_ctxt, ctxt, __LINE__,
                        "Reassigning Pdo #x%04X to SyncManager %u; "
                        "previously it was assigned to SyncManager %u\n",
                        pdo->index, sm->index, pdo->sm->index);
            }

            pr_debug(slave, NULL, "", 1,
                    "Reassigned Pdo #x%04X to SyncManager %u\n",
                    pdo->index, sm->index);

            pdo->sm = sm;
            return 0;
        }
    }


    pdo_select = mxGetField(pdo_spec, 0, "Exclude");
    if (pdo_select
            && (count = mxGetNumberOfElements(pdo_select))) {
        real_T *val;
        struct pdo *pdo;

        snprintf(ctxt, sizeof(ctxt), "%s.Pdo", p_ctxt);

        if (!mxIsNumeric(pdo_select) || !(val = mxGetPr(pdo_select))) {
            pr_error(slave, ctxt, "Exclude", __LINE__,
                    "Value is not a numeric vector");
            return -1;
        }
        pr_debug(slave, NULL, "", 1,
                "Found PDO selection list with %u elements\n",
                count);
        for (i = 0; i < count; i++) {
            uint_T idx = abs(val[i]);
            char_T idx_ctxt[20];
            snprintf(idx_ctxt, sizeof(idx_ctxt), "Exclude(%u)", i+1);

            for (pdo = slave->pdo; pdo != slave->pdo_end; pdo++) {
                if (pdo->index == idx) {
                    if (pdo->mandatory) {
                        pr_error(slave, ctxt, idx_ctxt, __LINE__,
                                "Cannot exclude mandatory Pdo #x%04X ",
                                pdo->index);
                        return -1;
                    }
                    pdo->exclude_from_config = 1;
                    pr_debug(slave, NULL, "", 2,
                            "Excluding Pdo #x%04X in configuration\n", idx);
                    break;
                }
            }
            if (pdo == slave->pdo_end) {
                pr_warn(slave, ctxt, idx_ctxt, __LINE__,
                        "Pdo Index #x%04X was not found in configuration\n",
                        idx);
            }
        }
    }

    return 0;
}

int_T
get_ioport_config(SimStruct *S, struct ecat_slave *slave)
{
    const mxArray * const io_spec = ssGetSFcnParam(S, PORT_SPEC);
    const mxArray *port_spec;
    const char_T *param = "IO_SPEC";

    struct io_port *port;
    char_T context[100];
    real_T val;
    uint_T io_spec_idx, io_spec_count;

    pr_debug(slave, NULL, NULL, 0,
            "--------------- Parsing IOSpec ------------\n");

    if (!io_spec) {
        pr_info(slave, NULL, NULL, 0,
                "No block input and output configuration spec "
                "was found\n");
        return 0;
    }

    /* Parse the parameters IOSpec.Pdo first */
    RETURN_ON_ERROR(get_pdo_config(slave, param, 2, io_spec));

    if (!(port_spec = mxGetField(io_spec, 0, "Port"))
            || !(io_spec_count = mxGetNumberOfElements(port_spec))) {
        return 0;
    }

    CHECK_CALLOC(S, io_spec_count, sizeof(struct io_port),
            slave->io_port);
    slave->io_port_end = slave->io_port + io_spec_count;

    pr_debug(slave, NULL, "", 1, "Processing %u IO Ports\n", io_spec_count);

    for (io_spec_idx = 0, port = slave->io_port;
            io_spec_idx < io_spec_count; io_spec_idx++, port++) {
        const mxArray *pdo_entry_spec =
            mxGetField(port_spec, io_spec_idx, "PdoEntry");
        struct pdo_entry **pdo_entry_ptr;
        struct pdo *pdo;
        uint_T pdo_entry_idx;
        boolean_T struct_access;
        real_T *pdo_entry_index_field;
        real_T *pdo_entry_subindex_field;
        uint_T pdo_data_type;
        char_T port_ctxt[100];
        const char_T *pdo_entry_ctxt = "PdoEntry";
        int_T rc;

        snprintf(port_ctxt, sizeof(port_ctxt),
                "%s.Port(%u)", param, io_spec_idx+1);

        pr_debug(slave, NULL, "", 2,
                "Processing IOSpec %u\n", io_spec_idx+1);

        if (!pdo_entry_spec) {
            pr_error(slave, port_ctxt, pdo_entry_ctxt, __LINE__,
                    "Parameter does not exist");
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
                pr_error(slave, port_ctxt, pdo_entry_ctxt, __LINE__,
                        "Structure is empty.\n");
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
                pr_error(slave, port_ctxt, pdo_entry_ctxt, __LINE__,
                        "Parameter is not a M-by-2 numeric array");
                return -1;
            }
        }
        else {
            pr_error(slave, port_ctxt, pdo_entry_ctxt, __LINE__,
                    "Ignoring unknown specification.\n");
            return -1;
        }

        CHECK_CALLOC(S, port->pdo_entry_count, sizeof(struct pdo_entry*),
                port->pdo_entry);


        /* Get the list of pdo entries that are mapped to this port */
        for (pdo_entry_idx = 0, pdo_entry_ptr = port->pdo_entry;
                pdo_entry_idx < port->pdo_entry_count;
                pdo_entry_idx++, pdo_entry_ptr++) {
            uint16_T pdo_entry_index;
            uint8_T pdo_entry_subindex;
            char_T pdo_entry_array_ctxt[50];
            uint_T vector_len;

            snprintf(pdo_entry_array_ctxt, sizeof(pdo_entry_array_ctxt),
                    "%s(%u)", pdo_entry_ctxt, pdo_entry_idx+1);

            if (struct_access) {
                char_T struct_ctxt[100];
                snprintf(struct_ctxt, sizeof(struct_ctxt),
                        "%s.%s", port_ctxt, pdo_entry_array_ctxt);

                RETURN_ON_ERROR(get_numeric_field(slave, struct_ctxt, __LINE__,
                            pdo_entry_spec, pdo_entry_idx, 0, 0,
                            "PdoEntryIndex", &val));
                pdo_entry_index = val;

                RETURN_ON_ERROR(get_numeric_field(slave, struct_ctxt, __LINE__,
                            pdo_entry_spec, pdo_entry_idx, 0, 0,
                            "PdoEntrySubIndex", &val));
                pdo_entry_subindex = val;
            }
            else {

                pdo_entry_index    = *pdo_entry_index_field++;
                pdo_entry_subindex = *pdo_entry_subindex_field++;
            }

            /* Get the pdo and pdo entry pointers for the specified
             * indices */
            if (!(pdo = find_pdo_entry(slave,
                            port_ctxt, pdo_entry_array_ctxt,
                            __LINE__, 2, pdo_entry_index, pdo_entry_subindex,
                            pdo_entry_ptr))) {
                return -1;
            }
            pdo->use_count++;

            if (pdo_entry_idx) {
                /* This is not the first Pdo Entry being mapped to this
                 * port, so check that the direction is consistent */
                if (port->direction != pdo->direction) {
                    pr_error(slave, port_ctxt, pdo_entry_array_ctxt,
                            __LINE__,
                            "Cannot mix Pdo directions on the same port\n"
                            "The first Pdo Entry mapped to this port is "
                            "a %sPdo, whereas this is a %sPdo.",
                            port->direction ? "Rx" : "Tx",
                            pdo->direction ? "Rx" : "Tx");
                    return -1;
                }
            }
            else {
                /* The Pdo direction of the first Pdo Entry mapped to this
                 * port determines the port's direction */
                port->direction = pdo->direction;
            }

            if (port->pdo_entry[0]->datatype !=
                    (*pdo_entry_ptr)->datatype) {
                pr_error(slave, port_ctxt, pdo_entry_array_ctxt, __LINE__,
                        "Cannot mix datatypes on the same port.\n"
                        "The first Pdo Entry mapped to this port has "
                        "data type %i, whereas this Pdo Entry has "
                        "datatype %i",
                        port->pdo_entry[0]->datatype,
                        (*pdo_entry_ptr)->datatype);
                return -1;
            }

            if (!port->data_bit_len) {
                /* data_bit_len was not specified in the port definition.
                 * Set this to the data type specified in the Pdo Entry */
                port->data_bit_len = abs((*pdo_entry_ptr)->datatype);
                pr_debug(slave, NULL, "", 3,
                        "Port data bit length was not specified. Use the "
                        "Pdo's bit length of %u bits\n",
                        port->data_bit_len);
            }

            if ((*pdo_entry_ptr)->bitlen % port->data_bit_len) {
                pr_error(slave, port_ctxt, pdo_entry_array_ctxt, __LINE__,
                        "The BitLen of this Pdo Entry is %u. This is not "
                        "an integer multiple of BitLen %u",
                        (*pdo_entry_ptr)->bitlen, port->data_bit_len);
                return -1;
            }

            vector_len = (*pdo_entry_ptr)->bitlen / port->data_bit_len;
            port->width += vector_len;
            pr_debug(slave, NULL, "", 3,
                    "Added Pdo Entry #x%04X.%02X to %s port %u. "
                    "Vector length %u\n",
                    (*pdo_entry_ptr)->index,
                    (*pdo_entry_ptr)->subindex,
                    port->direction ? "input" : "output",
                    (port->direction
                     ? slave->input_port_count
                     : slave->output_port_count) + 1,
                    port->width);
        }

        if (port->direction) {
            slave->input_port_count++;
            slave->input_pdo_entry_count  += port->pdo_entry_count;
        }
        else {
            slave->output_port_count++;
            slave->output_pdo_entry_count += port->pdo_entry_count;
        }

        /* Make sure that the PDO has a supported data type */
        RETURN_ON_ERROR(get_data_type(slave, port_ctxt, "PdoEntry(1)",
                    __LINE__,
                    port->pdo_entry[0]->datatype,
                    &pdo_data_type, 0, 0));

        /* If PdoDataType is specified, use it, otherwise it is guessed
         * by the DataBitLen spec below */
        RETURN_ON_ERROR(rc = get_numeric_field(slave, context, __LINE__,
                    port_spec, io_spec_idx, 0, 1, "PdoDataType", &val));
        if (rc) {
            /* Pdo data type was not specified, so use that determined
             * above */
            port->pdo_data_type = pdo_data_type;
        }
        else {
            if (port->data_bit_len > (uint_T)abs(val)) {
                pr_error(slave, port_ctxt, "PdoDataType", __LINE__,
                        "Bit length of pdo data type specified (%i) "
                        "is too small for for the data type assigned "
                        "of the port (%u)",
                        (int_T)val, port->data_bit_len);
                return -1;
            }
            RETURN_ON_ERROR(get_data_type(slave, port_ctxt, "PdoDataType",
                        __LINE__, val, &port->pdo_data_type, 1, 0));
        }

        if (port->pdo_data_type == SS_BOOLEAN)
            port->pdo_data_type = SS_UINT8;

        /* If PdoFullScale is specified, the port data type is double.
         * If port->pdo_full_scale == 0.0, it means that the port data
         * type is specified by PortBitLen */
        switch(get_numeric_field(slave, port_ctxt, __LINE__, port_spec,
                    io_spec_idx, 0, 1, "PdoFullScale", &val)) {
            case 0:
                port->pdo_full_scale = val;
                port->data_type = SS_DOUBLE;
                break;
            case 1:
                /* Field was not available - not an error */
                port->data_type = pdo_data_type;
                break;
            default:
                return -1;
        }

        if (port->data_bit_len % 8 || port->data_bit_len == 24) {
            port->bitop = 1;
        }

        if (port->pdo_full_scale) {
            RETURN_ON_ERROR(port->gain_count = get_parameter_values(slave,
                        pdo_entry_ctxt, port_spec, io_spec_idx, "Gain",
                        port->width, &port->gain_values,
                        &port->gain_name));

            /* No offset and filter for input ports */
            if (!port->direction) {
                RETURN_ON_ERROR(port->offset_count = get_parameter_values(slave,
                            pdo_entry_ctxt, port_spec, io_spec_idx, "Offset",
                            port->width, &port->offset_values,
                            &port->offset_name));
                RETURN_ON_ERROR(port->filter_count = get_parameter_values(slave,
                            pdo_entry_ctxt, port_spec, io_spec_idx, "Filter",
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
slave_mem_op(struct ecat_slave *slave, void (*method)(void*))
{
    struct io_port *port;
    struct sync_manager *sm;
    struct pdo *pdo;

    for (port = slave->io_port; port != slave->io_port_end; port++) {
        (*method)(port->pdo_entry);
        (*method)(port->gain_values);
        (*method)(port->gain_name);
        (*method)(port->offset_values);
        (*method)(port->offset_name);
        (*method)(port->filter_values);
        (*method)(port->filter_name);
    }
    (*method)(slave->io_port);

    for (sm = slave->sync_manager; sm != slave->sync_manager_end; sm++)
        (*method)(sm->pdo_ptr);
    (*method)(slave->sync_manager);

    for (pdo = slave->pdo; pdo != slave->pdo_end; pdo++)
        (*method)(pdo->entry);
    (*method)(slave->pdo);

    (*method)(slave->sdo_config);
    (*method)(slave->type);

    (*method)(slave);
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

int_T
check_pdo_configuration(SimStruct *S, struct ecat_slave *slave)
{
    struct pdo *pdo, *pdoX;
    struct sync_manager *sm;
    const char_T *ctxt = "Pdo Configuration Check";

    for (pdo = slave->pdo; pdo != slave->pdo_end; pdo++) {

        if (pdo->exclude_from_config && pdo->use_count) {
            pr_error(slave, ctxt, "", __LINE__,
                    "Problem with PDO #x%04X: Pdo was explicitly excluded "
                    "from configuration, however somehow it was assigned "
                    "to a port. This cannot be.", pdo->index);
            return -1;
        }
        pr_info(slave, NULL, "", 1, "Checking PDO #x%04X; use count %u\n",
                pdo->index, pdo->use_count);

        if (pdo->exclude_from_config)
            continue;

        if (!pdo->sm) {
            if (pdo->use_count) {
                /* Try to assign a SyncManager to the PDO */

                uint_T pdo_sm_dir = 
                    pdo->direction ? EC_SM_OUTPUT : EC_SM_INPUT;
                uint_T sm_dir_count = pdo->direction 
                    ? slave->output_sm_count : slave->input_sm_count;

                for (sm = slave->sync_manager; 
                        sm != slave->sync_manager_end; sm++ ) {
                    if (sm->virtual != pdo->virtual 
                            || sm->direction != pdo_sm_dir)
                        continue;
                    if (1 == sm_dir_count) {
                        pdo->sm = sm;
                        pr_info(slave, ctxt, "", 1,
                                "Automatically assigning SyncManager %u "
                                "for PDO Index #x%04X\n", 
                                sm->index, pdo->index);
                        break;
                    }
                    pr_error(slave, ctxt, "", __LINE__,
                            "Pdo #x%04X was neither assigned a SyncManager "
                            "in the EtherCATInfo nor using "
                            "the custom SmAssignment option.\n"
                            "Since the slave has %u %s SyncManagers "
                            "an automatic choice is not possible",
                            pdo->index, sm_dir_count, 
                            (sm->direction == EC_SM_INPUT 
                             ? "input" : "output"));
                    return -1;
                }
            }
            else {
                pr_info(slave, ctxt, "", 1,
                        "Automatically deselecting PDO #x%04X from the "
                        "SyncManager configuration because it does not "
                        "have a SyncManager assigned and it is unused\n",
                        pdo->index);
                pdo->exclude_from_config = 1;
                slave->unique_config = 1;
                continue;
            }
        }

        slave->pdo_count++;
        slave->pdo_entry_count += pdo->entry_end - pdo->entry;
        pdo->sm->pdo_ptr_end++;

        /* The PDO will be condigured. Make sure that it does not exist
         * in the exclusion list of another Pdo */
        for (pdoX = slave->pdo; pdoX != pdo; pdoX++) {
            uint_T *exclude_pdo;

            if (pdoX->exclude_from_config)
                continue;

            for (exclude_pdo = pdoX->exclude_index;
                    exclude_pdo != pdoX->exclude_index_end; exclude_pdo++) {

                if (pdo->index == *exclude_pdo) {
                    char_T msg[100];

                    snprintf(msg, sizeof(msg),
                            "PDO #x%04X and #x%04X mutually "
                            "exclude each other.", pdo->index, pdoX->index);

                    if (pdoX->use_count && pdo->use_count) {
                        pr_error(slave, ctxt, "", __LINE__,
                                "%s\n"
                                "Both are in use. Check the "
                                "slave's configuration.", msg);
                        return -1;

                    }
                    else {
                        /* One of them is unused, so deselect it
                         * automatically, making sure that the slave's
                         * pdo_count and pdo_entry_count are updated
                         * too */
                        if (pdo->use_count) {
                            pdoX->exclude_from_config = 1;
                            slave->pdo_count--;
                            slave->pdo_entry_count -=
                                pdoX->entry_end - pdoX->entry;
                        }
                        else {
                            pdo->exclude_from_config = 1;
                            slave->pdo_count--;
                            slave->pdo_entry_count -=
                                pdo->entry_end - pdo->entry;
                        }

                        pr_warn(slave, "PDO Configuration Check", "",
                                __LINE__,
                                "%s\n"
                                "Because PDO #x%04X is unused "
                                "it will be deselected automatically. "
                                "This may cause problems.\n",
                                msg,
                                pdoX->use_count ? pdo->index : pdoX->index);

                        /* Setting the unique flag so that this automatic
                         * deselection does not affect other slave
                         * configurations */
                        slave->unique_config = 1;
                    }
                }
            }
        }
    }

    /* While reading in all the PDO's, the pdo_ptr_end of the corresponding
     * SyncManager was incremented as well. Now that it is known how
     * many PDO's a SyncManager has, memory for a set of PDO pointers
     * can be allocated. */
    for (sm = slave->sync_manager; sm != slave->sync_manager_end; sm++) {
        uint_T pdo_count = sm->pdo_ptr_end - sm->pdo_ptr;

        CHECK_CALLOC(S, pdo_count, sizeof(struct pdo*), sm->pdo_ptr);
        sm->pdo_ptr_end = sm->pdo_ptr;
    }

    /* Now go through the pdo list again and setup the
     * SyncManager's pdo pointers */
    for (pdo = slave->pdo; pdo != slave->pdo_end; pdo++) {
        if (pdo->sm) {
            *pdo->sm->pdo_ptr_end++ = pdo;
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
    struct io_port *port;
    uint_T output_port_idx, input_port_idx;

    ssSetNumSFcnParams(S, PARAM_COUNT);  /* Number of expected parameters */
    if (ssGetNumSFcnParams(S) != ssGetSFcnParamsCount(S)) {
        /* Return if number of expected != number of actual parameters */
        return;
    }

    for( i = 0; i < PARAM_COUNT; i++)
        ssSetSFcnParamTunable(S, i, SS_PRM_NOT_TUNABLE);

    /* allocate memory for slave structure */
    if (!(slave = mxCalloc(1, sizeof(*slave)))) {
        return;
    }
    slave->debug_level = mxGetScalar(ssGetSFcnParam(S, DEBUG));
    slave->S = S;

    if (get_slave_address(slave)) return;

    if (get_ethercat_info(slave)) return;

    if (get_sdo_config(S, slave)) return;

    if (get_ioport_config(S, slave)) return;

    if (check_pdo_configuration(S, slave))
        return;

    /* Process input ports */
    if (!ssSetNumInputPorts(S, slave->input_port_count))
        return;
    if (!ssSetNumOutputPorts(S, slave->output_port_count))
        return;

    for ( port = slave->io_port, input_port_idx = output_port_idx = 0;
            port != slave->io_port_end; port++) {

        if (port->direction) {
            /* Input port */
            ssSetInputPortWidth(S, input_port_idx, DYNAMICALLY_SIZED);
            ssSetInputPortDataType(S, input_port_idx,
                    ( port->pdo_full_scale
                      ? DYNAMICALLY_TYPED : port->data_type));

            input_port_idx++;
        }
        else {
            /* Output port */
            ssSetOutputPortWidth(S,    output_port_idx, port->width);
            ssSetOutputPortDataType(S, output_port_idx, port->data_type);

            if (set_io_port_width(S, slave,
                        port - slave->io_port, port->width))
                return;

            output_port_idx++;
        }
    }

    ssSetNumSampleTimes(S, 1);

    if (mxGetScalar(ssGetSFcnParam(S, TSAMPLE))) {
        ssSetNumDiscStates(S, slave->filter_count);
    } else {
        ssSetNumContStates(S, slave->filter_count);
    }

    /* Make the memory peristent, otherwise it is lost just before
     * mdlRTW is called. To ensure that the memory is released again,
     * even in case of failures, the option SS_OPTION_CALL_TERMINATE_ON_EXIT
     * has to be set */
    slave_mem_op(slave, mexMakeMemoryPersistent);
    ssSetUserData(S, slave);

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
    ssSetSampleTime(S, 0, mxGetScalar(ssGetSFcnParam(S, TSAMPLE)));
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
    struct io_port *io_port;
    int_T port_idx = 0;

    if (!slave)
        return;

    ssSetInputPortWidth(S, port, width);

    for (io_port = slave->io_port; io_port != slave->io_port_end; io_port++) {
        if (io_port->direction) {
            if (port_idx++ == port)
                break;
        }
    }
    set_io_port_width(S, slave, port_idx-1, width);
}

/* This function is called when some ports are still DYNAMICALLY_SIZED
 * even after calling mdlSetInputPortWidth(). This occurs when input
 * ports are not connected. */
#define MDL_SET_DEFAULT_PORT_DIMENSION_INFO
static void mdlSetDefaultPortDimensionInfo(SimStruct *S)
{
    struct ecat_slave *slave = ssGetUserData(S);
    int_T input_port_idx, output_port_idx;
    struct io_port *port;

    if (!slave)
        return;

    for (port = slave->io_port, input_port_idx = output_port_idx = 0;
            port != slave->io_port_end; port++) {
        if (port->direction) {
            if (ssGetInputPortWidth(S, input_port_idx)
                    == DYNAMICALLY_SIZED) {
                ssSetInputPortWidth(S, input_port_idx, port->width);
                set_io_port_width(S, slave, input_port_idx, port->width);
            }
            input_port_idx++;
        }
        else {
            if (ssGetOutputPortWidth(S, output_port_idx)
                    == DYNAMICALLY_SIZED) {
                ssSetOutputPortWidth(S, output_port_idx, port->width);
            }
            output_port_idx++;
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

    for (port = slave->io_port; port != slave->io_port_end; port++) {

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
    uint_T pwork_index = 0;
    uint_T iwork_index = 0;
    uint_T rwork_index = 0;
    uint_T filter_index = 0;
    real_T rwork_vector[slave->rwork_count];
    uint_T sm_count = slave->sync_manager_end - slave->sync_manager;
    uint_T io_port_count = slave->io_port_end - slave->io_port;

    /* General assignments of array indices that form the basis for
     * the S-Function <-> TLC communication
     * DO NOT CHANGE THESE without updating the TLC ec_test2.tlc
     * as well */
    enum {
        PortSpecDir = 0,		/* 0 */
        PortSpecPortIdx,     		/* 1 */
        PortSpecPWork,  		/* 2 */
        PortSpecDType,                  /* 3 */
        PortSpecBitLen, 		/* 4 */
        PortSpecIWork,                  /* 5 */
        PortSpecGainCount,              /* 6 */
        PortSpecGainRWorkIdx,           /* 7 */
        PortSpecOffsetCount,            /* 8 */
        PortSpecOffsetRWorkIdx,         /* 9 */
        PortSpecFilterCount,            /* 10 */
        PortSpecFilterIdx,              /* 11 */
        PortSpecFilterRWorkIdx,         /* 12 */
        PortSpecMax                     /* 13 */
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
    if (!ssWriteRTWScalarParam(S, "MasterId",
                &slave->master, SS_UINT32))                     return;
    if (!ssWriteRTWScalarParam(S, "DomainId",
                &slave->domain, SS_UINT32))                     return;
    if (!ssWriteRTWScalarParam(S, "SlaveAlias",
                &slave->alias, SS_UINT32))                      return;
    if (!ssWriteRTWScalarParam(S, "SlavePosition",
                &slave->position, SS_UINT32))                   return;
    if (!ssWriteRTWStrParam(S, "ProductName",
                slave->type ? slave->type : ""))                return;
    if (!ssWriteRTWScalarParam(S, "VendorId",
                &slave->vendor_id, SS_UINT32))                  return;
    if (!ssWriteRTWScalarParam(S, "ProductCode",
                &slave->product_code, SS_UINT32))               return;
    if (!ssWriteRTWScalarParam(S, "RevisionNo",
                &slave->revision_no, SS_UINT32))                return;
    if (!ssWriteRTWScalarParam(S, "LayoutVersion",
                &slave->pdo_configuration_variant, SS_UINT32))  return;
    if (!ssWriteRTWScalarParam(S, "UniqueConfig",
                &slave->pdo_configuration_variant, SS_BOOLEAN)) return;
    if (!ssWriteRTWScalarParam(S, "NumOutputs",
                &slave->output_port_count, SS_BOOLEAN))         return;
    if (!ssWriteRTWScalarParam(S, "NumInputs",
                &slave->input_port_count, SS_BOOLEAN))          return;

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

    if (sm_count) {
        uint32_T   sync_manager[SM_Max][sm_count];
        uint32_T       pdo_info[PdoInfo_Max][slave->pdo_count];
        uint32_T pdo_entry_info[PdoEI_Max][slave->pdo_entry_count];

        uint_T sm_idx = 0, pdo_idx = 0, pdo_entry_idx = 0;

        struct sync_manager *sm;

        for (sm = slave->sync_manager; sm != slave->sync_manager_end;
                sm++, sm_idx++) {
            struct pdo **pdo_ptr;

            sync_manager[SM_Index][sm_idx] = sm->index;
            sync_manager[SM_Direction][sm_idx] =
                sm->direction == EC_SM_INPUT;
            sync_manager[SM_PdoCount][sm_idx] = 0;

            for (pdo_ptr = sm->pdo_ptr; pdo_ptr != sm->pdo_ptr_end;
                    pdo_ptr++) {
                struct pdo_entry *pdo_entry;

                if ((*pdo_ptr)->exclude_from_config)
                    continue;

                sync_manager[SM_PdoCount][sm_idx]++;

                pdo_info[PdoInfo_PdoIndex][pdo_idx] =
                    (*pdo_ptr)->index;
                pdo_info[PdoInfo_PdoEntryCount][pdo_idx] =
                    (*pdo_ptr)->entry_end - (*pdo_ptr)->entry;

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
                pdo_idx++;
            }
        }

        if (slave->pdo_entry_count) {
            if (!ssWriteRTW2dMatParam(S, "PdoEntryInfo", pdo_entry_info,
                        SS_UINT32, slave->pdo_entry_count, PdoEI_Max))
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
                    SS_UINT32, sm_count, SM_Max))
            return;
    }

    if (io_port_count) {
        struct io_port *port;
        int32_T port_spec[PortSpecMax][io_port_count];
        real_T pdo_full_scale[io_port_count];
        uint_T port_idx;
        const char_T *gain_name_ptr[io_port_count];
        const char_T *offset_name_ptr[io_port_count];
        const char_T *filter_name_ptr[io_port_count];
        const char_T *gain_str;
        const char_T *offset_str;
        const char_T *filter_str;
        uint_T output_port_idx = 0, input_port_idx = 0;

        memset(  gain_name_ptr, 0, sizeof(  gain_name_ptr));
        memset(offset_name_ptr, 0, sizeof(offset_name_ptr));
        memset(filter_name_ptr, 0, sizeof(filter_name_ptr));

        memset(port_spec, 0, sizeof(port_spec));

        for (port_idx = 0, port = slave->io_port;
                port_idx < io_port_count; port_idx++, port++) {
            struct pdo_entry **pdo_entry_ptr;

            port_spec[PortSpecDir][port_idx] = port->direction;
            port_spec[PortSpecPortIdx][port_idx] = 
                port->direction ? input_port_idx++ : output_port_idx++;
            port_spec[PortSpecPWork][port_idx] = pwork_index;
            port_spec[PortSpecDType][port_idx] =
                port->pdo_data_type == SS_BOOLEAN
                ? SS_UINT8 : port->pdo_data_type;
            port_spec[PortSpecBitLen][port_idx] =
                port->bitop ? port->data_bit_len : 0;
            port_spec[PortSpecIWork][port_idx] = iwork_index;

            if (port->gain_count) {
                port_spec[PortSpecGainCount][port_idx] =
                    port->gain_count;
                if (port->gain_name) {
                    port_spec[PortSpecGainRWorkIdx][port_idx] = -1;
                    gain_name_ptr[port_idx] = port->gain_name;
                }
                else {
                    memcpy(rwork_vector + rwork_index, port->gain_values,
                            sizeof(*rwork_vector) * port->gain_count);
                    port_spec[PortSpecGainRWorkIdx][port_idx] =
                        rwork_index;
                    rwork_index += port->gain_count;
                    gain_name_ptr[port_idx] = port->gain_count == 1
                        ? NULL : "<rwork>/NonTuneableParam";
                }
            }

            if (port->offset_count) {
                port_spec[PortSpecOffsetCount][port_idx] =
                    port->offset_count;
                if (port->offset_name) {
                    port_spec[PortSpecOffsetRWorkIdx][port_idx] = -1;
                    offset_name_ptr[port_idx] = port->offset_name;
                }
                else {
                    memcpy(rwork_vector + rwork_index, port->offset_values,
                            sizeof(*rwork_vector) * port->offset_count);
                    port_spec[PortSpecOffsetRWorkIdx][port_idx] =
                        rwork_index;
                    rwork_index += port->offset_count;
                    offset_name_ptr[port_idx] = port->offset_count == 1
                        ? NULL : "<rwork>/NonTuneableParam";
                }
            }

            if (port->filter_count) {
                port_spec[PortSpecFilterCount][port_idx] =
                    port->filter_count;
                port_spec[PortSpecFilterIdx][port_idx] = filter_index;
                filter_index += port->filter_count;
                if (port->filter_name) {
                    port_spec[PortSpecFilterRWorkIdx][port_idx] = -1;
                    filter_name_ptr[port_idx] = port->filter_name;
                }
                else {
                    memcpy(rwork_vector + rwork_index, port->filter_values,
                            sizeof(*rwork_vector) * port->filter_count);
                    port_spec[PortSpecFilterRWorkIdx][port_idx] =
                        rwork_index;
                    rwork_index += port->filter_count;
                    filter_name_ptr[port_idx] = port->filter_count == 1
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

        gain_str   = createStrVect(  gain_name_ptr, io_port_count);
        offset_str = createStrVect(offset_name_ptr, io_port_count);
        filter_str = createStrVect(filter_name_ptr, io_port_count);


        if (!ssWriteRTW2dMatParam(S, "IOPortSpec", port_spec,
                    SS_INT32, io_port_count, PortSpecMax))
            return;
        if (!ssWriteRTWVectParam(S, "PDOFullScale",
                    pdo_full_scale, SS_DOUBLE, io_port_count))
            return;
        if (!ssWriteRTWStrVectParam(S, "GainName", gain_str,
                    io_port_count))
            return;
        if (!ssWriteRTWStrVectParam(S, "OffsetName", offset_str,
                    io_port_count))
            return;
        if (!ssWriteRTWStrVectParam(S, "FilterName", filter_str,
                    io_port_count))
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
