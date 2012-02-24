/*
 * $Id$
 *
 * This SFunction implements a generic EtherCAT Slave, Version 3
 *
 * Copyright (c) 2010, Richard Hacker
 * License: GPL
 *
 * Description:
 * This generic SFunction can be used to realise any EtherCAT slave that
 * is used in EtherLab. All the necessary information needed
 * to configure the slave, all the outputs and inputs, scaling,
 * filtering, etc. is specified via the parameters passed to the SFunction.
 *
 * There are 7 parameters. These are described below:
 *
 * 1: EtherCAT Address:
 *
 * 2: EtherCAT Slave Description
 *        SS_INT32 and SS_UINT32
 *
 * 3: SDO Configuration - N-by-4 numeric matrix
 *   A matrix with 4 columns that is used to configure a slave. The
 *   configuration is transferred to the slave during initialisation.
 *   The columns have the following functions:
 *    1 - Index
 *    2 - Subindex
 *    3 - Data type: one of 8, 16, 32
 *    4 - Value
 *
 * 4: SoE Configuration String
 *
 * 5. Block IO port specification - structure vector
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
 * 6. Debugging
 *
 * 6. Sample Time: numeric scalar value.
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

#define S_FUNCTION_NAME  ec_slave3
#define S_FUNCTION_LEVEL 2

#include "simstruc.h"


#include <math.h>
#include <ctype.h>

#ifdef _WIN32
#define __attribute__(x)
#endif



enum {
    ADDRESS = 0,
    ETHERCAT_INFO,
    SDO_CONFIG,
    SOE_CONFIG,
    PORT_SPEC,
    DEBUG,
    TSAMPLE,
    PARAM_COUNT
};

static struct datatype_info {
    uint_T id;
    char *name;
    enum {Unsigned, Signed, Real} type;
    boolean_T is_standard;
    int_T mant_bits;
    DTypeId sl_type;
} datatype_info[] = {
    { 0x0101, "Boolean",    Unsigned, 1, 1,  SS_BOOLEAN        }, /*  0 */
    { 0x0102, "Bit2",       Unsigned, 0, 2,  SS_UINT8          }, /*  1 */
    { 0x0103, "Bit3",       Unsigned, 0, 3,  SS_UINT8          }, /*  2 */
    { 0x0104, "Bit4",       Unsigned, 0, 4,  SS_UINT8          }, /*  3 */
    { 0x0105, "Bit5",       Unsigned, 0, 5,  SS_UINT8          }, /*  4 */
    { 0x0106, "Bit6",       Unsigned, 0, 6,  SS_UINT8          }, /*  5 */
    { 0x0107, "Bit7",       Unsigned, 0, 7,  SS_UINT8          }, /*  6 */
    { 0x0108, "Unsigned8",  Unsigned, 1, 8,  SS_UINT8          }, /*  7 */
    { 0x0110, "Unsigned16", Unsigned, 1, 16, SS_UINT16         }, /*  8 */
    { 0x0118, "Unsigned24", Unsigned, 0, 24, SS_UINT32         }, /*  9 */
    { 0x0120, "Unsigned32", Unsigned, 1, 32, SS_UINT32         }, /* 10 */
    { 0x0128, "Unsigned40", Unsigned, 0, 40, DYNAMICALLY_TYPED }, /* 11 */
    { 0x0130, "Unsigned48", Unsigned, 0, 48, DYNAMICALLY_TYPED }, /* 12 */
    { 0x0138, "Unsigned56", Unsigned, 0, 56, DYNAMICALLY_TYPED }, /* 13 */
    { 0x0140, "Unsigned64", Unsigned, 1, 64, DYNAMICALLY_TYPED }, /* 14 */

    { 0x0208, "Integer8",   Signed,   1, 8,  SS_INT8           }, /* 15 */
    { 0x0210, "Integer16",  Signed,   1, 16, SS_INT16          }, /* 16 */
    { 0x0220, "Integer32",  Signed,   1, 32, SS_INT32          }, /* 17 */
    { 0x0240, "Integer64",  Signed,   1, 64, DYNAMICALLY_TYPED }, /* 18 */

    { 0x0320, "Real32",     Real,     1, 32, SS_SINGLE         }, /* 19 */
    { 0x0340, "Real64",     Real,     1, 64, SS_DOUBLE         }, /* 20 */
    {      0,                                                  },
};

static struct datatype_info *type_uint8_info  = &datatype_info[7];
static struct datatype_info *type_uint16_info = &datatype_info[8];
static struct datatype_info *type_uint32_info = &datatype_info[9];
static struct datatype_info *type_single_info = &datatype_info[19];
static struct datatype_info *type_double_info = &datatype_info[20];

static char errmsg[256];

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

    struct sdo_config {
        uint16_T index;
        uint8_T subindex;

        /* Data type. One of SS_UINT8, SS_UINT16, SS_UINT32 */
        struct datatype_info *datatype;

        /* Configuration value */
        uint_T value;
    } *sdo_config, *sdo_config_end;

    /* Sercos over EtherCat configuration for a slave */
    struct soe_config {
        uint16_T index;
        char_T *octet_string;
        size_t octet_string_len;
    } *soe_config, *soe_config_end;

    /* This value is needed right at the end when the values have
     * to be written by mdlRTW */
    uint_T pdo_entry_count;
    uint_T pdo_count;

    struct sync_manager {

        uint16_T index;
        boolean_T direction;    /* 0 = Output, for RxPdo's
                                 * 1 = Input, for TxPdo's */
        uint_T watchdog;

        /* Description of the slave's Pdo's */
        struct pdo {
            uint16_T index;

            uint_T os_index_inc; /* Oversampling index increment
                                  * 0 -> not specified */

            struct pdo_entry {
                uint16_T index;
                uint8_T subindex;
                uint_T bitlen;

                struct datatype_info *data_type;

                uint_T vector_length;   /* The width of how bitlen is
                                         * interpreted by the port */
                int_T pwork_index;
                int_T iwork_index;

            } *entry, *entry_end;

        } *pdo, *pdo_end;

    } *sync_manager, *sync_manager_end;

    /* Distributed clocks */
    struct dc_opmode {
        uint16_T assign_activate;

        /* CycleTimeSync, ShiftTimeSync */
        struct time_sync {
            /* Only specified for ShiftTimeSync0 */
            boolean_T input;

            int32_T factor;
            uint32_T value;
        } cycle_time_sync[2], shift_time_sync[2];

        /* OpMode */
        struct opmode_sm {
            uint_T no;
            struct opmode_pdo {
                uint_T osfac;
                uint_T value;
            } *pdo, *pdo_end;
        } *sm, *sm_end;
    } *dc_opmode, *dc_opmode_end, *use_dc_opmode;

    /* Maximum port width of both input and output ports. This is a
     * temporary value that is used to check whether the Simulink block
     * has mixed port widths */
    uint_T max_port_width;

    /* Set if the Simulink block has mixed port widths */
    boolean_T have_mixed_port_widths;

    struct io_port {
        struct pdo_entry **pdo_entry, **pdo_entry_end;

        uint_T index;

        DTypeId sl_data_type;

        /* Data type of the IO port
         * If 'PdoFullScale' is specified, it is SS_DOUBLE, otherwise
         * a data type is chosen that suits data_bit_len */
        struct datatype_info *pdo_data_type;

        /* Number of signals for the port
         * This is also the number of *map entries,
         * i.e. one PDO for every signal.
         */
        uint_T width;

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

    /* Here is the list of input and output ports in the sequence of
     * definition. This allows direct indexing into the io_port structure
     * knowing the port number */
    struct io_port **input_port;
    struct io_port **output_port;

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

    /* Constants  used to store parameters where no name was
     * supplied for gain_name, offset_name and filter_name. These
     * parameters are not exported in the C-API */
    uint_T constant_count;

    uint_T filter_count;
};

static char_T msg[512];

#undef printf

#ifdef _WIN32
int snprintf(char *str, size_t size, const char *format, ...)
{
     int rvalue;
     va_list ap;
     
     va_start(ap, format);
     sprintf(str, format, ap);
     va_end(ap);
     return rvalue;
}

int vsnprintf(char *str, size_t size, const char *format, va_list ap)
{
     int rvalue;
  
     vsprintf(str, format, ap);
     return rvalue;
}

#endif


static void
intro(const struct ecat_slave *slave, unsigned int indent,
        const char_T* parent_context, const char_T *context)
{
    static const char_T *path;

    if (path != ssGetPath(slave->S)) {
        path = ssGetPath(slave->S);
        mexPrintf("====== EtherCAT slave %s =======\n", path);
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

static void __attribute__((format (printf, 5, 6)))
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

static void __attribute__((format (printf, 5, 6)))
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

static void __attribute__((format (printf, 5, 6)))
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
            parent_context,
            context ? context : "", line_no);
    len = vsnprintf(msg + len, sizeof(msg) - len, fmt, ap);
    ssSetErrorStatus(slave->S, msg);
    va_end(ap);
}

static void __attribute__((format (printf, 5, 6)))
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
            ssGetPath(slave->S), parent_context, context, line_no);
    vprintf(fmt, ap);
    va_end(ap);
}

#define printf mexPrintf

static int_T
get_numeric_field(const struct ecat_slave *slave,
        const char_T *p_ctxt, unsigned int line_no,
        const mxArray *src, uint_T idx, boolean_T zero_allowed,
        boolean_T missing_allowed, boolean_T nan_allowed,
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

    if (!(mxIsNumeric(field) || mxIsLogical(field))) {
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
    else if (mxIsNaN(*dest)) {
        if (nan_allowed)
            return 1;
        pr_error(slave, p_ctxt, field_name, line_no,
                "Value is not allowed to be NaN");
        return -4;
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

static int_T
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

    len = mxGetNumberOfElements(field);

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
            pr_error(slave, p_ctxt, field_name, line_no,
                    "Default string is empty");
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

static int_T
get_char_field(const struct ecat_slave *slave,
        const char_T *p_ctxt, unsigned int line_no,
        const mxArray *src, uint_T idx,
        const char_T *field_name,
        char_T **dest,
        size_t *len)
{
    const mxArray *field = mxGetField(src, idx, field_name);
    uint_T i;
    size_t n;
    mxChar *s;

    if (!field || !mxIsChar(field)
            || !(n = mxGetNumberOfElements(field))) {
        return -1;
    }

    CHECK_CALLOC(slave->S, n, 1, *dest);

    s = mxGetChars(field);
    if (!s) {
        pr_error(slave, p_ctxt, field_name, line_no, "Expected a char array");
        return -1;
    }
    for (i = 0; i < n; i++)
        (*dest)[i] = s[i];
    *len = n;

    return 0;
}

#define RETURN_ON_ERROR(val)    \
    do {                        \
        int_T err = (val);        \
        if (err < 0)            \
            return err;         \
    } while(0)

static int_T
get_data_type_from_class(const struct ecat_slave *slave,
        const char_T *p_ctxt, unsigned int line_no,
        const mxArray *src, uint_T idx, const char_T *name,
        struct datatype_info **data_type, boolean_T missing_allowed)
{
    char *class;
    const mxArray *field = mxGetField(src, idx, name);
    char_T context[100];

    if (!field || !mxGetNumberOfElements(field)) {
        if (missing_allowed)
            return 1;

        pr_error(slave, p_ctxt, name, line_no,
                "Required source variable does not exist.");
        return -1;
    }

    snprintf(context, sizeof(context), "%s.%s", p_ctxt, name);
    RETURN_ON_ERROR(get_string_field(slave, context, __LINE__, field, 0,
                "Class", NULL, &class));

    *data_type = NULL;
    if      (!strcmp(class,  "INT"   )) {
        double val = 0.0;
        uint_T mant_bits;
        uint_T type;

        RETURN_ON_ERROR(get_numeric_field(slave, p_ctxt, __LINE__,
                    src, 0, 1, 0, 0, "IsSigned", &val));
        type = val ? Signed : Unsigned;

        RETURN_ON_ERROR(get_numeric_field(slave, p_ctxt, __LINE__,
                    src, 0, 0, 0, 0, "MantBits", &val));
        mant_bits = (uint_T)val;
        for (*data_type = datatype_info; (*data_type)->id; (*data_type)++) {
            if ((*data_type)->type == type
                    && (*data_type)->mant_bits == mant_bits)
                break;
        }
    }
    else if (!strcmp(class,  "DOUBLE")) {
        *data_type = type_double_info;
    }
    else if (!strcmp(class,  "SINGLE")) {
        *data_type = type_single_info;
    }

    mxFree(class);

    if (!*data_type || !(*data_type)->id) {
        *data_type = NULL;
        pr_error(slave, p_ctxt, name, __LINE__,
                "Invalid data type specified. "
                "Only uint([1..8,16,32]) sint([8,16,32]) "
                "float('single') or float('double') is allowed");
        return -1;
    }
    return 0;
}

static int_T
get_slave_address(struct ecat_slave *slave)
{
    const char_T *ctxt = "ADDRESS";
    const mxArray *address       = ssGetSFcnParam(slave->S, ADDRESS);
    real_T val;

    pr_debug(slave, NULL, NULL, 0,
            "--------------- Slave Address -----------------\n");
    RETURN_ON_ERROR(get_numeric_field(slave, ctxt, __LINE__, address,
                0, 1, 0, 0, "Master", &val));
    slave->master = val;
    RETURN_ON_ERROR(get_numeric_field(slave, ctxt, __LINE__, address,
                0, 1, 0, 0, "Domain", &val));
    slave->domain = val;
    RETURN_ON_ERROR(get_numeric_field(slave, ctxt, __LINE__, address,
                0, 1, 0, 0, "Alias", &val));
    slave->alias = val;
    RETURN_ON_ERROR(get_numeric_field(slave, ctxt, __LINE__, address,
                0, 1, 0, 0, "Position", &val));
    slave->position = val;

    pr_debug(slave, NULL, "", 1,
            "Master %u, Domain %u, Alias %u, Position %u\n",
            slave->master, slave->domain, slave->alias, slave->position);

    return 0;
}

static int_T
get_sync_manager(struct ecat_slave *slave, const char_T *p_ctxt,
        unsigned int indent_level, const mxArray *sm_info)
{
    struct sync_manager *sm;
    uint_T sm_idx;

    pr_debug(slave, NULL, NULL, 0,
            "--------------- SyncManager Configuration ------------\n");

    if (!sm_info || !(sm_idx = mxGetNumberOfElements(sm_info))) {
        pr_debug(slave, p_ctxt, "Sm", indent_level,
                "SyncManager is empty or not specified.\n");
        return 0;
    }
    pr_debug(slave, p_ctxt, "Sm", indent_level,
            "SyncManager count = %u\n", sm_idx);

    CHECK_CALLOC(slave->S, sm_idx, sizeof(struct sync_manager),
            slave->sync_manager);
    slave->sync_manager_end = slave->sync_manager + sm_idx;

    for (sm = slave->sync_manager, sm_idx = 0;
            sm != slave->sync_manager_end; sm++, sm_idx++) {

        pr_debug(slave, NULL, "", indent_level+1,
                "Sm Index %u ", sm_idx);

        sm->index = sm_idx;

    }

//    pr_info(slave, NULL, NULL, 0,
//            "Found %li SyncManagers:",
//            slave->sync_manager_end - slave->sync_manager);
//    for (sm = slave->sync_manager, sm_idx = 0;
//            sm != slave->sync_manager_end; sm++, sm_idx++) {
//        pr_info(slave, NULL, NULL, 0,
//                " %u (%s),", sm_idx,
//                (sm->virtual
//                 ? "virtual"
//                 : (sm->direction
//                     ? (sm->direction == EC_SM_OUTPUT ? "output" : "input")
//                     : "ignored")
//                ));
//    }
//    pr_info(slave, NULL, NULL, 0, "\n");

    return 0;
}

static struct sync_manager *
check_pdo_syncmanager(struct ecat_slave *slave,
        const char_T *p_ctxt, const char_T *ctxt,
        const struct pdo *pdo, unsigned int sm_idx,
        boolean_T set_sm_direction) 
{
//    struct sync_manager *sm = slave->sync_manager + sm_idx;
//    unsigned int expected_sm_dir =
//        pdo->direction ? EC_SM_OUTPUT : EC_SM_INPUT;
//
//    if (sm >= slave->sync_manager_end) {
//        pr_error(slave, p_ctxt, ctxt, __LINE__,
//                "SyncManager index %u is not available",
//                sm_idx);
//        return NULL;
//    }
//
//    if (sm->virtual != pdo->virtual) {
//        /* Error Virtual is different */
//        pr_error(slave, p_ctxt, ctxt, __LINE__,
//                "Conflict in virtual status: "
//                "SyncManager %u %s virtual whereas Pdo #x%04X %s "
//                "virtual",
//                sm->index, sm->virtual ? "is" : "is not",
//                pdo->index, pdo->virtual ? "is" : "is not");
//        return NULL;
//    }
//
//    if (!sm->direction) {
//        if (!sm->virtual) {
//            pr_error(slave, p_ctxt, ctxt, __LINE__,
//                    "SyncManager %u is not virtual, but it is not "
//                    "specified wheter it is an input or output.",
//                    sm->index);
//            return NULL;
//        }
//        else if (set_sm_direction) {
//            pr_debug(slave, p_ctxt, ctxt, 0,
//                    "Sm Idx %u automatically assigned %s direction\n",
//                    sm->index,
//                    expected_sm_dir == EC_SM_OUTPUT ? "output" : "input");
//            sm->direction = expected_sm_dir;
//            if (expected_sm_dir == EC_SM_OUTPUT) {
//                slave->output_sm_count++;
//            }
//            else {
//                slave->input_sm_count++;
//            }
//        }
//    }
//    else if (sm->direction && sm->direction != expected_sm_dir) {
//        /* Incorrect sm direction specified */
//        pr_error(slave, p_ctxt, ctxt, __LINE__,
//                "SyncManager %u is an %s SyncManager. "
//                "Assigning a %sPdo #x%04X to this SyncManager "
//                "is not possible",
//                sm->index,
//                (sm->direction == EC_SM_INPUT
//                 ? "input" : "output"),
//                pdo->direction ? "Rx" : "Tx",
//                pdo->index);
//        return NULL;
//    }
//
//    return sm;
}


static int_T
get_slave_pdo(struct ecat_slave *slave,
        const char_T *p_ctxt, unsigned int indent_level,
        const mxArray *pdo_info, unsigned int pdo_count,
        unsigned int pdo_dir, const char_T *dir_str)
{
//    unsigned int level;
//    struct pdo *pdo;
//    char_T context[100];
//    real_T val;
//    uint_T pdo_idx;
//    uint_T i;
//
//    pr_debug(slave, NULL, NULL, 0, "--------------- %s.%s ------------\n",
//            p_ctxt, dir_str);
//
//    if (!pdo_count) {
//        pr_debug(slave, p_ctxt, dir_str, indent_level,
//                "Pdo has no elements\n");
//        return 0;
//    }
//
//    if (!mxIsStruct(pdo_info)) {
//        pr_error(slave, p_ctxt, dir_str, __LINE__,
//                "mxArray is not a structure\n");
//        return -1;
//    }
//
//    for (pdo_idx = 0, pdo = slave->pdo_end;
//            pdo_idx < pdo_count; pdo_idx++, pdo++) {
//        const mxArray *exclude_info;
//        uint_T exclude_count;
//        const mxArray *pdo_entry_info;
//        struct pdo_entry *pdo_entry;
//        uint_T entry_idx;
//        uint_T entry_info_count;
//
//        level = indent_level+1;
//
//        snprintf(context, sizeof(context), "%s(%u)", dir_str, pdo_idx+1);
//
//        pdo->direction = pdo_dir;
//
//        RETURN_ON_ERROR(get_numeric_field(slave, p_ctxt, __LINE__,
//                    pdo_info, pdo_idx, 1, 0, 0, "Index", &val));
//        pdo->index = val;
//
//        pr_debug(slave, p_ctxt, context, indent_level,
//                "Found %s Index #x%04X\n", dir_str, pdo->index);
//
//        val = 0.0;
//        RETURN_ON_ERROR(get_numeric_field(slave, p_ctxt, __LINE__,
//                    pdo_info, pdo_idx, 1, 0, 0, "Virtual", &val));
//        pdo->virtual = val;
//
//        val = 0.0;
//        RETURN_ON_ERROR(get_numeric_field(slave, p_ctxt, __LINE__,
//                    pdo_info, pdo_idx, 1, 0, 0, "Mandatory", &val));
//        pdo->mandatory = val;
//
//        val = 0.0;
//        RETURN_ON_ERROR(get_numeric_field(slave, p_ctxt, __LINE__,
//                    pdo_info, pdo_idx, 1, 0, 1, "OSIndexInc", &val));
//        pdo->os_index_inc = mxIsNaN(val) ? -1 : val;
//
//        /* Get the SyncManager that this pdo is mapped to.
//         * Not specifying the SyncManager is not a good idea,
//         * Although it is possible to assume the right SyncManager if
//         * there is only one to choose from. */
//        RETURN_ON_ERROR(get_numeric_field(slave, p_ctxt, __LINE__,
//                    pdo_info, pdo_idx, 1, 0, 1, "Sm", &val));
//        if (mxIsNaN(val)) {
//            RETURN_ON_ERROR(get_numeric_field(slave, p_ctxt, __LINE__,
//                        pdo_info, pdo_idx, 1, 0, 1, "Su", &val));
//            pdo->su_idx = mxIsNaN(val) ? ~0U : val;
//        }
//        else if (!(pdo->sm = check_pdo_syncmanager(slave, p_ctxt, "Sm",
//                        pdo, val, 0))) {
//            /* Error reported by check_pdo_syncmanager */
//            return -1;
//        }
//
//        pr_debug(slave, NULL, "", indent_level+1,
//                "Attributes: Sm='%i' Virtual='%u', Mandatory='%u'\n",
//                pdo->sm ? pdo->sm->index : -1, 
//                pdo->virtual, pdo->mandatory);
//
//        exclude_info = mxGetField(pdo_info, pdo_idx, "Exclude");
//        if (exclude_info
//                && (exclude_count = mxGetNumberOfElements(exclude_info))) {
//            real_T *val = mxGetPr(exclude_info);
//
//            snprintf(context, sizeof(context),
//                    "%s(%u).Exclude", dir_str, pdo_idx+1);
//
//            if (!val || !mxIsNumeric(exclude_info)) {
//                pr_error(slave, p_ctxt, context, __LINE__,
//                        "Field Exclude is not a valid numeric array.");
//                return -1;
//            }
//
//            CHECK_CALLOC(slave->S, exclude_count, sizeof(uint_T),
//                    pdo->exclude_index);
//            pdo->exclude_index_end = pdo->exclude_index + exclude_count;
//
//            pr_debug(slave, NULL, "", indent_level+1, "Exclude PDOS:");
//            for (i = 0; i < exclude_count; i++) {
//                pdo->exclude_index[i] = val[i];
//                pr_debug(slave, NULL, NULL, 0,
//                        " #x%04X", pdo->exclude_index[i]);
//            }
//            pr_debug(slave, NULL, NULL, 0, "\n");
//        }
//
//        pdo_entry_info = mxGetField(pdo_info, pdo_idx, "Entry");
//        if (!mxIsStruct(pdo_entry_info)) {
//            pr_error(slave, p_ctxt, context, __LINE__,
//                    "This field is not a Matlab structure");
//            return -1;
//        }
//
//        entry_info_count =
//            pdo_entry_info ? mxGetNumberOfElements(pdo_entry_info) : 0;
//
//        CHECK_CALLOC(slave->S, entry_info_count, sizeof(struct pdo_entry),
//                pdo_entry);
//        pdo->entry     = pdo_entry;
//        pdo->entry_end = pdo_entry + entry_info_count;
//
//        for (entry_idx = 0; entry_idx < entry_info_count;
//                entry_idx++, pdo_entry++) {
//            uint_T data_type_id;
//
//            pdo_entry->pdo = pdo;
//
//            snprintf(context, sizeof(context),
//                    "%s(%u).Entry(%u)", dir_str, pdo_idx+1, entry_idx+1);
//
//            RETURN_ON_ERROR(get_numeric_field(slave, context, __LINE__,
//                        pdo_entry_info, entry_idx, 1, 0, 0, "Index", &val));
//            pdo_entry->index = val;
//            RETURN_ON_ERROR(get_numeric_field(slave, context, __LINE__,
//                        pdo_entry_info, entry_idx, 1, 0, 0, "BitLen", &val));
//            pdo_entry->bitlen = val;
//
//            if (!pdo_entry->bitlen) {
//                pr_error(slave,context,"BitLen",__LINE__,
//                        "Zero BitLen is not allowed");
//                return -1;
//            }
//
//            if (!pdo_entry->index) {
//                pr_debug(slave, NULL, "", indent_level+1,
//                        "Found PDO Entry Index #x0000,                "
//                        "BitLen %u   (spacer)\n",
//                        pdo_entry->bitlen);
//                continue;
//            }
//
//            RETURN_ON_ERROR(get_numeric_field(slave, context, __LINE__,
//                        pdo_entry_info, entry_idx, 1, 0, 0, "SubIndex",
//                        &val));
//            pdo_entry->subindex = val;
//
//            RETURN_ON_ERROR(get_numeric_field(slave, context, __LINE__,
//                        pdo_entry_info, entry_idx, 1, 0, 0, "DataType",
//                        &val));
//            data_type_id = val;
//
//            for (pdo_entry->data_type = datatype_info;
//                    pdo_entry->data_type->id; pdo_entry->data_type++) {
//                if (data_type_id == pdo_entry->data_type->id) {
//                    break;
//                }
//            }
//            if (!pdo_entry->data_type->id) {
//                pr_error(slave, context, "DataType", __LINE__,
//                        "No data type corresponds to id 0x%04x.", data_type_id);
//                return -1;
//            }
//
//            if (pdo_entry->bitlen % pdo_entry->data_type->mant_bits) {
//                pr_error(slave,context,"DataType",__LINE__,
//                        "PdoEntry's BitLen (%u) is not a multiple of "
//                        "the data type's significant bits (%u)",
//                        pdo_entry->bitlen,
//                        pdo_entry->data_type->mant_bits);
//                return -1;
//            }
//
//            pr_debug(slave, NULL, "", indent_level+1, 
//                    "Found PDO Entry Index #x%04X, SubIndex %u, "
//                    "BitLen %u, DataType %s\n",
//                    pdo_entry->index, pdo_entry->subindex,
//                    pdo_entry->bitlen,
//                    pdo_entry->data_type->name);
//        }
//    }
//    slave->pdo_end = pdo;
//
    return 0;
}

static int_T
get_dc_time_sync(struct ecat_slave *slave, const char_T *p_ctxt, 
        const mxArray *opmode_spec,
        const char_T *field, boolean_T get_input, struct time_sync *ts)
{
    const mxArray *entry = mxGetField(opmode_spec, 0, field);
    char_T ctxt[100];
    real_T tmp = 0.0;

    if (!entry || !mxGetNumberOfElements(entry))
        return 0;

    snprintf(ctxt, sizeof(ctxt), "%s.%s", p_ctxt, field);

    RETURN_ON_ERROR(get_numeric_field(slave, ctxt, __LINE__,
                entry, 0, 1, 0, 0, "Value", &tmp));
    ts->value = tmp;

    tmp = 0.0;
    RETURN_ON_ERROR(get_numeric_field(slave, ctxt, __LINE__,
                entry, 0, 1, 0, 1, "Factor", &tmp));
    ts->factor = mxIsNaN(tmp) ? 0 : tmp;

    if (get_input) {
        tmp = 0.0;
        RETURN_ON_ERROR(get_numeric_field(slave, ctxt, __LINE__,
                    entry, 0, 1, 0, 0, "Input", &tmp));
        ts->input = tmp;
        pr_debug(slave, NULL, "", 3, "%s Factor=%i Input=%i Value=%i\n",
                field, ts->factor, ts->input, ts->value);
    }
    else {
        pr_debug(slave, NULL, "", 3, "%s Factor=%i Value=%i\n",
                field, ts->factor, ts->value);
    }

    return 0;
}

static int_T
get_dc_spec(struct ecat_slave *slave,
        const char_T *p_ctxt, unsigned int p_indent, const mxArray *dc_spec)
{
    const mxArray *opmode_spec = 
        dc_spec ? mxGetField(dc_spec, 0, "OpMode") : NULL;
    uint_T count;
    uint_T opmode_idx;

    char_T ctxt[100];

    if (!dc_spec || !opmode_spec || 
            !(count = mxGetNumberOfElements(opmode_spec))) {
        pr_info(slave, NULL, NULL, 0,
                "No distributed clock specification found\n");
        return 0;
    }

    snprintf(ctxt, sizeof(ctxt), "%s.Dc.OpMode", p_ctxt);

    pr_debug(slave, NULL, NULL, 0,
        "----------- Distributed Clocks Configuration ----------\n");

    pr_debug(slave, p_ctxt, "Dc.OpMode", 1,
            "OpMode count = %u\n", count);

    CHECK_CALLOC(slave->S, count,
            sizeof(struct dc_opmode), slave->dc_opmode);
    slave->dc_opmode_end = slave->dc_opmode;
    
    /* FIXME: remove this */
    slave->use_dc_opmode = slave->dc_opmode;

    for (opmode_idx = 0; opmode_idx < count; opmode_idx++) {
        real_T tmp = 0.0;
        char_T om_ctxt[100];
        uint_T sm_count, sm_idx;
        const mxArray *sm_elem;

        snprintf(om_ctxt, sizeof(om_ctxt), "%s[%u]", ctxt, opmode_idx);

        RETURN_ON_ERROR(get_numeric_field(slave, om_ctxt, __LINE__,
                    opmode_spec, opmode_idx, 1, 0, 0, "AssignActivate",
                    &tmp));
        slave->dc_opmode_end->assign_activate = tmp;

        pr_debug(slave, NULL, "", 2,
                "OpMode Index %i AssignActivate=0x%04x\n",
                opmode_idx, slave->dc_opmode_end->assign_activate);

        RETURN_ON_ERROR(get_dc_time_sync(slave, om_ctxt, opmode_spec,
                    "CycleTimeSync0", 0,
                    &slave->dc_opmode_end->cycle_time_sync[0]));
        RETURN_ON_ERROR(get_dc_time_sync(slave, om_ctxt, opmode_spec,
                    "ShiftTimeSync0", 1,
                    &slave->dc_opmode_end->shift_time_sync[0]));
        RETURN_ON_ERROR(get_dc_time_sync(slave, om_ctxt, opmode_spec,
                    "CycleTimeSync1", 0,
                    &slave->dc_opmode_end->cycle_time_sync[1]));
        RETURN_ON_ERROR(get_dc_time_sync(slave, om_ctxt, opmode_spec,
                    "ShiftTimeSync1", 0,
                    &slave->dc_opmode_end->shift_time_sync[1]));

        sm_elem = mxGetField(opmode_spec, opmode_idx, "Sm");
        if (!sm_elem || !(sm_count = mxGetNumberOfElements(sm_elem))) {
            slave->dc_opmode_end++;
            continue;
        }

        CHECK_CALLOC(slave->S, sm_count,
                sizeof(struct opmode_sm), slave->dc_opmode_end->sm);
        slave->dc_opmode_end->sm_end = slave->dc_opmode_end->sm;

        for (sm_idx = 0; sm_idx < sm_count; sm_idx++) {
            char_T sm_ctxt[100];
            uint_T pdo_idx, pdo_count;
            const mxArray *pdo_elem;

            snprintf(sm_ctxt, sizeof(sm_ctxt), "%s.Sm[%i]", om_ctxt, sm_idx);

            RETURN_ON_ERROR(get_numeric_field(slave, sm_ctxt, __LINE__,
                        sm_elem, sm_idx, 1, 0, 0, "No", &tmp));
            slave->dc_opmode_end->sm_end->no = tmp;

            pr_debug(slave, NULL, "", 3,
                    "Sm No=%i\n", slave->dc_opmode_end->sm_end->no);

            pdo_elem = mxGetField(sm_elem, sm_idx, "Pdo");
            if (!pdo_elem || !(pdo_count = mxGetNumberOfElements(pdo_elem))) {
                slave->dc_opmode_end->sm_end++;
                continue;
            }

            CHECK_CALLOC(slave->S, pdo_count, sizeof(struct opmode_pdo),
                    slave->dc_opmode_end->sm_end->pdo);
            slave->dc_opmode_end->sm_end->pdo_end =
                slave->dc_opmode_end->sm_end->pdo;

            for (pdo_idx = 0; pdo_idx < pdo_count; pdo_idx++) {
                char_T pdo_ctxt[100];

                snprintf(pdo_ctxt, sizeof(pdo_ctxt), "%s.Pdo[%i]",
                        sm_ctxt, pdo_idx);

                RETURN_ON_ERROR(get_numeric_field(slave, pdo_ctxt, __LINE__,
                            pdo_elem, pdo_idx, 0, 0, 0, "Value", &tmp));
                slave->dc_opmode_end->sm_end->pdo_end->value = tmp;

                tmp = 0.0;
                RETURN_ON_ERROR(get_numeric_field(slave, pdo_ctxt, __LINE__,
                            pdo_elem, pdo_idx, 0, 0, 1, "OSFac", &tmp));
                slave->dc_opmode_end->sm_end->pdo_end->osfac = 
                    mxIsNaN(tmp) ? 0 : tmp;

                pr_debug(slave, NULL, "", 4,
                        "Pdo OSFac=%i idx=#x%04x\n",
                        slave->dc_opmode_end->sm_end->pdo_end->osfac,
                        slave->dc_opmode_end->sm_end->pdo_end->value);
                slave->dc_opmode_end->sm_end->pdo_end++;
            }

            slave->dc_opmode_end->sm_end++;
        }

        slave->dc_opmode_end++;
    }

    return 0;
}

static int_T
get_ethercat_info(struct ecat_slave *slave)
{
    const mxArray *ec_info = ssGetSFcnParam(slave->S, ETHERCAT_INFO);
    const mxArray
        *vendor = NULL,
        *descriptions = NULL,
        *devices = NULL,
        *device = NULL,
        *type = NULL;
//    const mxArray *rx_pdo, *tx_pdo;
//    uint_T rx_pdo_count, tx_pdo_count;
    const char_T *context;
    real_T val;

    if (!mxIsStruct(ec_info))
        return 0;

    /***********************
     * Get Slave address
     ***********************/
    do {
        if (!(vendor       = mxGetField( ec_info,      0, "Vendor"))
                || !mxGetNumberOfElements(vendor)) {
            vendor = NULL;
            break;
        }
        if (!(descriptions = mxGetField( ec_info,      0, "Descriptions"))
                || !mxGetNumberOfElements(descriptions)) {
            descriptions = NULL;
            break;
        }
        if (!(devices      = mxGetField( descriptions, 0, "Devices"))
                || !mxGetNumberOfElements(devices)) {
            devices = NULL;
            break;
        }
        if (!(device       = mxGetField( devices,      0, "Device"))
                || !mxGetNumberOfElements(device)) {
            device = NULL;
            break;
        }
        if (!(type         = mxGetField( device,       0, "Type"))
                || !mxGetNumberOfElements(type)) {
            type = NULL;
            break;
        }
    } while(0);

    if (!vendor || !descriptions || !devices || !device || !type) {
        pr_warn(slave, "EtherCATInfo", "", __LINE__,
                "Invalid structure format\n");
        return 0;
    }

    pr_debug(slave, NULL, NULL, 0,
            "--------------- Parsing EtherCATInfo ------------\n");

    /* Check the number of devices */
    switch (mxGetNumberOfElements(device)) {
        case 0:
            pr_error(slave,"Device count", "", __LINE__,
                    "EtherCATInfo structure does not have any devices");
            return -1;
        case 1:
            break;
        default:
            pr_warn(slave,"Device count", "", __LINE__,
                    "EtherCATInfo structure has %lu devices. "
                    "Using first device\n",
                    mxGetNumberOfElements(ec_info));
    }

    context = "EtherCATInfo.Vendor";
    RETURN_ON_ERROR(get_numeric_field(slave, context, __LINE__, vendor,
                0, 0, 0, 0, "Id", &val));
    slave->vendor_id = val;

    context = "EtherCATInfo.Descriptions.Devices.Device.Type";
    RETURN_ON_ERROR(get_numeric_field(slave, context, __LINE__, type,
                0, 1, 0, 0, "ProductCode", &val));
    slave->product_code = val;
    RETURN_ON_ERROR(get_numeric_field(slave, context, __LINE__, type,
                0, 1, 0, 0, "RevisionNo", &val));
    slave->revision_no = val;
    RETURN_ON_ERROR(get_string_field(slave, context, __LINE__, type, 0,
                "TextContent", "Unspecified", &slave->type));

    pr_debug(slave, NULL, NULL, 0,
            "  VendorId %u\n", slave->vendor_id);
    pr_debug(slave, NULL, NULL, 0,
            "  ProductCode #x%08X, RevisionNo #x%08X, Type '%s'\n",
            slave->product_code, slave->revision_no, slave->type);

    /***********************
     * Get SyncManager setup
     ***********************/
    context = "EtherCATInfo.Descriptions.Devices.Device";
    RETURN_ON_ERROR(get_sync_manager(slave, context, 1, 
                mxGetField(device, 0, "Sm")));

    RETURN_ON_ERROR(get_dc_spec(slave, context, 1,
                mxGetField(device, 0, "Dc")));

//    /***********************
//     * Pdo Setup
//     ***********************/
//    tx_pdo = mxGetField(device, 0, "TxPdo");
//    rx_pdo = mxGetField(device, 0, "RxPdo");
//
//    tx_pdo_count = tx_pdo ? mxGetNumberOfElements(tx_pdo) : 0;
//    rx_pdo_count = rx_pdo ? mxGetNumberOfElements(rx_pdo) : 0;
//
//    CHECK_CALLOC(slave->S, tx_pdo_count + rx_pdo_count,
//            sizeof(struct pdo), slave->pdo);
//    slave->pdo_end = slave->pdo;
//
//    RETURN_ON_ERROR(get_slave_pdo(slave, context, 1,
//                rx_pdo, rx_pdo_count, 1, "RxPdo"));
//    RETURN_ON_ERROR(get_slave_pdo(slave, context, 1,
//                tx_pdo, tx_pdo_count, 0, "TxPdo"));
//
    return 0;
}

static int_T
get_sdo_config(struct ecat_slave *slave)
{
    const mxArray *sdo_config_field = ssGetSFcnParam(slave->S, SDO_CONFIG);
    struct sdo_config *sdo_config;
    const char_T *param = "SDO_CONFIG";
    char_T context[100];
    uint_T sdo_config_count;

    pr_debug(slave, NULL, NULL, 0,
            "--------------- Parsing SDO Configuration ------------\n");

    sdo_config_count = mxGetNumberOfElements(sdo_config_field);

    CHECK_CALLOC(slave->S, sdo_config_count, sizeof(*slave->sdo_config),
            slave->sdo_config);
    slave->sdo_config_end = slave->sdo_config + sdo_config_count;

    pr_info(slave, param, NULL, 0,
            "Found %u SDO configuration options\n", sdo_config_count);

    for (sdo_config = slave->sdo_config;
            sdo_config != slave->sdo_config_end; sdo_config++) {
        real_T val;
        uint_T idx = sdo_config - slave->sdo_config;

        snprintf(context, sizeof(context), "%s(%u)", param, idx+1);

        RETURN_ON_ERROR(get_numeric_field(slave, context, __LINE__,
                    sdo_config_field, idx, 0, 0, 0, "Index", &val));
        sdo_config->index = val;

        RETURN_ON_ERROR(get_numeric_field(slave, context, __LINE__,
                    sdo_config_field, idx, 1, 0, 0, "SubIndex", &val));
        sdo_config->subindex = val;

        RETURN_ON_ERROR(get_numeric_field(slave, context, __LINE__,
                    sdo_config_field, idx, 0, 0, 0, "BitLen", &val));
        switch ((int_T)val) {
            case 8:
                sdo_config->datatype = type_uint8_info;
                break;
            case 16:
                sdo_config->datatype = type_uint16_info;
                break;
            case 32:
                sdo_config->datatype = type_uint32_info;
                break;
            default:
                pr_error(slave, context, "BitLen", __LINE__,
                        "Unknown length %i specified; "
                        "Allowed is 8,16,32",
                        (int_T)val);
                return -1;
        }

        RETURN_ON_ERROR(get_numeric_field(slave, context, __LINE__,
                    sdo_config_field, idx, 1, 0, 0, "Value", &val));
        sdo_config->value = val;

        pr_debug(slave, context, "", 1,
                "Index #x%04x:%u = %u (%u bit)\n",
                sdo_config->index,
                sdo_config->subindex,
                sdo_config->value,
                sdo_config->datatype->mant_bits);
    }

    return 0;
}

static int_T
get_soe_config(struct ecat_slave *slave)
{
    const mxArray *soe_config_field = ssGetSFcnParam(slave->S, SOE_CONFIG);

    struct soe_config *soe_config;
    const char_T *param = "SOE_CONFIG";
    char_T context[100];
    uint_T soe_config_count;

    soe_config_count = mxGetNumberOfElements(soe_config_field);

    pr_debug(slave, NULL, NULL, 0,
            "--------------- Parsing SoE Configuration ------------\n");

    CHECK_CALLOC(slave->S, soe_config_count, sizeof(*slave->soe_config),
            slave->soe_config);
    slave->soe_config_end = slave->soe_config + soe_config_count;

    pr_info(slave, param, NULL, 0,
            "Found %u SoE configuration options\n", soe_config_count);

    for (soe_config = slave->soe_config;
            soe_config != slave->soe_config_end; soe_config++) {
        real_T val;
        uint_T idx = soe_config - slave->soe_config;

        snprintf(context, sizeof(context), "%s(%u)", param, idx+1);

        RETURN_ON_ERROR(get_numeric_field(slave, context, __LINE__,
                    soe_config_field, idx, 0, 0, 0, "Index", &val));
        soe_config->index = val;

        RETURN_ON_ERROR(get_char_field(slave, context, __LINE__,
                    soe_config_field, idx, "OctetString",
                    &soe_config->octet_string,
                    &soe_config->octet_string_len));

#ifndef _WIN32
        if (slave->debug_level >= 2) {
            char_T s[4*soe_config->octet_string_len + 1];
            uint_T i, offset = 0;

            for (i = 0; i < soe_config->octet_string_len; i++) {
                offset += sprintf(s + offset,
                        isprint(soe_config->octet_string[i])
                        ? "%c" : "\\x%02x", soe_config->octet_string[i]);
            }

            pr_debug(slave, context, "", 1,
                    "Index #x%04x = \"%s\" (%li bytes)\n",
                    soe_config->index, s, soe_config->octet_string_len);
        }
#endif
    }

    return 0;
}

static struct pdo_entry *find_pdo_entry(const struct ecat_slave *slave,
        const char_T *p_ctxt, const char_T *ctxt,
        unsigned int line_no, unsigned int indent,
        uint16_T pdo_entry_index, uint8_T pdo_entry_subindex)
{
//    struct pdo *pdo;
//    struct pdo_entry *pdo_entry;
//
//    for (pdo = slave->pdo; pdo != slave->pdo_end; pdo++) {
//        for (pdo_entry = pdo->entry;
//                pdo_entry != pdo->entry_end; pdo_entry++) {
//            if (pdo_entry->index == pdo_entry_index
//                    && pdo_entry->subindex == pdo_entry_subindex) {
//                if (pdo_entry->pdo->exclude_from_config) {
//                    pr_error(slave, p_ctxt, ctxt, line_no,
//                            "PDO Entry Index #x%04X.%i was found, "
//                            "but this Pdo is explicitly excluded by "
//                            "user configuration.",
//                            pdo_entry_index, pdo_entry_subindex);
//                    return NULL;
//                }
//                else if (pdo_entry->pdo->excluded_by) {
//                    pr_error(slave, p_ctxt, ctxt, line_no,
//                            "PDO Entry Index #x%04X.%i was found, "
//                            "but this Pdo is explicitly excluded by "
//                            "Pdo #x%04x.",
//                            pdo_entry_index, pdo_entry_subindex,
//                            pdo_entry->pdo->excluded_by->index);
//                    return NULL;
//                }
//                return pdo_entry;
//            }
//        }
//    }
//
//    pr_error(slave, p_ctxt, ctxt, line_no,
//            "Could not find PDO Entry Index #x%04X.%i",
//            pdo_entry_index, pdo_entry_subindex);
    return NULL;
}

static int_T get_parameter_values(const struct ecat_slave *slave,
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

static int_T
get_pdo_config(struct ecat_slave *slave,
        const char_T *p_ctxt, unsigned int p_indent, const mxArray *pdo_spec)
{
//    const mxArray *sm_assign;
//    const mxArray *pdo_select;
//    uint_T count, i;
//    char_T ctxt[100];
//
//    pr_debug(slave, NULL, NULL, 0,
//        "--------------- Custom slave configuration ------------\n");
//
//
//    if (!pdo_spec) {
//        pr_info(slave, NULL, NULL, 0,
//                "No custom specification found\n");
//        return 0;
//    }
//
//    snprintf(ctxt, sizeof(ctxt), "%s.Pdo", p_ctxt);
//
//    sm_assign = mxGetField(pdo_spec, 0, "SmAssignment");
//    if (sm_assign && (count = mxGetM(sm_assign))) {
//        real_T *pdo_val, *sm_idx_val;
//        uint_T i;
//
//        pr_debug(slave, ctxt, "SmAssignment", 1,
//                "Found specification for %u Pdo Indices\n", count);
//
//        if (!mxIsNumeric(sm_assign)
//                || mxGetN(sm_assign) != 2
//                || !(pdo_val = mxGetPr(sm_assign))) {
//            pr_error(slave, ctxt, "SmAssignment", __LINE__,
//                    "Value is not numeric M-by-2 array");
//            return -1;
//        }
//        sm_idx_val = pdo_val + count;
//
//        for (i = 0; i < count; i++) {
//            uint_T pdo_idx = pdo_val[i];
//            struct sync_manager *sm;
//            struct pdo *pdo;
//
//            snprintf(ctxt, sizeof(ctxt),
//                    "Pdo.SmAssignment(%u) = [#x%04X, %i]", i+1,
//                    (int_T)pdo_val[i], (int_T)sm_idx_val[i]);
//            pr_debug(slave, NULL, "", 2,
//                    "Trying to reassign PDO #x%04X to SyncManager %u\n",
//                    (int_T)pdo_val[i], (int_T)sm_idx_val[i]);
//
//            if (pdo_val[i] <= 0.0 && sm_idx_val[i] < 0.0) {
//                pr_error(slave, p_ctxt, ctxt, __LINE__,
//                        "Negative values not allowed in SyncManager "
//                        "assignment specifiction");
//                return -1;
//            }
//
//            for ( pdo = slave->pdo; /* */; pdo++) {
//                if (pdo == slave->pdo_end) {
//                    /* The pdo specified is not found */
//                    pr_error(slave, p_ctxt, ctxt, __LINE__,
//                            "The Pdo index #x%04X was not found\n", pdo_idx);
//                    return -1;
//                }
//
//                if (pdo->index == pdo_idx)
//                    break;
//            }
//            sm = check_pdo_syncmanager(slave, p_ctxt, ctxt,
//                    pdo, sm_idx_val[i], 0);
//            if (!sm)
//                return -1;
//
//            if (pdo->sm && pdo->sm != sm) {
//                pr_warn(slave, p_ctxt, ctxt, __LINE__,
//                        "Reassigning PDO #x%04X from SyncManager %u "
//                        "to %u\n",
//                        pdo->index, pdo->sm->index, sm->index);
//            }
//
//            pdo->sm = sm;
//            return 0;
//        }
//    }
//
//    pdo_select = mxGetField(pdo_spec, 0, "Exclude");
//    if (pdo_select
//            && (count = mxGetNumberOfElements(pdo_select))) {
//        real_T *val;
//        struct pdo *pdo;
//
//        snprintf(ctxt, sizeof(ctxt), "%s.Pdo", p_ctxt);
//
//        if (!mxIsNumeric(pdo_select) || !(val = mxGetPr(pdo_select))) {
//            pr_error(slave, ctxt, "Exclude", __LINE__,
//                    "Value is not a numeric vector");
//            return -1;
//        }
//        pr_debug(slave, NULL, "", 1,
//                "Found PDO selection list with %u elements\n",
//                count);
//        for (i = 0; i < count; i++) {
//            uint_T idx = abs(val[i]);
//            char_T idx_ctxt[20];
//            snprintf(idx_ctxt, sizeof(idx_ctxt), "Exclude(%u)", i+1);
//
//            for (pdo = slave->pdo; pdo != slave->pdo_end; pdo++) {
//                if (pdo->index == idx) {
//                    if (pdo->mandatory) {
//                        pr_error(slave, ctxt, idx_ctxt, __LINE__,
//                                "Cannot exclude mandatory Pdo #x%04X ",
//                                pdo->index);
//                        return -1;
//                    }
//                    pdo->exclude_from_config = 1;
//                    pr_debug(slave, NULL, "", 2,
//                            "Excluding Pdo #x%04X in configuration\n", idx);
//                    break;
//                }
//            }
//            if (pdo == slave->pdo_end) {
//                pr_warn(slave, ctxt, idx_ctxt, __LINE__,
//                        "Pdo Index #x%04X was not found in configuration\n",
//                        idx);
//            }
//        }
//    }
//
    return 0;
}

static int_T
get_ioport_config(struct ecat_slave *slave)
{
//    const mxArray * const io_spec = ssGetSFcnParam(slave->S, PORT_SPEC);
//    const mxArray *port_spec;
//    const char_T *param = "IO_SPEC";
//
//    real_T val;
//    uint_T io_spec_idx, io_spec_count;
//
//    pr_debug(slave, NULL, NULL, 0,
//            "--------------- Parsing IOSpec ------------\n");
//
//    if (!io_spec) {
//        pr_info(slave, NULL, NULL, 0,
//                "No block input and output configuration spec "
//                "was found\n");
//        return 0;
//    }
//
//    /* Parse the parameters IOSpec.Pdo first */
//    RETURN_ON_ERROR(get_pdo_config(slave, param, 2,
//                mxGetField(io_spec, 0, "Pdo")));
//
//    if (!(port_spec = mxGetField(io_spec, 0, "Port"))
//            || !(io_spec_count = mxGetNumberOfElements(port_spec))) {
//        return 0;
//    }
//
//    CHECK_CALLOC(slave->S, io_spec_count, sizeof(struct io_port),
//            slave->io_port);
//    slave->io_port_end = slave->io_port + io_spec_count;
//
//    pr_debug(slave, NULL, "", 1, "Processing %u IO Ports\n", io_spec_count);
//
//    for (io_spec_idx = 0; io_spec_idx < io_spec_count; io_spec_idx++) {
//        struct io_port *port = slave->io_port + io_spec_idx;
//        const mxArray *pdo_entry_spec =
//            mxGetField(port_spec, io_spec_idx, "PdoEntry");
//        uint_T pdo_entry_idx_count;
//        uint_T pdo_entry_idx;
//        boolean_T struct_access;
//        real_T *pdo_entry_index_field;
//        real_T *pdo_entry_subindex_field;
//        char_T port_ctxt[100];
//        const char_T *pdo_entry_ctxt = "PdoEntry";
//        int_T rc;
//
//        snprintf(port_ctxt, sizeof(port_ctxt),
//                "%s.Port(%u)", param, io_spec_idx+1);
//
//        pr_debug(slave, NULL, "", 2,
//                "Processing IOSpec %u\n", io_spec_idx+1);
//
//        if (!pdo_entry_spec) {
//            pr_error(slave, port_ctxt, pdo_entry_ctxt, __LINE__,
//                    "Parameter does not exist");
//            return -1;
//        }
//
//        /* The field 'PdoEntry' is allowed to be a struct or a matrix.
//         * Check what was specified */
//        if (mxIsStruct(pdo_entry_spec)) {
//            /* Mapped pdos is defined using a structure array with
//             * the fields 'Index', 'EntryIndex' and 'EntrySubIndex' */
//            struct_access = 1;
//
//            pdo_entry_idx_count = mxGetNumberOfElements(pdo_entry_spec);
//            if (!pdo_entry_idx_count) {
//                pr_error(slave, port_ctxt, pdo_entry_ctxt, __LINE__,
//                        "Structure is empty.\n");
//                return -1;
//            }
//
//            /* Stop the compiler from complaining */
//            pdo_entry_index_field = pdo_entry_subindex_field = NULL;
//        }
//        else if ((pdo_entry_idx_count = mxGetM(pdo_entry_spec))) {
//            /* Mapped pdos is defined using a M-by-2 array, where
//             * the columns define the 'EntryIndex' and
//             * 'EntrySubIndex' */
//            int_T columns = mxGetN(pdo_entry_spec);
//            pdo_entry_index_field = mxGetPr(pdo_entry_spec);
//
//            struct_access = 0;
//
//            /* Check that everything is correct */
//            if ( !mxIsNumeric(pdo_entry_spec)
//                    || !pdo_entry_idx_count
//                    || columns != 2
//                    || !pdo_entry_index_field) {
//                pr_error(slave, port_ctxt, pdo_entry_ctxt, __LINE__,
//                        "Parameter is not a M-by-2 or M-by-3 numeric array");
//                return -1;
//            }
//
//            /* PdoEntrySubindex follows PdoEntryIndex */
//            pdo_entry_subindex_field =
//                pdo_entry_index_field + pdo_entry_idx_count;
//
//        }
//        else {
//            pr_error(slave, port_ctxt, pdo_entry_ctxt, __LINE__,
//                    "Ignoring unknown specification.\n");
//            return -1;
//        }
//
//        CHECK_CALLOC(slave->S, pdo_entry_idx_count, sizeof(struct pdo_entry*),
//                port->pdo_entry);
//        port->pdo_entry_end = port->pdo_entry + pdo_entry_idx_count;
//
//        /* Check whether "DataType" for the port was specified explicitly,
//         * and override port->pdo_data_type in that case */
//        RETURN_ON_ERROR(rc = get_data_type_from_class(slave, port_ctxt,
//                    __LINE__, port_spec, io_spec_idx, "DataType",
//                    &port->pdo_data_type, 1));
//        if (!rc) {
//            pr_debug(slave, NULL, "", 3,
//                    "Custom DataType specification: %s\n",
//                    port->pdo_data_type->name);
//        }
//
//        /* Get the list of pdo entries that are mapped to this port */
//        for (pdo_entry_idx = 0;
//                pdo_entry_idx < pdo_entry_idx_count; pdo_entry_idx++) {
//            uint16_T pdo_entry_index;
//            uint8_T pdo_entry_subindex;
//            char_T pdo_entry_array_ctxt[50];
//            struct pdo_entry *pdo_entry;
//            uint_T *exclude_index;
//
//            snprintf(pdo_entry_array_ctxt, sizeof(pdo_entry_array_ctxt),
//                    "%s(%u)", pdo_entry_ctxt, pdo_entry_idx+1);
//
//            if (struct_access) {
//                char_T struct_ctxt[100];
//                snprintf(struct_ctxt, sizeof(struct_ctxt),
//                        "%s.%s", port_ctxt, pdo_entry_array_ctxt);
//
//                RETURN_ON_ERROR(rc = get_numeric_field(
//                            slave, struct_ctxt, __LINE__,
//                            pdo_entry_spec, pdo_entry_idx, 0, 1, 0,
//                            "PdoIndex", &val));
//                pdo_entry_index = rc ? 0 : val;
//
//                RETURN_ON_ERROR(get_numeric_field(
//                            slave, struct_ctxt, __LINE__,
//                            pdo_entry_spec, pdo_entry_idx, 0, 0, 0,
//                            "PdoEntryIndex", &val));
//                pdo_entry_index = val;
//
//                RETURN_ON_ERROR(get_numeric_field(
//                            slave, struct_ctxt, __LINE__,
//                            pdo_entry_spec, pdo_entry_idx, 0, 0, 0,
//                            "PdoEntrySubIndex", &val));
//                pdo_entry_subindex = val;
//            }
//            else {
//                pdo_entry_index    = *pdo_entry_index_field++;
//                pdo_entry_subindex = *pdo_entry_subindex_field++;
//            }
//
//            /* Get the pdo and pdo entry pointers for the specified
//             * indices */
//            if (!(pdo_entry = find_pdo_entry(slave,
//                            port_ctxt, pdo_entry_array_ctxt, __LINE__, 2,
//                            pdo_entry_index, pdo_entry_subindex))) {
//                return -1;
//            }
//            port->pdo_entry[pdo_entry_idx] = pdo_entry;
//
//            /* Go through the exclude list to mark other pdo's as
//             * excluded */
//            for (exclude_index = pdo_entry->pdo->exclude_index;
//                    exclude_index != pdo_entry->pdo->exclude_index_end;
//                    exclude_index++) {
//                struct pdo *pdo;
//
//                for (pdo = slave->pdo; pdo != slave->pdo_end; pdo++) {
//                    if (pdo->index != *exclude_index)
//                        continue;
//
//                    if (pdo->use_count) {
//                        pr_error(slave, port_ctxt, pdo_entry_array_ctxt,
//                                __LINE__,
//                                "PDO Entry Index #x%04X.%i "
//                                "of Pdo Index #x%04x excludes using "
//                                "Pdo Index #x%04x simultaneously.",
//                                pdo_entry_index, pdo_entry_subindex,
//                                pdo_entry->pdo->index,
//                                *exclude_index);
//                        return -1;
//                    }
//                    pdo->excluded_by = pdo_entry->pdo;
//                }
//            }
//
//            /* A Pdo Entry may only be used once */
//            if (pdo_entry->io_port) {
//                pr_warn(slave, port_ctxt, pdo_entry_array_ctxt, __LINE__,
//                        "Pdo Entry #x%04x:#x%04x.%u is already assigned to "
//                        "%s port %i",
//                        pdo_entry->pdo->index,
//                        pdo_entry_index, pdo_entry_subindex,
//                        (pdo_entry->io_port->pdo_entry[0]->pdo->direction
//                         ? "input" : "output"),
//                        pdo_entry->io_port->index);
//            } else {
//                pdo_entry->io_port = port;
//            }
//
//            /* Make sure that the Pdo is included in the configuration
//             * by incrementing its use count */
//            pdo_entry->pdo->use_count++;
//
//            if (!pdo_entry->pdo->sm) {
//                struct sync_manager *sm = slave->sync_manager_end;
//
//                if (pdo_entry->pdo->virtual) {
//                    if (pdo_entry->pdo->su_idx != ~0U) {
//                        /* Looking at Beckhoff's .xml files, it looks as
//                         * though the SyncUnit points to the SyncManager */
//                        sm = slave->sync_manager + pdo_entry->pdo->su_idx;
//                    }
//                    else if (slave->virtual_sm_count == 1) {
//                        for (sm = slave->sync_manager;
//                                sm != slave->sync_manager_end
//                                && !sm->virtual; sm++);
//                    }
//                }
//                else if (1 == (pdo_entry->pdo->direction 
//                        ? slave->output_sm_count : slave->input_sm_count)) {
//                    unsigned int expected_sm_dir = pdo_entry->pdo->direction
//                        ? EC_SM_OUTPUT : EC_SM_INPUT;
//
//                    for (sm = slave->sync_manager;
//                            sm->direction != expected_sm_dir
//                            && sm != slave->sync_manager_end; sm++);
//                }
//
//                if (sm < slave->sync_manager_end) {
//                    pdo_entry->pdo->sm = check_pdo_syncmanager(
//                            slave, port_ctxt, pdo_entry_array_ctxt,
//                            pdo_entry->pdo, sm - slave->sync_manager, 1);
//                    if (!pdo_entry->pdo->sm)
//                        /* Error reported by check_pdo_syncmanager */
//                        return -1;
//                }
//                else {
//                    pr_error(slave, port_ctxt, pdo_entry_array_ctxt,
//                            __LINE__,
//                            "Cannot guess which SyncManager Pdo #x%04x "
//                            "to be assigned to. Use manual configuration.",
//                            pdo_entry->pdo->index);
//                    return -1;
//                }
//            }
//            else if (pdo_entry->pdo->sm->virtual
//                    && !pdo_entry->pdo->sm->direction
//                    && !check_pdo_syncmanager(
//                        slave, port_ctxt, pdo_entry_array_ctxt,
//                        pdo_entry->pdo,
//                        pdo_entry->pdo->sm - slave->sync_manager, 1)) {
//                /* Error reported by check_pdo_syncmanager */
//                return -1;
//            }
//
//            /* Check that the direction is consistent */
//            if (port->pdo_entry[0]->pdo->direction
//                    != pdo_entry->pdo->direction) {
//                pr_error(slave, port_ctxt, pdo_entry_array_ctxt,
//                        __LINE__,
//                        "Cannot mix Pdo directions on the same port\n"
//                        "The first Pdo Entry mapped to this port is "
//                        "a %sPdo, whereas this is a %sPdo.",
//                        port->pdo_entry[0]->pdo->direction ? "Rx" : "Tx",
//                        pdo_entry->pdo->direction ? "Rx" : "Tx");
//                return -1;
//            }
//
//            /* Must have same data types */
//            if (port->pdo_entry[0]->data_type != pdo_entry->data_type) {
//                pr_error(slave, port_ctxt, pdo_entry_array_ctxt, __LINE__,
//                        "Cannot mix datatypes on the same port.\n"
//                        "The first Pdo Entry mapped to this port has "
//                        "data type %s, whereas this Pdo Entry has "
//                        "datatype %s",
//                        port->pdo_entry[0]->data_type->name,
//                        pdo_entry->data_type->name);
//                return -1;
//            }
//
//            /* If the port's data type has not been specified using
//             * IOSpec.port(x).DataType, then use the pdo data type */
//            if (!port->pdo_data_type) {
//                port->pdo_data_type = pdo_entry->data_type;
//            }
//
//            /* Tell the Pdo Entry how many elements it will be broken
//             * into */
//            pdo_entry->vector_length =
//                pdo_entry->bitlen / port->pdo_data_type->mant_bits;
//            port->width += pdo_entry->vector_length;
//
//            pdo_entry->pwork_index = slave->pwork_count;
//            slave->pwork_count += pdo_entry->vector_length;
//
//            if (port->pdo_data_type->mant_bits % 8) {
//                pdo_entry->iwork_index = slave->iwork_count;
//                slave->iwork_count += pdo_entry->vector_length;
//            }
//            else {
//                pdo_entry->iwork_index = -1;
//            }
//
//            pr_debug(slave, NULL, "", 3,
//                    "Added Pdo Entry #x%04x:#x%04X.%u\n",
//                    pdo_entry->pdo->index,
//                    pdo_entry->index,
//                    pdo_entry->subindex);
//        }
//
//        /* Check that a custom data type specification has a bitlen
//         * smaller than that of the pdo */
//        if (port->pdo_data_type->mant_bits
//                > port->pdo_entry[0]->data_type->mant_bits) {
//            pr_error(slave, port_ctxt, "DataType", __LINE__,
//                    "Cannot choose port data type %s for output port %u\n"
//                    "The bit length is greater than that "
//                    "of the pdo (%s)",
//                    port->pdo_data_type->name, port->index,
//                    port->pdo_entry[0]->data_type->name);
//            return -1;
//        }
//
//        /* Now that the port's direction is known, get its index */
//        if (port->pdo_entry[0]->pdo->direction) {
//            port->index = slave->input_port_count++;
//            slave->input_pdo_entry_count += pdo_entry_idx_count;
//        }
//        else {
//            port->index = slave->output_port_count++;
//            slave->output_pdo_entry_count += pdo_entry_idx_count;
//        }
//        pr_debug(slave, NULL, "", 3,
//                "%s Port %u; Vector length %u\n",
//                port->pdo_entry[0]->pdo->direction ? "Input" : "Output",
//                port->index+1,
//                port->width);
//
//        /* By default, make the port's output data type compatible to
//         * that of the Pdo. This can be overridden when PdoFullScale
//         * is specified, and has to be overridden when the data type
//         * cannot be represented by any simulink data type, such as
//         * Uint64 */
//        port->sl_data_type = port->pdo_data_type->sl_type;
//
//        /* If PdoFullScale is specified, the port data type is double.
//         * If port->pdo_full_scale == 0.0, it means that the port data
//         * type is specified by PortBitLen */
//        RETURN_ON_ERROR(rc = get_numeric_field(slave, port_ctxt, __LINE__,
//                    port_spec, io_spec_idx, 0, 1, 0, "PdoFullScale", &val));
//        if (!rc) {
//            if (val)
//                port->pdo_full_scale = val;
//            else {
//                switch (port->pdo_entry[0]->data_type->type) {
//                    case Unsigned:
//                        port->pdo_full_scale =
//                            pow(2, port->pdo_entry[0]->data_type->mant_bits);
//                        break;
//                    case Signed:
//                        port->pdo_full_scale =
//                            pow(2, port->pdo_entry[0]->data_type->mant_bits - 1);
//                        break;
//                    default:
//                        port->pdo_full_scale = 1.0;
//                }
//            }
//
//            /* The port data type is always double when using scaling */
//            port->sl_data_type = SS_DOUBLE;
//
//            RETURN_ON_ERROR(port->gain_count = get_parameter_values(slave,
//                        pdo_entry_ctxt, port_spec, io_spec_idx, "Gain",
//                        port->width, &port->gain_values,
//                        &port->gain_name));
//
//            /* Offset and filter only for output ports */
//            if (!port->pdo_entry[0]->pdo->direction) {
//                RETURN_ON_ERROR(
//                        port->offset_count = get_parameter_values(slave,
//                            pdo_entry_ctxt, port_spec, io_spec_idx, "Offset",
//                            port->width, &port->offset_values,
//                            &port->offset_name));
//                RETURN_ON_ERROR(
//                        port->filter_count = get_parameter_values(slave,
//                            pdo_entry_ctxt, port_spec, io_spec_idx, "Filter",
//                            port->width, &port->filter_values,
//                            &port->filter_name));
//                slave->filter_count += port->filter_count ? port->width : 0;
//            }
//        }
//
//        /* At this point, the data type must be specified. If it is still
//         * DYNAMICALLY_TYPED, it means that the Pdo data type cannot be
//         * represented by any native simulink data type */
//        if (port->sl_data_type == DYNAMICALLY_TYPED
//                && !port->pdo_entry[0]->pdo->direction) {
//            pr_error(slave, port_ctxt, "", __LINE__,
//                    "Cannot assign a data type for output port %u "
//                    "because the Pdo's data type (%s) cannot be mapped to "
//                    "a native Simulink data type.\n"
//                    "Use %s.DataType to specify one explicitly,\n"
//                    "or specify %s.PdoFullScale = 0.0 to make it a double",
//                    port->index+1, port->pdo_data_type->name,
//                    port_ctxt, port_ctxt);
//            return -1;
//        }
//
//        if (slave->max_port_width) {
//            if (slave->max_port_width > 1 && port->width > 1
//                    && slave->max_port_width != port->width) {
//                slave->have_mixed_port_widths = 1;
//                pr_info(slave, "Set IO Port Width", "", 0,
//                        "Setting mixed port widths\n");
//            }
//
//            slave->max_port_width = max(slave->max_port_width, port->width);
//        }
//        else {
//            slave->max_port_width = port->width;
//        }
//
//        if (port->gain_count) {
//            if (port->gain_name)
//                slave->runtime_param_count += port->gain_count;
//            else {
//                slave->constant_count += port->gain_count;
//            }
//        }
//
//        if (port->offset_count) {
//            if (port->offset_name)
//                slave->runtime_param_count += port->offset_count;
//            else {
//                slave->constant_count += port->offset_count;
//            }
//        }
//
//        if (port->filter_count) {
//            if (port->filter_name)
//                slave->runtime_param_count += port->filter_count;
//            else {
//                slave->constant_count += port->filter_count;
//            }
//        }
//
//    }
    return 0;
}

/* This function is used to operate on the allocated memory with a generic
 * operator. This function is used to fix or release allocated memory.
 */
static void
slave_mem_op(struct ecat_slave *slave, void (*method)(void*))
{
    struct io_port *port;
//    struct sync_manager *sm;
//    struct pdo *pdo;
    struct dc_opmode *om;
    struct soe_config *sc;

    (*method)(slave->type);
    (*method)(slave->sdo_config);

    for (sc = slave->soe_config; sc != slave->soe_config_end; sc++)
        (*method)(sc->octet_string);
    (*method)(slave->soe_config);

//    for (pdo = slave->pdo; pdo != slave->pdo_end; pdo++) {
//        (*method)(pdo->exclude_index);
//        (*method)(pdo->entry);
//    }
//    (*method)(slave->pdo);

//    for (sm = slave->sync_manager; sm != slave->sync_manager_end; sm++)
//        (*method)(sm->pdo_ptr);
    (*method)(slave->sync_manager);

    for (om = slave->dc_opmode; om != slave->dc_opmode_end; om++) {
        struct opmode_sm *sm;
        for (sm = om->sm; sm != om->sm_end; sm++) {
            (*method)(sm->pdo);
        }
        (*method)(om->sm);
    }
    (*method)(slave->dc_opmode);

//    (*method)(slave->io_sync_manager);

    for (port = slave->io_port; port != slave->io_port_end; port++) {
        (*method)(port->pdo_entry);
        (*method)(port->gain_name);
        (*method)(port->gain_values);
        (*method)(port->offset_name);
        (*method)(port->offset_values);
        (*method)(port->filter_name);
        (*method)(port->filter_values);
    }
    (*method)(slave->io_port);

    (*method)(slave->input_port);
    (*method)(slave->output_port);

    (*method)(slave);
}

static int_T
check_pdo_configuration(struct ecat_slave *slave)
{
//    struct pdo *pdo, *pdoX;
//    struct io_port *io_port;
//    struct sync_manager *sm;
//    const char_T *ctxt = "Pdo Configuration Check";
//
//    for (pdo = slave->pdo; pdo != slave->pdo_end; pdo++) {
//        if (pdo->exclude_from_config && pdo->use_count) {
//            pr_error(slave, ctxt, "", __LINE__,
//                    "Problem with PDO #x%04X: Pdo was explicitly excluded "
//                    "from configuration, however somehow it was assigned "
//                    "to a port. This cannot be.", pdo->index);
//            return -1;
//        }
//        pr_info(slave, NULL, "", 1, "Checking PDO #x%04X; use count %u\n",
//                pdo->index, pdo->use_count);
//
//        if (pdo->exclude_from_config)
//            continue;
//
//        if (!pdo->sm) {
//            if (pdo->use_count) {
//                /* Try to assign a SyncManager to the PDO */
//
//                uint_T pdo_sm_dir = 
//                    pdo->direction ? EC_SM_OUTPUT : EC_SM_INPUT;
//                uint_T sm_dir_count = pdo->direction
//                    ? slave->output_sm_count : slave->input_sm_count;
//
//                for (sm = slave->sync_manager; 
//                        sm != slave->sync_manager_end; sm++ ) {
//                    if (sm->virtual != pdo->virtual 
//                            || sm->direction != pdo_sm_dir)
//                        continue;
//                    if (1 == sm_dir_count) {
//                        pdo->sm = sm;
//                        pr_info(slave, ctxt, "", 1,
//                                "Automatically assigning SyncManager %u "
//                                "for PDO Index #x%04X\n", 
//                                sm->index, pdo->index);
//                        break;
//                    }
//                    pr_error(slave, ctxt, "", __LINE__,
//                            "Pdo #x%04X was neither assigned a SyncManager "
//                            "in the EtherCATInfo nor using "
//                            "the custom SmAssignment option.\n"
//                            "Since the slave has %u %s SyncManagers "
//                            "an automatic choice is not possible",
//                            pdo->index, sm_dir_count, 
//                            (sm->direction == EC_SM_INPUT 
//                             ? "input" : "output"));
//                    return -1;
//                }
//            }
//            else {
//                pr_info(slave, ctxt, "", 1,
//                        "Automatically deselecting PDO #x%04X from the "
//                        "SyncManager configuration because it does not "
//                        "have a SyncManager assigned and it is unused\n",
//                        pdo->index);
//                pdo->exclude_from_config = 1;
//                slave->unique_config = 1;
//                continue;
//            }
//        }
//
//        slave->pdo_count++;
//        slave->pdo_entry_count += pdo->entry_end - pdo->entry;
//        pdo->sm->pdo_ptr_end++;
//
//        /* The PDO will be configured. Make sure that it does not exist
//         * in the exclusion list of another Pdo */
//        for (pdoX = slave->pdo; pdoX != pdo; pdoX++) {
//            uint_T *exclude_pdo;
//
//            if (pdoX->exclude_from_config)
//                continue;
//
//            for (exclude_pdo = pdoX->exclude_index;
//                    exclude_pdo != pdoX->exclude_index_end; exclude_pdo++) {
//
//                if (pdo->index == *exclude_pdo) {
//                    char_T msg[100];
//
//                    snprintf(msg, sizeof(msg),
//                            "PDO #x%04X and #x%04X mutually "
//                            "exclude each other.", pdo->index, pdoX->index);
//
//                    if (pdoX->use_count && pdo->use_count) {
//                        pr_error(slave, ctxt, "", __LINE__,
//                                "%s\n"
//                                "Both are in use. Check the "
//                                "slave's configuration.", msg);
//                        return -1;
//
//                    }
//                    else {
//                        /* One of them is unused, so deselect it
//                         * automatically, making sure that the slave's
//                         * pdo_count and pdo_entry_count are updated
//                         * too */
//                        if (pdo->use_count) {
//                            pdoX->exclude_from_config = 1;
//                            slave->pdo_count--;
//                            slave->pdo_entry_count -=
//                                pdoX->entry_end - pdoX->entry;
//                        }
//                        else {
//                            pdo->exclude_from_config = 1;
//                            slave->pdo_count--;
//                            slave->pdo_entry_count -=
//                                pdo->entry_end - pdo->entry;
//                        }
//
//                        pr_warn(slave, "PDO Configuration Check", "",
//                                __LINE__,
//                                "%s\n"
//                                "Because PDO #x%04X is unused "
//                                "it will be deselected automatically. "
//                                "This may cause problems.\n",
//                                msg,
//                                pdoX->use_count ? pdo->index : pdoX->index);
//
//                        /* Setting the unique flag so that this automatic
//                         * deselection does not affect other slave
//                         * configurations */
//                        slave->unique_config = 1;
//                    }
//                }
//            }
//        }
//    }
//
//    CHECK_CALLOC(slave->S, slave->input_port_count,
//            sizeof(*slave->input_port), slave->input_port);
//    CHECK_CALLOC(slave->S, slave->output_port_count,
//            sizeof(*slave->output_port), slave->output_port);
//
//    slave->input_port_count = 0;
//    slave->output_port_count = 0;
//
//    for (io_port = slave->io_port; 
//            io_port != slave->io_port_end; io_port++) {
//        if (io_port->pdo_entry[0]->pdo->direction) {
//            slave->input_port[slave->input_port_count++] = io_port;
//        }
//        else {
//            slave->output_port[slave->output_port_count++] = io_port;
//        }
//    }
//
//    CHECK_CALLOC(slave->S, slave->output_sm_count + slave->input_sm_count,
//            sizeof(struct sync_manager *), slave->io_sync_manager);
//    slave->io_sync_manager_end = slave->io_sync_manager;
//
//    /* While reading in all the PDO's, the pdo_ptr_end of the corresponding
//     * SyncManager was incremented as well. Now that it is known how
//     * many PDO's a SyncManager has, memory for a set of PDO pointers
//     * can be allocated. */
//    for (sm = slave->sync_manager; sm != slave->sync_manager_end; sm++) {
//        uint_T pdo_count = sm->pdo_ptr_end - sm->pdo_ptr;
//
//        CHECK_CALLOC(slave->S, pdo_count, sizeof(struct pdo*), sm->pdo_ptr);
//        sm->pdo_ptr_end = sm->pdo_ptr;
//
//        if (sm->direction == EC_SM_INPUT 
//                || sm->direction == EC_SM_OUTPUT) {
//            *slave->io_sync_manager_end++ = sm;
//        }
//    }
//
//    /* Now go through the pdo list again and setup the
//     * SyncManager's pdo pointers */
//    for (pdo = slave->pdo; pdo != slave->pdo_end; pdo++) {
//        if (pdo->sm) {
//            *pdo->sm->pdo_ptr_end++ = pdo;
//        }
//    }
//
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
//    struct io_port *port;

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

    if (get_sdo_config(slave)) return;

    if (get_soe_config(slave)) return;

    if (get_ioport_config(slave)) return;

    if (check_pdo_configuration(slave)) return;

    /* Process input ports */
    if (!ssSetNumInputPorts(S, slave->input_port_count))
        return;
    if (!ssSetNumOutputPorts(S, slave->output_port_count))
        return;

//    for (port = slave->io_port; port != slave->io_port_end; port++) {
//        if (port->pdo_entry[0]->pdo->direction) {
//            /* Input port */
//            ssSetInputPortWidth   (S, port->index, DYNAMICALLY_SIZED);
//            ssSetInputPortDataType(S, port->index, port->sl_data_type);
//        }
//        else {
//            /* Output port */
//            ssSetOutputPortWidth   (S, port->index, port->width);
//            ssSetOutputPortDataType(S, port->index, port->sl_data_type);
//        }
//    }

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

    /* Set the options. Note that SS_OPTION_EXCEPTION_FREE_CODE cannot
     * be set because this SFunction uses mxCalloc, which can cause
     * an exception */
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

    if (!slave)
        return;

    ssSetInputPortWidth(S, port, width);
    pr_debug(slave, ssGetPath(S), "", 0,
            "Setting input port width of port %u to %u\n",
            port+1, width);

    if (width > slave->input_port[port]->width) {
        snprintf(errmsg, sizeof(errmsg),
                "Trying to assign a vector of %u elements "
                "to input port %u, which has only %u objects.",
                width, port+1, slave->input_port[port]->width);
        ssSetErrorStatus(S, errmsg);
    }
}

/* This function is called when some ports are still DYNAMICALLY_SIZED
 * even after calling mdlSetInputPortWidth(). This occurs when input
 * ports are not connected. */
#define MDL_SET_DEFAULT_PORT_DIMENSION_INFO
static void mdlSetDefaultPortDimensionInfo(SimStruct *S)
{
    struct ecat_slave *slave = ssGetUserData(S);
//    struct io_port *port;

    if (!slave)
        return;

//    for (port = slave->io_port; port != slave->io_port_end; port++) {
//        if (port->pdo_entry[0]->pdo->direction) {
//            if (ssGetInputPortWidth(S, port->index) == DYNAMICALLY_SIZED) {
//                ssSetInputPortWidth(S, port->index, port->width);
//            }
//        }
//        else {
//            if (ssGetOutputPortWidth(S, port->index) == DYNAMICALLY_SIZED) {
//                ssSetOutputPortWidth(S, port->index, port->width);
//            }
//        }
//    }
}

#define MDL_SET_WORK_WIDTHS
static void mdlSetWorkWidths(SimStruct *S)
{
    struct ecat_slave *slave = ssGetUserData(S);
    struct io_port *port;
    uint_T param_idx = 0;

    if (!slave)
        return;

    ssSetNumPWork(S, slave->pwork_count);
    ssSetNumIWork(S, slave->iwork_count);

    if (slave->have_mixed_port_widths) {
        int_T port_idx = 0;
        for (port_idx = 0; port_idx < slave->input_port_count; port_idx++) {
            ssSetInputPortRequiredContiguous(S, port_idx, 1);
        }
    }

    ssSetNumRunTimeParams(S, slave->runtime_param_count);

    for (port = slave->io_port; port != slave->io_port_end; port++) {

//        /* Skip not connected input ports */
//        if (port->pdo_entry[0]->pdo->direction
//                && !ssGetInputPortConnected(S, port->index))
//            continue;

        if (port->gain_count && port->gain_name) {
            ssParamRec p;
            p.name = port->gain_name;
            p.nDimensions = 1;
            p.dimensions = &port->gain_count;
            p.dataTypeId = SS_DOUBLE;
            p.complexSignal = 0;
            p.data = port->gain_values;
            p.dataAttributes = NULL;
            p.nDlgParamIndices = 0;
            p.dlgParamIndices = NULL;
            p.transformed = RTPARAM_TRANSFORMED;
            p.outputAsMatrix = 0;
            ssSetRunTimeParamInfo(S, param_idx++, &p);
        }

        if (port->offset_count && port->offset_name) {
            ssParamRec p;
            p.name = port->offset_name;
            p.nDimensions = 1;
            p.dimensions = &port->offset_count;
            p.dataTypeId = SS_DOUBLE;
            p.complexSignal = 0;
            p.data = port->offset_values;
            p.dataAttributes = NULL;
            p.nDlgParamIndices = 0;
            p.dlgParamIndices = NULL;
            p.transformed = RTPARAM_TRANSFORMED;
            p.outputAsMatrix = 0;
            ssSetRunTimeParamInfo(S, param_idx++, &p);
        }

        if (port->filter_count && port->filter_name) {
            ssParamRec p;
            p.name = port->filter_name;
            p.nDimensions = 1;
            p.dimensions = &port->filter_count;
            p.dataTypeId = SS_DOUBLE;
            p.complexSignal = 0;
            p.data = port->filter_values;
            p.dataAttributes = NULL;
            p.nDlgParamIndices = 0;
            p.dlgParamIndices = NULL;
            p.transformed = RTPARAM_TRANSFORMED;
            p.outputAsMatrix = 0;
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

#define MDL_UPDATE
static void mdlUpdate(SimStruct *S, int_T tid)
{
    /** Needed for Matlab versions from 2007b, otherwise no update code is
     * generated.
     */
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
static char_T *
createStrVect(const char_T *strings[], uint_T count)
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
//    uint_T const_index = 0;
//    uint_T filter_index = 0;
    real_T *const_vector =
        mxCalloc(slave->constant_count, sizeof(real_T));
    /* General assignments of array indices that form the basis for
     * the S-Function <-> TLC communication
     * DO NOT CHANGE THESE without updating the TLC ec_test2.tlc
     * as well */
    enum {
        PortSpecDir = 0,                /* 0 */
        PortSpecPortIdx,                /* 1 */
        PortSpecPWork,                  /* 2 */
        PortSpecDType,                  /* 3 */
        PortSpecBitLen,                 /* 4 */
        PortSpecIWork,                  /* 5 */
        PortSpecGainCount,              /* 6 */
        PortSpecGainConstIdx,           /* 7 */
        PortSpecOffsetCount,            /* 8 */
        PortSpecOffsetConstIdx,         /* 9 */
        PortSpecFilterCount,            /* 10 */
        PortSpecFilterIdx,              /* 11 */
        PortSpecFilterConstIdx,         /* 12 */
        PortSpecMax                     /* 13 */
    };
    enum {
        SM_Index = 0,
        SM_Direction,
        SM_PdoCount,
        SM_PdoIndex,
        SM_Watchdog,
        SM_Max
    };
    enum {
        PdoEI_Index = 0,
        PdoEI_SubIndex,
        PdoEI_BitLen,
        PdoEI_Max
    };
    enum {
        PdoInfo_Index = 0,
        PdoInfo_EntryCount,
        PdoInfo_EntryIndex,
        PdoInfo_Max
    };
    enum {
        SdoConfigIndex = 0,         /* 0 */
        SdoConfigSubIndex,          /* 1 */
        SdoConfigDataType,          /* 2 */
        SdoConfigValue,             /* 3 */
        SdoConfigMax                /* 4 */
    };
    enum {
        PdoEntryIndex = 0, 		        /* 0 */
        PdoEntrySubIndex, 		        /* 1 */
        PdoEntryVectorLength,           /* 2 */
        PdoEntryDir,                    /* 3 */
        PdoEntryDTypeSize,              /* 4 */
        PdoEntryBitLen,                 /* 5 */
        PdoEntryPWork,                  /* 6 */
        PdoEntryIWork,                  /* 7 */
        PdoEntryMax                     /* 8 */
    };
    
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

    if (slave->sdo_config != slave->sdo_config_end) {
        /* Transpose slave->sdo_config */
        uint_T sdo_config_count = slave->sdo_config_end - slave->sdo_config;
        uint32_T (*sdo_config_out)[SdoConfigMax];
        struct sdo_config *sdo_config;

        sdo_config_out =
            mxCalloc( sdo_config_count, sizeof(*sdo_config_out));

        for (sdo_config = slave->sdo_config;
                sdo_config != slave->sdo_config_end; sdo_config++) {
            uint_T idx = sdo_config - slave->sdo_config;

            sdo_config_out[idx][SdoConfigIndex] = sdo_config->index;
            sdo_config_out[idx][SdoConfigSubIndex] = sdo_config->subindex;
            sdo_config_out[idx][SdoConfigDataType] =
                sdo_config->datatype->id;
            sdo_config_out[idx][SdoConfigValue] = sdo_config->value;
        }

        if (!ssWriteRTW2dMatParam(S, "SdoConfig", sdo_config_out,
                    SS_UINT32, SdoConfigMax, sdo_config_count))
            return;

        mxFree(sdo_config_out);
    }

    if (slave->soe_config != slave->soe_config_end) {
        uint_T soe_config_count = slave->soe_config_end - slave->soe_config;
        uint32_T *soe_index_out;
        struct soe_config *soe_config;
        uint_T idx;

        soe_index_out = mxCalloc(soe_config_count,sizeof(uint32_T));
        
        for (soe_config = slave->soe_config, idx = 0;
                soe_config != slave->soe_config_end;
                soe_config++, idx++) {
            char_T name[32];
            snprintf(name, sizeof(name), "SoeConfigData_%i", idx);
            soe_index_out[idx] = soe_config->index;
            if (!ssWriteRTWVectParam(S, name, soe_config->octet_string,
                        SS_UINT8, soe_config->octet_string_len))
                return;
        }

        if (!ssWriteRTWVectParam(S, "SoeConfigIndex", soe_index_out,
                    SS_UINT32, soe_config_count))
            return;

        mxFree(soe_index_out);
    }

    if (slave->sync_manager != slave->sync_manager_end) {
        uint_T sm_idx = 0;
        uint_T pdo_idx = 0;
        uint_T entry_idx = 0;
        uint32_T (*sm_map)[SM_Max];
        uint32_T (*pdo_map)[PdoInfo_Max];
        uint32_T (*pdo_entry_map)[PdoEI_Max];
        struct sync_manager *sm;

        sm_map = mxCalloc(
                slave->sync_manager_end - slave->sync_manager,
                sizeof(*sm_map));
        pdo_map = mxCalloc(slave->pdo_count, sizeof(*pdo_map));
        pdo_entry_map =
            mxCalloc(slave->pdo_entry_count, sizeof(*pdo_entry_map));
        
        for (sm = slave->sync_manager; sm != slave->sync_manager_end; sm++) {
            struct pdo *pdo;

            sm_map[sm_idx][SM_Index]     = sm->index;
            sm_map[sm_idx][SM_Direction] = sm->direction;
            sm_map[sm_idx][SM_PdoCount]  = sm->pdo_end - sm->pdo;
            sm_map[sm_idx][SM_PdoIndex]  = pdo_idx;
            sm_map[sm_idx][SM_Watchdog]  = sm->watchdog;
            sm_idx++;

            for (pdo = sm->pdo; pdo != sm->pdo_end; pdo++) {
                struct pdo_entry *pdo_entry;

                pdo_map[pdo_idx][PdoInfo_Index] = pdo->index;
                pdo_map[pdo_idx][PdoInfo_EntryCount] =
                    pdo->entry_end - pdo->entry;
                pdo_map[pdo_idx][PdoInfo_EntryIndex] = entry_idx;
                pdo_idx++;

                for (pdo_entry = pdo->entry;
                        pdo_entry != pdo->entry_end; pdo++) {
                    pdo_entry_map[entry_idx][PdoEI_Index] =
                        pdo_entry->index;
                    pdo_entry_map[entry_idx][PdoEI_SubIndex] =
                        pdo_entry->subindex;
                    pdo_entry_map[entry_idx][PdoEI_BitLen] =
                        pdo_entry->bitlen;

                    entry_idx++;
                }
            }
        }
        if (!ssWriteRTW2dMatParam(S, "PdoEntry_Map", pdo_entry_map,
                    SS_UINT32, PdoEI_Max, entry_idx))
            return;
        if (!ssWriteRTW2dMatParam(S, "Pdo_Map", pdo_map,
                    SS_UINT32, PdoInfo_Max, pdo_idx))
            return;
        if (!ssWriteRTW2dMatParam(S, "Sm_Map", sm_map,
                    SS_UINT32, SM_Max, sm_idx))
            return;
    }

//    if (slave->io_sync_manager_end != slave->io_sync_manager) {
//        uint_T sm_count = slave->io_sync_manager_end - slave->io_sync_manager;
//        uint32_T (*sync_manager)[SM_Max];
//        uint32_T (*pdo_info)[PdoInfo_Max];
//        uint_T mapped_pdo_entry_count = slave->input_pdo_entry_count
//            + slave->output_pdo_entry_count;
//        int32_T (*mapped_pdo_entry)[PdoEntryMax];
//        uint_T m_pdo_entry_idx = 0;
//        uint32_T (*pdo_entry_info)[PdoEI_Max];
//
//        uint_T pdo_idx = 0, pdo_entry_idx = 0;
//
//        struct sync_manager **sm_ptr;
//
//        sync_manager = mxCalloc(sm_count, sizeof(*sync_manager));
//        pdo_info = mxCalloc(slave->pdo_count, sizeof(*pdo_info));
//        mapped_pdo_entry =
//            mxCalloc(mapped_pdo_entry_count, sizeof(*mapped_pdo_entry));
//        pdo_entry_info =
//            mxCalloc(slave->pdo_entry_count, sizeof(*pdo_entry_info));
//        
//        for (sm_ptr = slave->io_sync_manager; 
//                sm_ptr != slave->io_sync_manager_end; sm_ptr++) {
//            struct pdo **pdo_ptr;
//
//            sync_manager[sm_idx_max][SM_Index] = (*sm_ptr)->index;
//            sync_manager[sm_idx_max][SM_Direction] =
//                (*sm_ptr)->direction == EC_SM_INPUT;
//            sync_manager[sm_idx_max][SM_PdoCount] = 0;
//
//            for (pdo_ptr = (*sm_ptr)->pdo_ptr; 
//                    pdo_ptr != (*sm_ptr)->pdo_ptr_end; pdo_ptr++) {
//                struct pdo_entry *pdo_entry;
//
//                if ((*pdo_ptr)->exclude_from_config)
//                    continue;
//
//                sync_manager[sm_idx_max][SM_PdoCount]++;
//
//                pdo_info[pdo_idx][PdoInfo_PdoIndex] =
//                    (*pdo_ptr)->index;
//                pdo_info[pdo_idx][PdoInfo_PdoEntryCount] =
//                    (*pdo_ptr)->entry_end - (*pdo_ptr)->entry;
//
//                for (pdo_entry = (*pdo_ptr)->entry;
//                        pdo_entry != (*pdo_ptr)->entry_end;
//                        pdo_entry++, pdo_entry_idx++) {
//                    pdo_entry_info[pdo_entry_idx][PdoEI_Index] =
//                        pdo_entry->index;
//                    pdo_entry_info[pdo_entry_idx][PdoEI_SubIndex] =
//                        pdo_entry->subindex;
//                    pdo_entry_info[pdo_entry_idx][PdoEI_BitLen] =
//                        pdo_entry->bitlen;
//
//                    if (!pdo_entry->io_port)
//                        continue;
//
//                    mapped_pdo_entry[m_pdo_entry_idx][PdoEntryIndex] =
//                        pdo_entry->index;
//                    mapped_pdo_entry[m_pdo_entry_idx][PdoEntrySubIndex] =
//                        pdo_entry->subindex;
//                    mapped_pdo_entry[m_pdo_entry_idx][PdoEntryDir] = 
//                        pdo_entry->pdo->direction;
//                    mapped_pdo_entry[m_pdo_entry_idx][PdoEntryVectorLength] =
//                        pdo_entry->vector_length;
//                    mapped_pdo_entry[m_pdo_entry_idx][PdoEntryDTypeSize] =
//                        pdo_entry->io_port->pdo_data_type->mant_bits / 8;
//                    mapped_pdo_entry[m_pdo_entry_idx][PdoEntryBitLen] =
//                        pdo_entry->io_port->pdo_data_type->mant_bits;
//                    mapped_pdo_entry[m_pdo_entry_idx][PdoEntryPWork] = 
//                        pdo_entry->pwork_index;
//
//                    mapped_pdo_entry[m_pdo_entry_idx][PdoEntryIWork] =
//                        pdo_entry->iwork_index;
//
//                    m_pdo_entry_idx++;
//                }
//                pdo_idx++;
//            }
//            if (sync_manager[sm_idx_max][SM_PdoCount])
//                sm_idx_max++;
//        }
//
//        if (mapped_pdo_entry_count) {
//            if (!ssWriteRTW2dMatParam(S, "MappedPdoEntry", mapped_pdo_entry,
//                        SS_INT32, PdoEntryMax, mapped_pdo_entry_count))
//                return;
//        }
//
//        if (slave->pdo_entry_count) {
//            if (!ssWriteRTW2dMatParam(S, "PdoEntryInfo", pdo_entry_info,
//                        SS_UINT32, PdoEI_Max, slave->pdo_entry_count))
//                return;
//        }
//        if (slave->pdo_count) {
//            if (!ssWriteRTW2dMatParam(S, "PdoInfo", pdo_info,
//                        SS_UINT32, PdoInfo_Max, slave->pdo_count))
//                return;
//        }
//        if (slave->use_dc_opmode) {
//            int32_T opmode[9];
//            struct time_sync *ts;
//
//            opmode[0] = slave->use_dc_opmode->assign_activate;
//
//            ts = &slave->use_dc_opmode->cycle_time_sync[0];
//            opmode[1] = ts->factor;
//            opmode[2] = ts->value;
//
//            ts = &slave->use_dc_opmode->shift_time_sync[0];
//            opmode[3] = ts->factor;
//            opmode[4] = ts->value;
//
//            ts = &slave->use_dc_opmode->cycle_time_sync[1];
//            opmode[5] = ts->factor;
//            opmode[6] = ts->value;
//
//            ts = &slave->use_dc_opmode->shift_time_sync[1];
//            opmode[7] = ts->factor;
//            opmode[8] = ts->value;
//
//            if (!ssWriteRTWVectParam(S, "DcOpMode", opmode, SS_INT32, 9))
//                return;
//        }
//
//        /* Don't need to check for slave->sync_manager_count here,
//         * was checked already */
//        if (!ssWriteRTW2dMatParam(S, "SyncManager", sync_manager,
//                    SS_UINT32, SM_Max,
//                    slave->io_sync_manager_end - slave->io_sync_manager))
//            return;
//        
//        mxFree(sync_manager);
//        mxFree(pdo_info);
//        mxFree(mapped_pdo_entry);
//        mxFree(pdo_entry_info);
//    }

//    if (slave->io_port_end != slave->io_port) {
//        uint_T io_port_count = slave->io_port_end - slave->io_port;
//        struct io_port *port;
//        int32_T (*port_spec)[PortSpecMax];
//        real_T *pdo_full_scale;
//        uint_T port_idx;
//        const char_T **gain_name_ptr;
//        const char_T **offset_name_ptr;
//        const char_T **filter_name_ptr;
//        const char_T *gain_str;
//        const char_T *offset_str;
//        const char_T *filter_str;
//    
//        port_spec       = mxCalloc(io_port_count, sizeof(*port_spec));
//        pdo_full_scale  = mxMalloc(io_port_count*sizeof(real_T));
//        gain_name_ptr   = mxCalloc(io_port_count, sizeof(char_T*));
//        offset_name_ptr = mxCalloc(io_port_count, sizeof(char_T*));
//        filter_name_ptr = mxCalloc(io_port_count, sizeof(char_T*));
//        
//        for (port_idx = 0, port = slave->io_port;
//                port_idx < io_port_count; port_idx++, port++) {
//
//            port_spec[port_idx][PortSpecDir] =
//                port->pdo_entry[0]->pdo->direction;
//            port_spec[port_idx][PortSpecPortIdx] = 
//                port->index;
//            port_spec[port_idx][PortSpecPWork] = 
//                port->pdo_entry[0]->pwork_index;
//            port_spec[port_idx][PortSpecDType] =
//                port->pdo_data_type->id;
//            port_spec[port_idx][PortSpecBitLen] =
//                port->pdo_data_type->mant_bits;
//            port_spec[port_idx][PortSpecIWork] = 
//                port->pdo_entry[0]->iwork_index;
//
//            if (port->gain_count) {
//                port_spec[port_idx][PortSpecGainCount] =
//                    port->gain_count;
//                if (port->gain_name) {
//                    port_spec[port_idx][PortSpecGainConstIdx] = -1;
//                    gain_name_ptr[port_idx] = port->gain_name;
//                }
//                else {
//                    memcpy(const_vector + const_index, port->gain_values,
//                            sizeof(*const_vector) * port->gain_count);
//                    port_spec[port_idx][PortSpecGainConstIdx] =
//                        const_index;
//                    const_index += port->gain_count;
//                }
//            }
//
//            if (port->offset_count) {
//                port_spec[port_idx][PortSpecOffsetCount] =
//                    port->offset_count;
//                if (port->offset_name) {
//                    port_spec[port_idx][PortSpecOffsetConstIdx] = -1;
//                    offset_name_ptr[port_idx] = port->offset_name;
//                }
//                else {
//                    memcpy(const_vector + const_index, port->offset_values,
//                            sizeof(*const_vector) * port->offset_count);
//                    port_spec[port_idx][PortSpecOffsetConstIdx] =
//                        const_index;
//                    const_index += port->offset_count;
//                }
//            }
//
//            if (port->filter_count) {
//                port_spec[port_idx][PortSpecFilterCount] =
//                    port->filter_count;
//                port_spec[port_idx][PortSpecFilterIdx] = filter_index;
//                filter_index += port->filter_count;
//                if (port->filter_name) {
//                    port_spec[port_idx][PortSpecFilterConstIdx] = -1;
//                    filter_name_ptr[port_idx] = port->filter_name;
//                }
//                else {
//                    memcpy(const_vector + const_index, port->filter_values,
//                            sizeof(*const_vector) * port->filter_count);
//                    port_spec[port_idx][PortSpecFilterConstIdx] =
//                        const_index;
//                    const_index += port->filter_count;
//                }
//            }
//
//            pdo_full_scale[port_idx] = port->pdo_full_scale;
//        }
//
//        gain_str   = createStrVect(  gain_name_ptr, io_port_count);
//        offset_str = createStrVect(offset_name_ptr, io_port_count);
//        filter_str = createStrVect(filter_name_ptr, io_port_count);
//
//
//        if (!ssWriteRTW2dMatParam(S, "IOPortSpec", port_spec,
//                    SS_INT32, PortSpecMax, io_port_count))
//            return;
//        if (!ssWriteRTWVectParam(S, "PDOFullScale",
//                    pdo_full_scale, SS_DOUBLE, io_port_count))
//            return;
//        if (!ssWriteRTWStrVectParam(S, "GainName", gain_str,
//                    io_port_count))
//            return;
//        if (!ssWriteRTWStrVectParam(S, "OffsetName", offset_str,
//                    io_port_count))
//            return;
//        if (!ssWriteRTWStrVectParam(S, "FilterName", filter_str,
//                    io_port_count))
//            return;
//        
//        mxFree(port_spec);  
//        mxFree(pdo_full_scale);
//        mxFree(gain_name_ptr);  
//        mxFree(offset_name_ptr);
//        mxFree(filter_name_ptr);
//    }

    if (!ssWriteRTWScalarParam(S,
                "FilterCount", &slave->filter_count, SS_UINT32))
        return;

    if (!ssWriteRTWVectParam(S, "ConstantValues",
                const_vector, SS_DOUBLE, slave->constant_count))
        return;

    if (!ssWriteRTWWorkVect(S, "IWork", 1, "BitOffset", slave->iwork_count))
        return;
    if (!ssWriteRTWWorkVect(S, "PWork", 1, "DataPtr", slave->pwork_count))
        return;
                
    mxFree(const_vector);
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
