/*
 * $Id$
 *
 * This SFunction implements a generic EtherCAT Slave.
 *
 * Copyright 2012, Richard Hacker
 * License: GPLv3+
 *
 * Description:
 * This generic SFunction can be used to realise any EtherCAT slave that
 * is used in EtherLab. All the necessary information needed
 * to configure the slave, all the outputs and inputs, scaling,
 * filtering, etc. is specified via the parameters passed to the SFunction.
 *
 * Slave address:
 * This is a struct with the fields:
 *      master:
 *      domain:
 *      alias:
 *      position:
 *
 * Slave configuration (SLAVE_CONFIG):
 * This is a structure with the fields:
 *      vendor: Vendor Id
 *      product: Product Code
 *      description: (optional) Description string
 *
 *      sdo: SDO configuration; OPTIONAL; CellArray{n,4} or Real(n,4)
 *          { Index, SubIndex, BitLen, Value;
 *            Index, SubIndex,      0, [Byte, Byte, ...];
 *            Index, SubIndex,      0, 'string';
 *            Index,       -1,      0, [Byte, Byte, ...] }
 *
 *          Value and Byte must be double-typed.
 *
 *          Whenever arrays are specified, BitLen must be 0
 *          Example:
 *          Row 1 configures a single value
 *                BitLen = one of  8,  16,  32,  64: uintX_t
 *                                -8, -16, -32, -64: intX_t
 *                                             0.32: single
 *                                             0.64: double
 *          Row 2 configures a variable array intepreted as uint8
 *          Row 3 configures a variable array intepreted as string
 *          Row 4 configures a variable array using complete access
 *
 *
 *          If SDO is an nx4 matrix of reals, only values of type
 *          as in Row 1 can be specified.
 *
 *      soe: SOE configuration; OPTIONAL; CellArray of vectors
 *          { Index, [Value, Value, ...];...
 *            Index, 'string'}
 *          Row 1 configures a variable array of uint8
 *          Row 2 configures a string array
 *
 *      dc: Distributed Clocks Configuration: OPTIONAL; Vector[10] | Value
 *          Value:   Single value AssignActivate
 *          Vector[10]: [ AssignActivate, ...
 *                   CycleTimeSync0, CycleTimeSync0Factor, ...
 *                   ShiftTimeSync0, ShiftTimeSync0Factor,
 *                   ShiftTimeSync0Input, ...
 *                   CycleTimeSync1, CycleTimeSync1Factor, ...
 *                   ShiftTimeSync1, ShiftTimeSync1Factor]
 *
 *          Setting AssignActivate to zero disables DC
 *
 *      sm: Optional slave SyncManager definition. This definition has 3 level
 *                  of indirections Sm <- Pdos <- Entries
 *
 *          sm:        {SmDef*}
 *          SmDef:     {SmIndex, SmDir, Pdos}
 *          SmIndex:    Index of SyncManager
 *          SmDir:      SyncManager direction, as seen by the Master:
 *                      0 => Output (RxPdo), 1 => Input (TxPdo)
 *
 *          Pdos:       {Pdo*}
 *          Pdo:        {PdoIndex, Entries}
 *          PdoIndex:   Index of Pdo
 *
 *          Entries:    [Entry*]        (Nx3 array)
 *          Entry:      [EntryIndex, EntrySubIndex, BitLen]
 *          EntryIndex: Index of Pdo Entry (If 0, only BitLen is considered)
 *          EntrySubIndex: SubIndex of Pdo Entry ( >= 0 )
 *          BitLen:     number of bits (>0)
 *
 * PARAMETER_CONFIG: Vector structure with fields:
 *      .name = char
 *          Mandatory parameter name.
 *
 *      .value = vector
 *          Parameter value
 *
 *      .mask_index = idx
 *          Index of a S-Function mask parameter. In this case, the value
 *          will be taken from there and .value above is ignored.
 *          This is very powerful, as it allows model wide parameters to be
 *          used.
 *
 * PORT_CONFIG:      Vector structure with fields
 *      .output := outputspec*     Block outputs; The number of elements
 *                                  correspond to the number of ports
 *      .input  := inputspec*      Block inputs; The number of elements
 *                                  correspond to the number of ports
 *
 *      outputspec := Structure with the fields
 *          .gain = ParamSpec
 *          .offset = ParamSpec
 *          .filter = ParamSpec
 *          .full_scale = This value is used to normalize an integer to 1.0
 *                        e.g. .full_scale = 32768.0 for int16_T
 *
 *                        output = filter((PDO / full_scale) * gain + offset)
 *
 *          .pdo = PdoSpec
 *          .big_endian = True for big endian data type
 *          .pdo_data_type = specifies the data type of the PDO.
 *              The data type can be specified using:
 *              1) Matlab data types
 *                      * uint(n): Unsigned integer with n bits,
 *                                 n E [1..8, 16, 24, 32, 40, 48, 56, 64]
 *                      * sint(n): Signed integer with n bits
 *                                 n E [8, 16, 32, 64]
 *                      * float('single') or float('double')
 *              2) String specification:
 *                      'Boolean',
 *                      'Bit2', 'Bit3', 'Bit4', 'Bit5', 'Bit6', 'Bit7',
 *                      'Unsigned8',  'Unsigned16', 'Unsigned24', 'Unsigned32',
 *                      'Unsigned40', 'Unsigned48', 'Unsigned56', 'Unsigned64',
 *                      'Integer8',   'Integer16',  'Integer32',  'Integer64',
 *                      'Real32', 'Real64'
 *              3) Numeric value (deprecated, for backward compatability)
 *                      1001: Boolean
 *                      1002: Bit2
 *                      1003: Bit3
 *                      1004: Bit4
 *                      1005: Bit5
 *                      1006: Bit6
 *                      1007: Bit7
 *                      1008: Unsigned8
 *                      1016: Unsigned16
 *                      1024: Unsigned24
 *                      1032: Unsigned32
 *                      1040: Unsigned40
 *                      1048: Unsigned48
 *                      1056: Unsigned56
 *                      1064: Unsigned64
 *
 *                      2008: Integer8
 *                      2016: Integer16
 *                      2032: Integer32
 *                      2064: Integer64
 *
 *                      3032: Real32
 *                      3064: Real64
 *
 *      ParamSpec  :=  []              Empty, do nothing
 *                   | vector          Constant anonymous real_T value
 *
 *                   | {'Name', vector}
 *                   | struct('name', name,
 *                            'value', value)
 *                       Named value, will be a parameter
 *
 *                   | struct('parameter', idx)
 *                       Value references a mask parameter as a vector
 *
 *                   | struct('parameter', idx,
 *                            'element', idx)
 *                       Value references a mask parameter as a scalar
 *
 *                   The vector can have none, 1 or the same
 *                   number of elements as there are pdo's
 *
 *      PdoSpec   := [SmIdx, PdoIdx, PdoEntryIdx, DataIdx]  (nx4 Matrix)
 *
 *      SmIdx, PdoIdx, PdoEntryIdx := Zero based index into the
 *                                    slave configuration
 *      DataIdx := index of the PdoEntry value, e.g. DataIdx = 3
 *                 means take the fourth bit out of the bit vector
 *                 when PdoEntry has BitLen = 64 and
 *                 PdoEntryDataType = boolean
 *                 Note that the data types must match
 *
 *      inputspec := same as outputspec, except for filter
 *                      In this case,
 *                      PDO = (input * gain + offset) * full_scale
 */

#define S_FUNCTION_NAME  ec_slave3
#define S_FUNCTION_LEVEL 2

#include "simstruc.h"

#include <math.h>
#include <ctype.h>

#ifdef _WIN32
#define __attribute__(x)
#endif

#define ADDRESS         0
#define SLAVE_CONFIG    1
#define PORT_CONFIG     2
#define DEBUG           3
#define TSAMPLE         4
#define PARAM_COUNT     5

/* New version of ec_slave3 accept a parameter configuration that
 * must be specified after PARAM_COUNT. The first element is
 * parameter specification, the rest are runtime parameters. */
#define PARAMETER_CONFIG 5

static const struct datatype_info {
    uint_T id;
    char *name;
    uint_T is_generic;
    int_T mant_bits;
    DTypeId sl_type;
} datatype_info[] = {
    { 1001, "Boolean",    0, 1,  SS_BOOLEAN }, /*  0 */
    { 1002, "Bit2",       0, 2,  SS_UINT8   }, /*  1 */
    { 1003, "Bit3",       0, 3,  SS_UINT8   }, /*  2 */
    { 1004, "Bit4",       0, 4,  SS_UINT8   }, /*  3 */
    { 1005, "Bit5",       0, 5,  SS_UINT8   }, /*  4 */
    { 1006, "Bit6",       0, 6,  SS_UINT8   }, /*  5 */
    { 1007, "Bit7",       0, 7,  SS_UINT8   }, /*  6 */
    { 1008, "Unsigned8",  1, 8,  SS_UINT8   }, /*  7 */
    { 1016, "Unsigned16", 1, 16, SS_UINT16  }, /*  8 */
    { 1024, "Unsigned24", 0, 24, SS_UINT32  }, /*  9 */
    { 1032, "Unsigned32", 1, 32, SS_UINT32  }, /* 10 */
    { 1040, "Unsigned40", 0, 40, SS_DOUBLE  }, /* 11 */
    { 1048, "Unsigned48", 0, 48, SS_DOUBLE  }, /* 12 */
    { 1056, "Unsigned56", 0, 56, SS_DOUBLE  }, /* 13 */
    { 1064, "Unsigned64", 1, 64, SS_DOUBLE  }, /* 14 */

    { 2008, "Integer8",   1, 8,  SS_INT8    }, /* 15 */
    { 2016, "Integer16",  1, 16, SS_INT16   }, /* 16 */
    { 2032, "Integer32",  1, 32, SS_INT32   }, /* 17 */
    { 2064, "Integer64",  1, 64, SS_DOUBLE  }, /* 18 */

    { 3032, "Real32",     1, 32, SS_SINGLE  }, /* 19 */
    { 3064, "Real64",     1, 64, SS_DOUBLE  }, /* 20 */

    {    0,                                 },
};

static const struct datatype_info *type_bool   = &datatype_info[0];
static const struct datatype_info *type_uint8  = &datatype_info[7];
static const struct datatype_info *type_uint16 = &datatype_info[8];
static const struct datatype_info *type_uint32 = &datatype_info[10];
static const struct datatype_info *type_uint64 = &datatype_info[14];
static const struct datatype_info *type_sint8  = &datatype_info[15];
static const struct datatype_info *type_sint16 = &datatype_info[16];
static const struct datatype_info *type_sint32 = &datatype_info[17];
static const struct datatype_info *type_sint64 = &datatype_info[18];
static const struct datatype_info *type_single = &datatype_info[19];
static const struct datatype_info *type_double = &datatype_info[20];

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
    uint32_T product_code;
    char_T *type;

    struct sdo_config {
        uint16_T index;
        int16_T  subindex; /* may be negative to indicate complete access */

        /* Data type. One of SS_UINT8, SS_UINT16, SS_UINT32 */
        const struct datatype_info *datatype;

        /* Configuration value or array length if it is used */
        real_T value;
        uint8_T *byte_array;

    } *sdo_config, *sdo_config_end;

    /* Sercos over EtherCat configuration for a slave */
    struct soe_config {
        uint16_T index;
        uint8_T *octet_string;
        size_t octet_string_len;
    } *soe_config, *soe_config_end;

    /* This value is needed right at the end when the values have
     * to be written by mdlRTW */
    uint_T pdo_entry_count;
    uint_T pdo_count;

    struct sync_manager {
        uint16_T index;

        /* Direction as seen by the master */
        enum sm_direction {EC_SM_NONE = 0,
            EC_SM_INPUT, EC_SM_OUTPUT, EC_SM_MAX} direction;

        /* Description of the slave's Pdo's */
        struct pdo {
            uint16_T index;

            /* PDO Entries */
            struct pdo_entry {
                uint16_T index;
                uint8_T subindex;
                uint_T bitlen;
            } *entry, *entry_end;
        } *pdo, *pdo_end;

    } *sync_manager, *sync_manager_end;

    /* Distributed clocks */
    struct dc_opmode {
        uint16_T assign_activate;

        boolean_T shift_time_sync0_input;

        /* CycleTimeSync0Factor, ShiftTimeSync0Factor,
         * CycleTimeSync1Factor, ShiftTimeSync1Factor */
        int32_T  factor[4];

        /* CycleTimeSync0, ShiftTimeSync0,
         * CycleTimeSync1, ShiftTimeSync1*/
        int32_T  value[4];
    } dc_opmode;

    /* Structure for setting a port parameter */
    struct parameter {
        char_T *name;
        int32_T count;
        real_T *value;
        int_T mask_idx;
    } *parameter, *parameter_end;

    struct io_port {
        /* Structure for setting a port parameter */
        struct param_spec {
            /* Parameter structure. This may be local or it can
             * point to a structure in slave->parameter, seee is_ref */
            struct parameter* ptr;

            /* ptr references a value in slave->parameter */
            boolean_T is_ref;

            /* Starting index of parameter in ptr; -1 for all */
            int32_T element_idx;
        } gain, offset, filter;

        struct port_pdo {
            const struct pdo_entry *entry;
            size_t element_idx;
        } *pdo, *pdo_end;

        int32_T  dwork_idx;

        real_T fullscale;

        /* Data type how to interpret PDO */
        const struct datatype_info *data_type;
        boolean_T big_endian;

        /* Simulink port data type */
        DTypeId sl_port_data_type;
    } *o_port, *o_port_end, *i_port, *i_port_end;

    /* Runtime parameters are used to store parameters where a name was
     * supplied for gain_name, offset_name and filter_name. These
     * parameters are exported in the C-API */
    uint_T runtime_param_count;

    int_T filter_count;
    int_T dwork_count;
};

static char_T msg[512];

/***************************************************************************/
/***************************************************************************/
#ifdef _WIN32
int vsnprintf(char *str, size_t size, const char *format, va_list ap)
{
     int rvalue;
     char _str[1000];

     rvalue = vsprintf(_str, format, ap);

     if ((rvalue >= 0) && (rvalue < size))
       strcpy(str,_str);
     else {
       mexPrintf("SNPRINTF BUG negative\n");
       *str = 0;
     }
     return rvalue;
}

/***************************************************************************/
int snprintf(char *str, size_t size, const char *format, ...)
{
     va_list ap;
     int rvalue;

     va_start(ap,format);
     rvalue = vsnprintf(str, size, format, ap);
     va_end(ap);

     return rvalue;
}

#endif


/***************************************************************************/
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

/***************************************************************************/
static void __attribute__((format (__printf__, 5, 6)))
pr_info(const struct ecat_slave *slave,
        const char_T *parent_context, const char_T *context,
        unsigned int indent, const char_T *fmt, ...)
{
    va_list ap;
    char_T message[200];

    va_start(ap, fmt);
    intro(slave, indent, parent_context, context);
    vsnprintf(message, sizeof(message), fmt, ap);
    mexPrintf("%s", message);
    va_end(ap);
}

/***************************************************************************/
static void __attribute__((format (__printf__, 5, 6)))
pr_debug(const struct ecat_slave *slave,
        const char_T *parent_context, const char_T *context,
        unsigned int indent, const char_T *fmt, ...)
{
    va_list ap;
    char_T message[200];

    if (slave->debug_level < 2)
        return;

    va_start(ap, fmt);
    intro(slave, indent, parent_context, context);
    vsnprintf(message, sizeof(message), fmt, ap);
    mexPrintf("%s", message);
    va_end(ap);
}

static void __attribute__((format (__printf__, 5, 6)))
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

static void __attribute__((format (__printf__, 5, 6)))
pr_warn(const struct ecat_slave *slave,
        const char_T *parent_context, const char_T *context,
        unsigned int line_no, const char_T *fmt, ...)
{
    va_list ap;
    char_T message[200];

    va_start(ap, fmt);
    mexPrintf( (slave->debug_level < 2
                ? ("WARNING for EtherCAT slave '%s', "
                    "Config variable: %s.%s\n    ")
                : ("WARNING for EtherCAT slave '%s', "
                    "Config variable: %s.%s (Line %u)\n    ")),
            ssGetPath(slave->S), parent_context, context, line_no);
    vsnprintf(message, sizeof(message), fmt, ap);
    mexPrintf("%s", message);
    va_end(ap);
}


/***************************************************************************/
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

/***************************************************************************/
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

    if (!dflt) {
        pr_error(slave, p_ctxt, field_name, line_no,
                "String is not allowed to be empty ('')");
        return -1;
    }

    len = strlen(dflt) + 1;
    CHECK_CALLOC(slave->S, len, 1, *dest);
    strcpy(*dest, dflt);
    return 0;
}

/****************************************************************************/
static int_T
get_numeric_scalar(struct ecat_slave *slave, const char_T *p_ctxt,
        uint_T line, uint_T idx, const mxArray *cell, real_T *val)
{
    const mxArray *src = mxGetCell(cell, idx);

    if (mxIsEmpty(src) || !(mxIsNumeric(src) || mxIsLogical(src))) {
        char_T ctxt[10];
        snprintf (ctxt, sizeof(ctxt), "{%u}", idx+1);
        pr_error(slave, p_ctxt, ctxt, line, "Expected numeric value");
        return -1;
    }

    *val = mxGetScalar(src);
    return 0;
}

#define RETURN_ON_ERROR(val)    \
    do {                        \
        int_T err = (val);        \
        if (err < 0)            \
            return err;         \
    } while(0)

/***************************************************************************/
static int_T
get_slave_info(struct ecat_slave *slave)
{
    const char_T *ctxt = "ADDRESS";
    const mxArray *address       = ssGetSFcnParam(slave->S, ADDRESS);
    real_T val;

    pr_debug(slave, NULL, NULL, 0,
            "--------------- Slave Address -----------------\n");
    RETURN_ON_ERROR(get_numeric_field(slave, ctxt, __LINE__, address,
                0, 1, 0, 0, "master", &val));
    slave->master = val;

    RETURN_ON_ERROR(get_numeric_field(slave, ctxt, __LINE__, address,
                0, 1, 0, 0, "domain", &val));
    slave->domain = val;

    RETURN_ON_ERROR(get_numeric_field(slave, ctxt, __LINE__, address,
                0, 1, 0, 0, "alias", &val));
    slave->alias = val;

    RETURN_ON_ERROR(get_numeric_field(slave, ctxt, __LINE__, address,
                0, 1, 0, 0, "position", &val));
    slave->position = val;

    pr_debug(slave, NULL, "", 1,
            "Master %u, Domain %u, Alias %u, Position %u\n",
            slave->master, slave->domain, slave->alias, slave->position);

    return 0;
}

/***************************************************************************/
static int_T
get_sync_manager_pdo(struct ecat_slave *slave, struct pdo *pdo,
        const char_T *p_ctxt, const mxArray *pdo_def)
{
    char_T ctxt[50];
    const mxArray *pdo_cell;
    uint_T entry_count, i;
    const real_T *pval;
    real_T val;

    if (!mxIsCell(pdo_def) || mxGetNumberOfElements(pdo_def) != 2) {
        pr_error(slave, p_ctxt, NULL, __LINE__,
                "PDO configuration is not a cell array with 2 elements");
        return -1;
    }

    /* Index */
    RETURN_ON_ERROR (get_numeric_scalar(slave, p_ctxt, __LINE__,
                0, pdo_def, &val));
    pdo->index = val;

    /* Pdo Entry */
    pdo_cell = mxGetCell(pdo_def, 1);
    if (!pdo_cell || !(entry_count = mxGetM(pdo_cell)))
        return 0;

    /* Check that Pdo Entry is a Nx3 array */
    if (mxGetN(pdo_cell) != 3
            || !mxIsDouble(pdo_cell) || !(pval = mxGetPr(pdo_cell))) {
        snprintf(ctxt, sizeof(ctxt), "%s{2}", p_ctxt);
        pr_error(slave, ctxt, NULL, __LINE__,
                "Value is not a Nx3 numeric array");
        return -1;
    }

    CHECK_CALLOC(slave->S, entry_count,
            sizeof(struct pdo_entry), pdo->entry);
    pdo->entry_end = pdo->entry + entry_count;

    for (i = 0; i < entry_count; i++) {
        struct pdo_entry *pdo_entry = pdo->entry + i;

        pdo_entry->index = pval[i];
        pdo_entry->subindex = pval[i + entry_count];
        pdo_entry->bitlen = pval[i + 2*entry_count];
    }
    slave->pdo_entry_count += entry_count;

    return 0;
}

/***************************************************************************/
static int_T
get_sync_manager(struct ecat_slave *slave, struct sync_manager *sm,
        const char_T *p_ctxt, const mxArray *sync_manager_def)
{
    char_T ctxt[50];
    const mxArray *sm_cell;
    uint_T pdo_count, i;
    real_T val;

    if (!mxIsCell(sync_manager_def)
            || mxGetNumberOfElements(sync_manager_def) != 3) {
        pr_error(slave, p_ctxt, NULL, __LINE__,
                "SyncManager configuration is not a\n"
                "cell array with 3 elements");
        return -1;
    }

    /* Index */
    RETURN_ON_ERROR (get_numeric_scalar(slave, p_ctxt, __LINE__,
                0, sync_manager_def, &val));
    sm->index = val;

    /* Direction */
    RETURN_ON_ERROR (get_numeric_scalar(slave, p_ctxt, __LINE__,
                1, sync_manager_def, &val));
    sm->direction = val ? EC_SM_INPUT : EC_SM_OUTPUT;

    /* Pdo definition */
    sm_cell = mxGetCell(sync_manager_def, 2);
    if (!sm_cell || !mxGetNumberOfElements(sm_cell)
            || !(pdo_count = mxGetNumberOfElements(sm_cell)))
        return 0;

    CHECK_CALLOC(slave->S, pdo_count, sizeof(struct pdo), sm->pdo);
    sm->pdo_end = sm->pdo + pdo_count;

    for (i = 0; i < pdo_count; i++) {

        snprintf(ctxt, sizeof(ctxt), "%s{3}{%u}", p_ctxt, i + 1);

        if (get_sync_manager_pdo(slave,
                    sm->pdo + i, ctxt, mxGetCell(sm_cell, i)))
            return -1;
    }
    slave->pdo_count += pdo_count;

    return 0;
}

/****************************************************************************/
static const struct datatype_info *
get_data_type_from_real(real_T type_id)
{
    const struct datatype_info *dt;
    uint_T dt_id = type_id;

    for (dt = datatype_info; dt->id; ++dt) {
        if (dt->id == dt_id)
            return dt;
    }

    dt_id = type_id*100;
    switch (dt_id) {
        case    32: return type_single;
        case    64: return type_double;
        case   100:
        case   200:
        case   300:
        case   400:
        case   500:
        case   600:
        case   700:
        case   800: return &datatype_info[dt_id/100 - 1];
        case  1600: return type_uint16;
        case  2400: return &datatype_info[9];
        case  3200: return type_uint32;
        case  4000: return &datatype_info[11];
        case  4800: return &datatype_info[12];
        case  5600: return &datatype_info[13];
        case  6400: return type_uint64;
        case  -800: return type_sint8;
        case -1600: return type_sint16;
        case -3200: return type_sint32;
        case -6400: return type_sint64;
    }

    return NULL;
}

/****************************************************************************/
static const struct datatype_info *
get_data_type(struct ecat_slave *slave,
        const char_T *p_ctxt, const char_T *ctxt, int_T line,
        const mxArray *spec)
{
    const struct datatype_info *dt = NULL;

    if (!spec || !mxGetNumberOfElements(spec))
        return NULL;

    if (mxIsDouble(spec)) {
        real_T dt_spec = *mxGetPr(spec);

        if (!dt_spec)
            return NULL;

        dt = get_data_type_from_real(dt_spec);
        if (!dt)
            pr_error(slave, p_ctxt, ctxt, line,
                    "Invalid data type %g", dt_spec);
    }
    else if (mxIsChar(spec)) {
        char_T name[20];

        if (mxGetString(spec, name, sizeof(name))) {
            pr_error(slave, p_ctxt, ctxt, line, "Invalid string data type");
            return NULL;
        }

        for (dt = datatype_info; dt->id; ++dt) {
            if (!strcmp(name, dt->name))
                return dt;
        }
        dt = NULL;

        pr_error(slave, p_ctxt, ctxt, line, "Invalid data type %s", name);
    }
    else if (mxIsStruct(spec)) {
        const mxArray *class = mxGetField(spec, 0, "Class");
        char_T class_name[20];

        if (!class || !mxIsChar(class) || !mxGetNumberOfElements(class)
                || mxGetString(class, class_name, sizeof(class_name))) {
            pr_error(slave, p_ctxt, ctxt, line,
                    "Invalid class name in data type!");
            return NULL;
        }

        if (!strcmp(class_name, "DOUBLE"))
            return type_double;
        else if (!strcmp(class_name, "SINGLE"))
            return type_single;
        else if (!strcmp(class_name, "INT")) {
            const mxArray *is_signed = mxGetField(spec, 0, "IsSigned");
            const mxArray *mant_bits = mxGetField(spec, 0, "MantBits");
            const real_T *val;
            real_T dt_id;

            if (!is_signed || !mxIsDouble(is_signed)
                    || !mxGetNumberOfElements(is_signed)
                    || !(val = mxGetPr(is_signed))) {
                pr_error(slave, p_ctxt, ctxt, line,
                        "Invalid field 'IsSigned' in data type!");
                return NULL;
            }

            dt_id = *val ? 2000.0 : 1000.0;

            if (!mant_bits || !mxIsDouble(mant_bits)
                    || !mxGetNumberOfElements(mant_bits)
                    || !(val = mxGetPr(mant_bits))) {
                pr_error(slave, p_ctxt, ctxt, line,
                        "Invalid field 'MantBits' in data type!");
                return NULL;
            }
            dt_id += *val;

            dt = get_data_type_from_real(dt_id);
            if (!dt)
                pr_error(slave, p_ctxt, ctxt, line,
                        "Invalid MantBits=%i in INT", (int_T)*val);
        }
    }
    else if (mxIsClass(spec, "Simulink.NumericType")) {
        const mxArray *dtMode = mxGetProperty(spec, 0, "DataTypeMode");
        char_T className[64];

        if (!dtMode || !mxIsChar(dtMode) || !mxGetNumberOfElements(dtMode)
                || mxGetNumberOfElements(dtMode) >= sizeof(className)
                || mxGetString(dtMode, className, sizeof(className))) {
            pr_error(slave, p_ctxt, ctxt, line,
                    "Invalid field 'DataTypeMode' in data type!");
            return NULL;
        }

        if (!strcmp(className, "Double")) {
            return type_double;
        }
        else if (!strcmp(className, "Single")) {
            return type_single;
        }
        else if (!strcmp(className, "Fixed-point: binary point scaling")) {
            const mxArray *signedness = mxGetProperty(spec, 0, "Signedness");
            const mxArray *wordLength = mxGetProperty(spec, 0, "WordLength");
            const real_T *bitLength;
            char_T sign[20];
            real_T dtId;

            if (!signedness || !mxIsChar(signedness)
                    || !mxGetNumberOfElements(signedness)
                    || mxGetNumberOfElements(signedness) >= sizeof(sign)
                    || mxGetString(signedness, sign, sizeof(sign))) {
                pr_error(slave, p_ctxt, ctxt, line,
                        "Invalid field 'Signedness' in data type!");
                return NULL;
            }

            if (!strcmp(sign, "Unsigned")) {
                dtId = 1000.0;
            }
            else if (!strcmp(sign, "Signed")) {
                dtId = 2000.0;
            }
            else {
                pr_error(slave, p_ctxt, ctxt, line,
                        "Invalid value '%s' for 'Signedness' in data type!",
                        sign);
                return NULL;
            }

            if (!wordLength || !mxIsDouble(wordLength)
                    || !mxGetNumberOfElements(wordLength)
                    || !(bitLength = mxGetPr(wordLength))) {
                pr_error(slave, p_ctxt, ctxt, line,
                        "Invalid field 'WordLength' in data type!");
                return NULL;
            }

            dtId += *bitLength;

            dt = get_data_type_from_real(dtId);
            if (!dt)
                pr_error(slave, p_ctxt, ctxt, line,
                        "Word length '%lf' not supported in data type!",
                        *bitLength);
        }
    }

    return dt;
}

/****************************************************************************/
static int_T
get_slave_sdo_array(struct ecat_slave *slave, const real_T* pval,
        const char_T *context, size_t rows)
{
    size_t i;

    for (i = 0; i < rows; i++) {
        struct sdo_config *sdo_config = slave->sdo_config_end;
        char_T ctxt[50];

        snprintf(ctxt, sizeof(ctxt), "sdo{Row=%zu}", i+1);

        /* Index */
        sdo_config->index = pval[i];

        /* SubIndex */
        sdo_config->subindex = pval[i + rows];
        if (sdo_config->subindex < 0 || sdo_config->subindex > 255) {
            pr_error(slave, context, ctxt, __LINE__,
                    "SDO SubIndex is out of range [0..255]");
            return -1;
        }

        /* BitLen */
        sdo_config->datatype = get_data_type_from_real(pval[i + 2*rows]);
        if (!sdo_config->datatype || !sdo_config->datatype->is_generic) {
            pr_error(slave, context, ctxt, __LINE__,
                    "Unknown data type %i", (int_T)pval[i + 2*rows]);
            return -1;
        }

        /* Value */
        sdo_config->value = pval[i + 3*rows];

        slave->sdo_config_end++;
    }

    return 0;
}

/****************************************************************************/
static int_T
get_slave_sdo_cell(struct ecat_slave *slave, const mxArray* array,
        const char_T *context, size_t rows)
{
    size_t i, j;
    real_T val;

    for (i = 0; i < rows; i++) {
        struct sdo_config *sdo_config = slave->sdo_config_end;
        const mxArray *valueCell;
        char_T ctxt[50];
        real_T *pval;
        size_t nelem;

        snprintf(ctxt, sizeof(ctxt), "%s.sdo{Row=%zu}", context, i+1);

        /* Index */
        RETURN_ON_ERROR (get_numeric_scalar(slave, ctxt, __LINE__,
                    i, array, &val));
        sdo_config->index = val;

        /* SubIndex */
        RETURN_ON_ERROR (get_numeric_scalar(slave, ctxt, __LINE__,
                    i + rows, array, &val));
        sdo_config->subindex = val;
        if (sdo_config->subindex < -1 || sdo_config->subindex > 255) {
            pr_error(slave, ctxt, NULL, __LINE__,
                    "SDO SubIndex is out of range [-1..255]");
            return -1;
        }

        /* BitLen */
        sdo_config->datatype = get_data_type(slave,
                ctxt, NULL, __LINE__, mxGetCell(array, i + 2*rows));
        if (ssGetErrorStatus(slave->S))
            return -1;

        if (sdo_config->datatype && !sdo_config->datatype->is_generic) {
            pr_error(slave, ctxt, NULL, __LINE__,
                    "Only generic data types are allowed");
            return -1;
        }

        /* Value */
        valueCell = mxGetCell(array, i + 3*rows);
        if (!valueCell || !(nelem = mxGetNumberOfElements(valueCell))) {
            pr_error(slave, ctxt, NULL, __LINE__,
                    "SDO value is corrupt, not double or empty");
            return -1;
        }

        pval = mxIsDouble(valueCell) ? mxGetPr(valueCell) : NULL;

        if (sdo_config->datatype
                && (sdo_config->datatype != type_uint8
                    || (pval && nelem == 1))) {

            /* Single value */
            if (nelem > 1) {
                pr_error(slave, ctxt, NULL, __LINE__,
                        "SDO BitLen must be zero when using value array");
                return -1;
            }

            if (sdo_config->subindex < 0) {
                pr_error(slave, ctxt, NULL, __LINE__,
                        "SDO SubIndex must be >= 0");
                return -1;
            }

            if (!pval) {
                pr_error(slave, ctxt, NULL, __LINE__,
                        "SDO value is corrupt, not double nor empty");
                return -1;
            }

            sdo_config->value = *pval;

            if ((sdo_config->datatype == type_uint8
                        || sdo_config->datatype == type_uint16
                        || sdo_config->datatype == type_uint32
                        || sdo_config->datatype == type_uint64)
                    && sdo_config->value < 0.0) {
                pr_warn(slave, ctxt, NULL, __LINE__,
                        "Negative value %i for unsigned integer\n",
                        (int_T)sdo_config->value);
            }
        }
        else {

            /* SDO value is an array */

            sdo_config->datatype = type_uint8;
            sdo_config->value = nelem; /* Value is misused as number of
                                          elements */

            /* one more than necessary, in case it is a string */
            CHECK_CALLOC(slave->S, nelem+1, 1, sdo_config->byte_array);

            if (pval) {
                /* Array of real */
                for (j = 0; j < nelem; j++)
                    sdo_config->byte_array[j] = *pval++;
            } else if (mxIsChar(valueCell)) {
                /* String */
                if (mxGetString(valueCell,
                            (char_T*)sdo_config->byte_array, nelem+1)) {
                    pr_error(slave, ctxt, NULL, __LINE__,
                            "SDO string is corrupt");
                    return -1;
                }
            } else {
                pr_error(slave, ctxt, NULL, __LINE__,
                        "SDO is neither string nor array");
                return -1;
            }
        }

        slave->sdo_config_end++;
    }

    return 0;
}

/****************************************************************************/
static int_T
get_slave_sdo(struct ecat_slave *slave, const mxArray* array,
        const char_T *context)
{
    size_t rows, i, j;

    if (!array || !(rows = mxGetM(array)))
        return 0;

    if (!(mxIsDouble(array) || mxIsCell(array)) || mxGetN(array) != 4) {
        pr_error(slave, context, "sdo", __LINE__,
                "SDO configuration is not a Mx4 cell/numeric array");
        return -1;
    }

    CHECK_CALLOC(slave->S, rows,
            sizeof(struct sdo_config), slave->sdo_config);
    slave->sdo_config_end = slave->sdo_config;

    if (mxIsDouble(array)) {
        if (get_slave_sdo_array(slave, mxGetPr(array), context, rows))
            return -1;
    }
    else {
        if (get_slave_sdo_cell(slave, array, context, rows))
            return -1;
    }

    rows = slave->sdo_config_end - slave->sdo_config;
    pr_debug(slave, NULL, "", 1, "SDO count %zu\n", rows);

    for (i  = 0; i < rows; i++) {
        if (slave->sdo_config[i].byte_array) {
            pr_debug(slave, NULL, "", 2,
                    (slave->sdo_config[i].subindex < 0
                     ?  "#x%04X:-1 ["
                     : "#x%04X:%02x ["),
                    slave->sdo_config[i].index,
                    slave->sdo_config[i].subindex);
            for (j = 0; j < slave->sdo_config[i].value; j++)
                pr_debug(slave, NULL, NULL, 0,
                        "%02x,", slave->sdo_config[i].byte_array[j]);
            pr_debug(slave, NULL, NULL, 0, "] (Unsigned8)\n");
        }
        else {
            pr_debug(slave, NULL, "", 2,
                    "#x%04X:%02x %g (%s)\n",
                    slave->sdo_config[i].index,
                    slave->sdo_config[i].subindex,
                    slave->sdo_config[i].value,
                    slave->sdo_config[i].datatype->name);
        }
    }

    return 0;
}

/****************************************************************************/
static int_T
get_slave_soe(struct ecat_slave *slave, const mxArray* array,
        const char_T *context)
{
    const mxArray *valueCell;
    size_t rows, i, j;
    real_T val;

    if (!array || !(rows = mxGetM(array)))
        return 0;

    if (!mxIsCell(array) || mxGetN(array) != 2) {
        pr_error(slave, context, "soe", __LINE__,
                "SoE configuration is not a Mx2 cell array");
        return -1;
    }

    CHECK_CALLOC(slave->S, rows,
            sizeof(struct soe_config), slave->soe_config);
    slave->soe_config_end = slave->soe_config;

    for (i = 0; i < rows; i++) {
        struct soe_config *soe_config = slave->soe_config_end;
        char_T value_ctxt[20];
        char_T ctxt[50];
        size_t n;

        snprintf(ctxt, sizeof(ctxt), "%s.soe{Row=%zu}", context, i+1);
        snprintf(value_ctxt, sizeof(value_ctxt), "soe{%zu,2}", i+1);

        /* Index */
        RETURN_ON_ERROR (get_numeric_scalar(slave, ctxt, __LINE__,
                    i, array, &val));
        soe_config->index = val;

        /* Value */
        valueCell = mxGetCell(array, i + rows);
        if (!valueCell
                || !(n = mxGetNumberOfElements(valueCell))) {

            pr_error(slave, context, value_ctxt, __LINE__,
                    "SoE value is corrupt or empty");
            return -1;
        }

        CHECK_CALLOC(slave->S, n, sizeof(uint8_T),
                soe_config->octet_string);

        if (mxIsChar(valueCell)) {
            if (!mxGetString(valueCell,
                        (char*)soe_config->octet_string, n)) {

                pr_error(slave, context, value_ctxt, __LINE__,
                        "SoE string value is invalid");
                return -1;
            }
            soe_config->octet_string_len = n - 1;
        }
        else if (mxIsDouble(valueCell)) {
            real_T *pval;

            if (!(pval = mxGetPr(valueCell))) {
                pr_error(slave, context, value_ctxt, __LINE__,
                        "SoE value not a valid number");
                return -1;
            }
            for (j = 0; j < n; j++)
                soe_config->octet_string[j] = pval[j];
            soe_config->octet_string_len = n;
        }
        else {
            pr_error(slave, context, value_ctxt, __LINE__,
                    "SoE value is neither a string nor numeric array");
            return -1;
        }

        slave->soe_config_end++;
    }

    rows = slave->soe_config_end - slave->soe_config;
    pr_debug(slave, NULL, "", 1, "SoE count %zu\n", rows);
    for (i  = 0; i < rows; i++) {
        pr_debug(slave, NULL, "", 2,
                "Index=#x%04X Value=", slave->soe_config[i].index);
        for (j = 0; j < slave->soe_config[i].octet_string_len; j++)
            pr_debug(slave, NULL, NULL, 0,
                    "%02x, ", slave->soe_config[i].octet_string[j]);
        pr_debug(slave, NULL, NULL, 0, "\n");
    }

    return 0;
}

/****************************************************************************/
static int_T
get_slave_dc(struct ecat_slave *slave, const mxArray* array,
        const char_T *context)
{
    const real_T *val;
    size_t count;

    if (!array || !(count = mxGetNumberOfElements(array)))
        return 0;

    if (!mxIsDouble(array)
            || (count != 1 && count != 10)
            || !(val = mxGetPr(array))) {
        pr_error(slave, context, "dc", __LINE__,
                "DC configuration is not a vector[10]");
        return -1;
    }

    slave->dc_opmode.assign_activate            = val[0];

    if (count > 1 && slave->dc_opmode.assign_activate) {
        slave->dc_opmode.shift_time_sync0_input     = val[5] != 0.0;

        slave->dc_opmode.value[0]                   = val[1];
        slave->dc_opmode.value[1]                   = val[3];
        slave->dc_opmode.value[2]                   = val[6];
        slave->dc_opmode.value[3]                   = val[8];

        slave->dc_opmode.factor[0]                  = val[2];
        slave->dc_opmode.factor[1]                  = val[4];
        slave->dc_opmode.factor[2]                  = val[7];
        slave->dc_opmode.factor[3]                  = val[9];
    }

    pr_debug(slave, NULL, "", 1,
            "DC AssignActivate=%u "
            "CycleTimeSync0=%i, Factor=%i; "
            "ShiftTimeSync0=%i, Factor=%i, "
            "Input=%i; "
            "CycleTimeSync1=%i, Factor=%i; "
            "ShiftTimeSync1=%i, Factor=%i\n",
            slave->dc_opmode.assign_activate,
            slave->dc_opmode.value[0],
            slave->dc_opmode.factor[0],
            slave->dc_opmode.value[1],
            slave->dc_opmode.factor[1],
            slave->dc_opmode.shift_time_sync0_input,
            slave->dc_opmode.value[2],
            slave->dc_opmode.factor[2],
            slave->dc_opmode.value[3],
            slave->dc_opmode.factor[3]);

    return 0;
}

/****************************************************************************/
static int_T
get_slave_config(struct ecat_slave *slave)
{
    const mxArray *slave_config = ssGetSFcnParam(slave->S, SLAVE_CONFIG);
    const mxArray *array;
    const char_T *context = "SLAVE_CONFIG";
    real_T val;
    size_t i;
    uint_T sm_count;

    if (!slave_config || !mxIsStruct(slave_config)
            || !mxGetNumberOfElements(slave_config))
        return 0;

    RETURN_ON_ERROR(get_numeric_field(slave, context, __LINE__, slave_config,
                0, 1, 0, 0, "vendor", &val));
    slave->vendor_id = val;

    RETURN_ON_ERROR(get_numeric_field(slave, context, __LINE__, slave_config,
                0, 1, 0, 0, "product", &val));
    slave->product_code = val;

    RETURN_ON_ERROR(get_string_field(slave, context, __LINE__, slave_config,
                0, "description", "Unspecified", &slave->type));

    pr_debug(slave, NULL, "", 1,
            "VendorId %u\n", slave->vendor_id);
    pr_debug(slave, NULL, "", 1,
            "ProductCode #x%08X, Type '%s'\n",
            slave->product_code, slave->type);

    /* SDO */
    RETURN_ON_ERROR (get_slave_sdo(slave,
                mxGetField(slave_config, 0, "sdo"), context));

    /* SoE */
    RETURN_ON_ERROR (get_slave_soe(slave,
                mxGetField(slave_config, 0, "soe"), context));

    /* DC */
    RETURN_ON_ERROR (get_slave_dc(slave,
                mxGetField(slave_config, 0, "dc"), context));


    /***********************
     * Get PDO's
     ***********************/
    if ((array = mxGetField( slave_config, 0, "sm"))
            && (sm_count = mxGetNumberOfElements(array))) {

        if (!mxIsCell(array)) {
            pr_error(slave, context, "sm", __LINE__,
                    "SyncManager configuration is not a Mx3 cell array");
            return -1;
        }

        CHECK_CALLOC(slave->S, sm_count,
                sizeof(struct sync_manager), slave->sync_manager);
        slave->sync_manager_end = slave->sync_manager + sm_count;

        for (i = 0; i < sm_count; i++) {
            char_T ctxt[30];
            snprintf(ctxt, sizeof(ctxt), "SLAVE_CONFIG.sm{%zu}", i + 1);
            if (get_sync_manager(slave, slave->sync_manager + i,
                        ctxt, mxGetCell(array, i)))
                return -1;
        }

        pr_debug(slave, NULL, "", 1, "SM count %u\n", sm_count);
        for (i  = 0; i < sm_count; i++) {
            struct sync_manager *sm = slave->sync_manager + i;
            const struct pdo *pdo;

            pr_debug(slave, NULL, "", 2,
                    "SMIndex=%u Dir=%s PdoCount=%zi\n",
                    sm->index,
                    (sm->direction == 
                     EC_SM_OUTPUT ? "OP (RxPdo)" : "IP (TxPdo)"),
                    sm->pdo_end - sm->pdo);

            for (pdo = sm->pdo; pdo != sm->pdo_end; pdo++) {
                const struct pdo_entry *entry;

                pr_debug(slave, NULL, "", 3,
                        "PdoIndex=#x%04X EntryCount=%zu\n",
                        pdo->index, pdo->entry_end - pdo->entry);

                for (entry = pdo->entry; entry != pdo->entry_end; entry++) {
                    pr_debug(slave, NULL, "", 4,
                            "Index=#x%04X:%02x BitLen=%2u\n",
                            entry->index,
                            entry->subindex,
                            entry->bitlen);
                }
            }
        }
    }

    return 0;
}

/****************************************************************************
 * parameter specification is of the form:
 *    []
 *    | [value]
 *    | { 'string', [value] }
 *    | { 'string', uint32_t(mask index) }
 * anything else is an error
 ****************************************************************************/
static int_T
get_parameter_config(struct ecat_slave *slave)
{
    const mxArray *spec = ssGetSFcnParam(slave->S, PARAMETER_CONFIG);
    char_T ctxt[30];
    const char_T *p_ctxt = "PARAMETER_CONFIG";
    int_T idx, count;

    if (!mxIsStruct(spec)
            || !(count = mxGetNumberOfElements(spec)))
        return 0;

    pr_debug(slave, NULL, NULL, 0,
            "------------Parsing global parameters ---------\n");

    CHECK_CALLOC(slave->S, count, sizeof(struct parameter), slave->parameter);
    slave->parameter_end = slave->parameter + count;

    for (idx = 0; idx < count; ++idx) {
        struct parameter* param = slave->parameter + idx;
        const mxArray *array;
        const char *name;
        int_T i;
        real_T val, *p_val;

        snprintf(ctxt, sizeof(ctxt), "%s(%i)", p_ctxt, idx+1);
        RETURN_ON_ERROR(get_string_field(slave, ctxt, __LINE__,
                spec, idx, "name", NULL, &param->name));

        name = "mask_index";
        RETURN_ON_ERROR((i = get_numeric_field(slave, ctxt, __LINE__,
                spec, idx, 1, 1, 0, name, &val)));
        if (!i) {
            param->mask_idx = val;

            if (param->mask_idx <= PARAMETER_CONFIG
                    || param->mask_idx >= ssGetNumSFcnParams(slave->S)) {
                pr_error(slave, ctxt, name, __LINE__,
                        "Index %i does not point to a "
                        "valid mask parameter.",
                        param->mask_idx);
                return -1;
            }
            ssSetSFcnParamTunable(slave->S, param->mask_idx, SS_PRM_TUNABLE);

            /* Make spec point to mask parameter */
            array = ssGetSFcnParam(slave->S, param->mask_idx);
        }
        else {
            name = "value";
            array = mxGetField(spec, idx, name);
            param->mask_idx = -1;
        }

        /* Get the value from *array */
        if (!array
                || !mxIsDouble(array)
                || !(p_val = mxGetPr(array))
                || !(param->count = mxGetNumberOfElements(array))) {
            pr_error(slave, ctxt, name, __LINE__,
                    "Value invalid or missing\n");
            return -1;
        }

        if (param->mask_idx < 0) {
            /* Hard wired value */
            CHECK_CALLOC(slave->S,
                    param->count, sizeof(real_T), param->value);
            for (i = 0; i < param->count; ++i)
                param->value[i] = *p_val++;
        }
        else {
            /* value from mask parameter */
            param->value = p_val;
        }

        /* debugging */
        pr_debug(slave, NULL, "", 1, "%s: ",
                param->name ? param->name : "(Constant)");
        if (param->mask_idx >= 0)
            pr_debug(slave, NULL, NULL, 0,
                    "(Mask parameter %i) ", param->mask_idx);
        for (i = 0; i < param->count; ++i)
            pr_debug(slave, NULL, NULL, 0, "%g,",
                    param->value[i]);
        pr_debug(slave, NULL, NULL, 0, "\n");

        slave->runtime_param_count++;
    }

    return 0;
}


/****************************************************************************
 * parameter specification is of the form:
 *    []
 *    | value
 *    | { 'string', value }
 *    | { uint32_t(mask index), <element, <isVector>> }
 * anything else is an error
 ****************************************************************************/
static int_T
get_port_parameter(struct ecat_slave *slave, const char_T *p_ctxt,
        const mxArray *port_spec, struct io_port *port,
        uint_T element, struct param_spec *param, const char_T *name)
{
    const mxArray *spec = mxGetField(port_spec, element, name);
    const mxArray *name_spec = 0, *value_spec;
    const int_T port_width = port->pdo_end - port->pdo;
    char_T name_ctxt[30], value_ctxt[30];
    int_T i, spec_count;

    if (!spec || !(spec_count = mxGetNumberOfElements(spec)))
        return 0;

    param->element_idx = -1;

    if (mxIsDouble(spec)) {
        /* Anonymous array of numbers */
        value_spec = spec;

        snprintf(value_ctxt, sizeof(value_ctxt), "%s", name);
    }
    else if (mxIsCell(spec)) {
        /* {'Name', value} specification */

        if (spec_count < 2) {
            pr_error(slave, p_ctxt, name, __LINE__,
                    "Need a cell array with 2 elements");
            return -1;
        }

        /* Evaluate first elements of cell array */
        name_spec  = mxGetCell(spec, 0);
        value_spec = mxGetCell(spec, 1);

        /* Write contexts */
        snprintf( name_ctxt, sizeof( name_ctxt), "%s{1}", name);
        snprintf(value_ctxt, sizeof(value_ctxt), "%s{2}", name);
    }
    else if (mxIsStruct(spec)) {
        const mxArray *param_spec   = mxGetField(spec, 0, "parameter");
        const mxArray *element_spec = mxGetField(spec, 0, "element");
        name_spec                   = mxGetField(spec, 0, "name");
        value_spec                  = mxGetField(spec, 0, "value");

        if (name_spec) {
            if (!value_spec) {
                pr_error(slave, p_ctxt, name, __LINE__,
                        "Field 'value' missing");
                return -1;
            }

            if (param_spec || element_spec) {
                pr_error(slave, p_ctxt, name, __LINE__,
                        "Cannot specify 'name' "
                        "together with 'parameter' or 'element'.");
                return -1;
            }

            snprintf(name_ctxt, sizeof(name_ctxt), "%s.name", name);
            snprintf(value_ctxt, sizeof(value_ctxt), "%s.value", name);
        }
        else if (value_spec) {
            pr_error(slave, p_ctxt, name, __LINE__,
                    "Cannot specify 'value' without 'name'");
            return -1;
        }
        else if (!param_spec) {
            pr_error(slave, p_ctxt, name, __LINE__,
                    "Field 'parameter' missing");
            return -1;
        }
        else {
            param->is_ref = true;

            param->ptr =
                slave->parameter + (int_T)mxGetScalar(param_spec);
            /* Check the pointer */
            if (param->ptr < slave->parameter
                    || param->ptr >= slave->parameter_end) {
                snprintf(value_ctxt, sizeof(value_ctxt),
                        "%s.parameter", name);
                pr_error(slave, p_ctxt, value_ctxt, __LINE__,
                        "Index %zi does not point "
                        "to a valid mask parameter.",
                        param->ptr - slave->parameter);
                return -1;
            }

            if (element_spec) {
                param->element_idx = mxGetScalar(element_spec);
                if (param->element_idx < 0
                        || param->element_idx >= param->ptr->count) {
                    snprintf(value_ctxt, sizeof(value_ctxt),
                            "%s.element", name);
                    pr_error(slave, p_ctxt, value_ctxt, __LINE__,
                            "Mask parameter index %i "
                            "is not in the range [0,%i)",
                            param->element_idx,
                            param->ptr->count);
                    return -1;
                }
            }
        }
    }
    else {
        pr_error(slave, p_ctxt, name, __LINE__,
                "Specification invalid.");
        return -1;
    }

    if (value_spec) {
        real_T *val;

        spec_count = mxGetNumberOfElements(value_spec);
        if (!spec_count)
            return 0;

        if (!mxIsDouble(value_spec)
                || !(val = mxGetPr(value_spec))) {
            pr_error(slave, p_ctxt, value_ctxt, __LINE__,
                    "Value not a valid numeric.");
            return -1;
        }

        CHECK_CALLOC(slave->S, 1, sizeof(struct parameter), param->ptr);
        param->ptr->count = spec_count;
        param->ptr->mask_idx = -1;
        CHECK_CALLOC(slave->S, spec_count,
                sizeof(real_T), param->ptr->value);
        
        for (i = 0; i < spec_count; ++i)
            param->ptr->value[i] = val[i];
    }

    if (name_spec) {
        param->ptr->name = mxArrayToString(name_spec);
        if (!param->ptr->name || !strlen(param->ptr->name)) {
            pr_error(slave, p_ctxt, name_ctxt, __LINE__,
                    "Parameter name not a valid string");
            return -1;
        }
        slave->runtime_param_count++;
    }

    /* Check for matching parameter and port widths */
    if (param->element_idx < 0
            && param->ptr->count > 1
            && param->ptr->count != port_width) {
        pr_error(slave, p_ctxt, name, __LINE__,
                "Parameter and port widths do not match.");
        return -1;
    }

    /* Debugging */
    pr_debug(slave, NULL, "", 3, "%s parameter: '%s'=", name,
            param->ptr->name ?  param->ptr->name : "(const)");
    if (param->element_idx < 0) {
        for (i = 0; i < param->ptr->count; ++i)
            pr_debug(slave, NULL, NULL, 0, "%f,",
                    param->ptr->value[i]);
    } else {
        pr_debug(slave, NULL, NULL, 0, "%f,",
                param->ptr->value[param->element_idx]);
    }
    pr_debug(slave, NULL, NULL, 0, "\n");

    return 0;
}

/****************************************************************************/
static int
get_port_raw_pdo_spec (struct ecat_slave *slave, const char_T *p_ctxt,
        struct io_port *port, real_T *val, enum sm_direction dir)
{
    size_t i, rows;
    const struct sync_manager *sm;
    const struct pdo *pdo;
    const struct pdo_entry *entry;
    const char_T *element;

    /* First element is SM index */
    element = "pdo(1,1)";
    sm = slave->sync_manager + (size_t)val[0];
    if (sm < slave->sync_manager || sm >= slave->sync_manager_end) {
        pr_error(slave, p_ctxt, element, __LINE__,
                "SyncManager row index %zi out of range [0,%zu)",
                sm - slave->sync_manager,
                slave->sync_manager_end - slave->sync_manager);
        return -1;
    }

    if (sm->direction != dir) {
        pr_warn(slave, p_ctxt, element, __LINE__,
                "SyncManager direction is incorrect\n");
    }

    /* Second element is PDO index */
    element = "pdo(1,2)";
    pdo = sm->pdo + (size_t)val[1];
    if (pdo < sm->pdo || pdo >= sm->pdo_end) {
        pr_error(slave, p_ctxt, element, __LINE__,
                "Pdo row index %zi out of range [0,%zu)",
                pdo - sm->pdo,
                sm->pdo_end - sm->pdo);
        return -1;
    }

    /* Add up the bits in the PDO */
    rows = 0;
    for (entry = pdo->entry; entry != pdo->entry_end; entry++)
        rows += entry->bitlen;

    if (rows % 8) {
        pr_warn(slave, p_ctxt, element, __LINE__,
                "PDO BitLen is not byte aligned\n");
    }

    /* One row for every byte */
    rows = (rows + 7) / 8;

    CHECK_CALLOC(slave->S, rows, sizeof(struct port_pdo), port->pdo);
    port->pdo_end = port->pdo + rows;

    for (i = 0; i < rows; i++) {
        port->pdo[i].entry = pdo->entry;
        port->pdo[i].element_idx = i;
    }

    /* Set data types for port and PDO */
    port->sl_port_data_type = SS_UINT8;
    port->data_type = type_uint8;

    return 0;
}


/****************************************************************************/
static int
get_port_pdo_spec (struct ecat_slave *slave, const char_T *p_ctxt,
        const mxArray *port_spec, struct io_port *port, size_t idx,
        real_T *val, int_T rows, enum sm_direction dir)
{
    static const char_T* spec_name = "pdo_data_type";
    int_T j;
    real_T real;

    /* Read the PDO data type */
    port->data_type = get_data_type(slave, p_ctxt, spec_name, __LINE__,
            mxGetField(port_spec, idx, spec_name));
    if (!port->data_type) {
        if (!ssGetErrorStatus(slave->S))
            pr_error(slave, p_ctxt, spec_name, __LINE__,
                    "Data type unspecified");
        return -1;
    }

    CHECK_CALLOC(slave->S, rows, sizeof(struct port_pdo), port->pdo);
    port->pdo_end = port->pdo + rows;
    for (j = 0; j < rows; j++) {
        const struct sync_manager *sm;
        const struct pdo *pdo;
        char_T element[20];

        sm = slave->sync_manager + (size_t)val[j];
        if (sm < slave->sync_manager
                || sm >= slave->sync_manager_end) {
            snprintf(element, sizeof(element), "pdo(%i,1)", j+1);
            pr_error(slave, p_ctxt, element, __LINE__,
                    "SyncManager row index %zi out of range [0,%zu)",
                    (ssize_t)val[j],
                    slave->sync_manager_end - slave->sync_manager);
            return -1;
        }

        if (sm->direction != dir) {
            snprintf(element, sizeof(element), "pdo(%i,1)", j+1);
            pr_warn(slave, p_ctxt, element, __LINE__,
                    "SyncManager direction is incorrect\n");
        }

        pdo = sm->pdo + (size_t)val[j + rows];
        if (pdo < sm->pdo
                || pdo >= sm->pdo_end) {
            snprintf(element, sizeof(element), "pdo(%i,2)", j+1);
            pr_error(slave, p_ctxt, element, __LINE__,
                    "Pdo row index %zi out of range [0,%zu)",
                    (ssize_t)val[j + rows],
                    sm->pdo_end - sm->pdo);
            return -1;
        }

        port->pdo[j].entry = pdo->entry + (size_t)val[j + 2*rows];
        if (port->pdo[j].entry < pdo->entry
                || port->pdo[j].entry >= pdo->entry_end) {
            snprintf(element, sizeof(element), "pdo(%i,3)", j+1);
            pr_error(slave, p_ctxt, element, __LINE__,
                    "PdoEntry row index %zi out of range [0,%zu)",
                    (ssize_t)val[j + 2*rows],
                    pdo->entry_end - pdo->entry);
            return -1;
        }

        if (!port->pdo[j].entry->index) {
            snprintf(element, sizeof(element), "pdo(%i,3)", j+1);
            pr_error(slave, p_ctxt, element, __LINE__,
                    "Cannot choose Pdo Entry #x0000");
            return -1;
        }

        port->pdo[j].element_idx = val[j + 3*rows];
        if (port->data_type->mant_bits * (port->pdo[j].element_idx + 1)
                > port->pdo[j].entry->bitlen) {
            snprintf(element, sizeof(element), "pdo(%i,4)", j+1);
            pr_error(slave, p_ctxt, element, __LINE__,
                    "Element index %zi out of range [0,%u)",
                    (ssize_t)val[j + 3*rows],
                    (port->pdo[j].entry->bitlen
                     / port->data_type->mant_bits));
            return -1;
        }

        pr_debug(slave, NULL, "", 3,
                "Pdo Entry #x%04X.%u element %s[%zu]\n",
                port->pdo[j].entry->index,
                port->pdo[j].entry->subindex,
                port->data_type->name,
                port->pdo[j].element_idx);
    }

    /* Read the endianness if specified */
    real = 0;
    RETURN_ON_ERROR(get_numeric_field(slave, p_ctxt, __LINE__,
                port_spec, idx, 1, 1, 0, "big_endian", &real));
    port->big_endian = real != 0.0;

    RETURN_ON_ERROR (get_port_parameter(slave, p_ctxt, port_spec,
                port, idx, &port->gain, "gain"));
    RETURN_ON_ERROR (get_port_parameter(slave, p_ctxt, port_spec,
                port, idx, &port->offset, "offset"));

    if (dir == EC_SM_INPUT) {
        RETURN_ON_ERROR (get_port_parameter(slave, p_ctxt, port_spec,
                    port, idx, &port->filter, "filter"));
        if (port->filter.ptr)
            slave->filter_count += port->pdo_end - port->pdo;
    }

    RETURN_ON_ERROR(get_numeric_field(slave, p_ctxt, __LINE__,
                port_spec, idx, 0, 1, 0,
                "full_scale", &port->fullscale));

    if (port->gain.ptr || port->offset.ptr || port->filter.ptr
            || port->fullscale) {
        /* Data type is always double if gain, offset, filter
         * or fullscale is used */
        port->sl_port_data_type = SS_DOUBLE;

        /* Temporary storage will be needed for data */
        port->dwork_idx = ++slave->dwork_count;
    }
    else {
        /* Input ports (with PDO dir = EC_SM_OUTPUT) have their port
         * data types set dynamically, output port data types are
         * fixed to PDO's data type */
        port->sl_port_data_type = dir == EC_SM_OUTPUT
            ? DYNAMICALLY_TYPED : port->data_type->sl_type;
    }

    return 0;
}


/****************************************************************************/
static int_T
get_port_config(struct ecat_slave *slave, const char_T *section,
        enum sm_direction dir, struct io_port **port_begin)
{
    const mxArray * const io_spec = ssGetSFcnParam(slave->S, PORT_CONFIG);
    const mxArray *port_spec;
    const char_T *param = "PORT_CONFIG";
    uint_T count = 0;
    size_t i;
    struct io_port *port = 0;

    *port_begin = 0;

    if (!io_spec) {
        pr_info(slave, NULL, NULL, 0,
                "No block input and output configuration spec "
                "was found\n");
        return 0;
    }

    port_spec = mxGetField(io_spec, 0, section);

    if (!port_spec || !(count = mxGetNumberOfElements(port_spec)))
        return 0;

    pr_debug(slave, NULL, NULL, 0,
            "--------------- Parsing %s IOSpec ------------\n", section);

    CHECK_CALLOC(slave->S, count, sizeof(struct io_port), port);
    *port_begin = port;

    pr_debug(slave, NULL, "", 1, "Port count %u\n", count);
    for (i = 0; i < count; i++) {
        char_T ctxt[50];
        const mxArray *pdo_spec;
        real_T *val;
        int_T rows, cols, numel;

        snprintf(ctxt, sizeof(ctxt), "%s.%s(%zu)", param, section, i+1);

        pr_debug(slave, NULL, "", 2, "Port %zu\n", i+1);

        pdo_spec = mxGetField(port_spec, i, "pdo");
        if (!pdo_spec
                || !(rows = mxGetM(pdo_spec))
                || !(cols = mxGetN(pdo_spec))
                || !(numel = mxGetNumberOfElements(pdo_spec)))
            continue;

        if (!mxIsDouble(pdo_spec)
                || !(val = mxGetPr(pdo_spec))) {
            pr_error(slave, ctxt, "pdo", __LINE__,
                    "Pdo specification is not a valid numeric array");
            return -1;
        }

        if (numel == 2) {
            /* When there are only 2 elements, use PDO Raw mode.
             * The whole PDO is presented as a byte array */
            RETURN_ON_ERROR(get_port_raw_pdo_spec(slave, ctxt,
                        port, val, dir));
        }
        else if (cols == 4) {
            /* PDO specification is an Nx4 numeric matrix */
            RETURN_ON_ERROR(get_port_pdo_spec(slave, ctxt,
                        port_spec, port, i, val, rows, dir));
        }
        else {
            pr_error(slave, ctxt, "pdo", __LINE__,
                    "Pdo specification is not a [Mx4] array"
                    " or 2 element vector");
            return -1;
        }

        port++;
    }

    return port - *port_begin;
}

/****************************************************************************/
static int_T
get_ioport_config(struct ecat_slave *slave)
{
    int_T n;

    /* Note: a simulink's output port corresponds to an input for
     * the master */
    RETURN_ON_ERROR (n = get_port_config(slave,
                "output", EC_SM_INPUT, &slave->o_port));
    slave->o_port_end = slave->o_port + n;

    RETURN_ON_ERROR (n = get_port_config(slave,
                "input", EC_SM_OUTPUT, &slave->i_port));
    slave->i_port_end = slave->i_port + n;

    return 0;
}

static void
slave_port_mem_op(
    struct io_port *port_start, const struct io_port *port_end,
    void (*method)(void*))
{
    const struct io_port *port;

    for (port = port_start; port != port_end; port++) {
        if (port->gain.ptr && !port->gain.is_ref) {
            (*method)(port->gain.ptr->name);
            (*method)(port->gain.ptr->value);
            (*method)(port->gain.ptr);
        }

        if (port->offset.ptr && !port->offset.is_ref) {
            (*method)(port->offset.ptr->name);
            (*method)(port->offset.ptr->value);
            (*method)(port->offset.ptr);
        }

        if (port->filter.ptr && !port->filter.is_ref) {
            (*method)(port->filter.ptr->name);
            (*method)(port->filter.ptr->value);
            (*method)(port->filter.ptr);
        }

        (*method)(port->pdo);
    }
    (*method)(port_start);
}

/* This function is used to operate on the allocated memory with a generic
 * operator. This function is used to fix or release allocated memory.
 */
static void
slave_mem_op(struct ecat_slave *slave, void (*method)(void*))
{
    const struct sync_manager *sm;
    const struct pdo *pdo;
    const struct soe_config *soe;
    const struct sdo_config *sdo;
    const struct parameter *param;

    (*method)(slave->type);

    for (sdo = slave->sdo_config; sdo != slave->sdo_config_end; sdo++)
        (*method)(sdo->byte_array);
    (*method)(slave->sdo_config);

    for (soe = slave->soe_config; soe != slave->soe_config_end; soe++)
        (*method)(soe->octet_string);
    (*method)(slave->soe_config);

    for (sm = slave->sync_manager; sm != slave->sync_manager_end; sm++) {
        for (pdo = sm->pdo; pdo != sm->pdo_end; pdo++)
            (*method)(pdo->entry);
        (*method)(sm->pdo);
    }
    (*method)(slave->sync_manager);

    for (param = slave->parameter; param != slave->parameter_end; ++param) {
        if (param->mask_idx < 0)
            (*method)(param->value);
        (*method)(param->name);
    }
    (*method)(slave->parameter);

    slave_port_mem_op(slave->o_port, slave->o_port_end, method);
    slave_port_mem_op(slave->i_port, slave->i_port_end, method);

    (*method)(slave);
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
    int_T i;
    struct ecat_slave *slave;
    const struct io_port *port;

    /* Make sure there are at least PARAM_COUNT parameters */
    i = ssGetSFcnParamsCount(S);
    if (i < PARAM_COUNT)
        i = PARAM_COUNT;

    ssSetNumSFcnParams(S, i);  /* Number of expected parameters */
    if (ssGetNumSFcnParams(S) != ssGetSFcnParamsCount(S)) {
        /* Return if number of expected != number of actual parameters */
        return;
    }

    for( i = 0; i < ssGetNumSFcnParams(S); i++)
        ssSetSFcnParamTunable(S, i, SS_PRM_NOT_TUNABLE);

    /* allocate memory for slave structure */
    if (!(slave = mxCalloc(1, sizeof(*slave)))) {
        return;
    }
    slave->debug_level = mxGetScalar(ssGetSFcnParam(S, DEBUG));
    slave->S = S;

    if (get_slave_info(slave)) return;

    if (get_slave_config(slave)) return;

    if (ssGetNumSFcnParams(S) > PARAM_COUNT
            && get_parameter_config(slave))
        return;

    if (get_ioport_config(slave)) return;

    /* Process input ports */
    if (!ssSetNumInputPorts(S, slave->i_port_end - slave->i_port))
        return;
    for (i = 0, port = slave->i_port;
            port != slave->i_port_end; port++, i++) {
        ssSetInputPortWidth   (S, i, DYNAMICALLY_SIZED);
        ssSetInputPortDataType(S, i, port->sl_port_data_type);
    }

    /* Process output ports */
    if (!ssSetNumOutputPorts(S, slave->o_port_end - slave->o_port))
        return;
    for (i = 0, port = slave->o_port;
            port != slave->o_port_end; port++, i++) {
        ssSetOutputPortWidth   (S, i, port->pdo_end - port->pdo);
        ssSetOutputPortDataType(S, i, port->sl_port_data_type);
    }

    ssSetNumSampleTimes(S, 1);
    ssSetNumDWork(S, DYNAMICALLY_SIZED);

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

/* Function: mdlSetInputPortDataType ========================================
 * Abstract:
 *    This function is called on input ports whos data type could not be fixed
 *    until now.
 */
#define MDL_SET_INPUT_PORT_DATA_TYPE
static void mdlSetInputPortDataType(SimStruct *S, int_T p, DTypeId id)
{
    struct ecat_slave *slave = ssGetUserData(S);
    struct io_port *port = slave->i_port + p;

    /* Check whether the data type is compatible with the PDO data type or is
     * SS_SINGLE or SS_DOUBLE */
    if (port->data_type->sl_type != id) {
        if (id != SS_DOUBLE
                && id != SS_SINGLE && port->data_type != type_bool) {
            snprintf(msg, sizeof(msg),
                    "Trying to set data type of input port %i to %s,\n"
                    "whereas PDO has data type %s. Choose PDO data type\n"
                    "or SS_DOUBLE or SS_SINGLE",
                    p + 1, ssGetDataTypeName(S, id), port->data_type->name);
            ssSetErrorStatus(S, msg);
            return;
        }
        port->dwork_idx = ++slave->dwork_count;
        port->sl_port_data_type = id;
    }

    ssSetInputPortDataType(S, p, id);
}

/* Function: mdlSetInputPortWidth ===========================================
 * Abstract:
 *    This function is called on input ports whos input width could not be
 *    determined until this point
 */
#define MDL_SET_INPUT_PORT_WIDTH
static void mdlSetInputPortWidth(SimStruct *S, int_T port, int_T width)
{
    struct ecat_slave *slave = ssGetUserData(S);
    int_T max_width = slave->i_port[port].pdo_end - slave->i_port[port].pdo;

    if (!slave)
        return;

    if (width > max_width) {
        snprintf(errmsg, sizeof(errmsg),
                "Trying to assign a vector of %u elements "
                "to input port %u, which has only %u objects.",
                width, port+1, max_width);
        ssSetErrorStatus(S, errmsg);
    }

    ssSetInputPortWidth(S, port, width);

    pr_debug(slave, ssGetPath(S), "", 0,
            "Setting input port width of port %u to %u\n",
            port+1, width);
}

#define MDL_SET_DEFAULT_PORT_DATA_TYPES
static void mdlSetDefaultPortDataTypes(SimStruct *S)
{
    struct ecat_slave *slave = ssGetUserData(S);
    const struct io_port *port;
    uint_T i = 0;

    if (!slave)
        return;

    for (port = slave->i_port; port != slave->i_port_end; port++, i++) {
        if (ssGetInputPortDataType(S, i) == DYNAMICALLY_TYPED)
            ssSetInputPortDataType(S, i, port->data_type->sl_type);
    }
}

/* This function is called when some ports are still DYNAMICALLY_SIZED
 * even after calling mdlSetInputPortWidth(). This occurs when input
 * ports are not connected. */
#define MDL_SET_DEFAULT_PORT_DIMENSION_INFO
static void mdlSetDefaultPortDimensionInfo(SimStruct *S)
{
    struct ecat_slave *slave = ssGetUserData(S);
    const struct io_port *port;
    uint_T i = 0;

    if (!slave)
        return;

    for (port = slave->i_port; port != slave->i_port_end; port++, i++) {
        if (ssGetInputPortWidth(S, i) == DYNAMICALLY_SIZED)
            ssSetInputPortWidth(S, i, port->pdo_end - port->pdo);
    }
}

static uint_T
create_runtime_parameter(SimStruct *S,
        int_T idx, struct parameter *p, boolean_T is_ref)
{
    ssParamRec rtp;
    if (!p || !p->name || is_ref)
        return 0;

    rtp.name = p->name;
    rtp.nDimensions = 1;
    rtp.dimensions = &p->count;
    rtp.dataTypeId = SS_DOUBLE;
    rtp.complexSignal = 0;
    rtp.data = p->value;
    rtp.dataAttributes = NULL;
    if (p->mask_idx < 0) {
        rtp.nDlgParamIndices = 0;
        rtp.dlgParamIndices = NULL;
        rtp.transformed = RTPARAM_TRANSFORMED;
    }
    else {
        rtp.nDlgParamIndices = 1;
        rtp.dlgParamIndices = &p->mask_idx;
        rtp.transformed = RTPARAM_NOT_TRANSFORMED;
    }
    rtp.outputAsMatrix = 0;
    ssSetRunTimeParamInfo(S, idx, &rtp);

    return 1;
}

static uint_T
mdl_set_port_work_widths(SimStruct* S,
    uint_T param_idx, const struct io_port *port, int_T port_width)
{
    param_idx += create_runtime_parameter (S,
            param_idx, port->gain.ptr, port->gain.is_ref);
    param_idx += create_runtime_parameter (S,
            param_idx, port->offset.ptr, port->offset.is_ref);
    param_idx += create_runtime_parameter (S,
            param_idx, port->filter.ptr, port->filter.is_ref);

    if (port->dwork_idx) {
        ssSetDWorkWidth(S, port->dwork_idx-1, port_width);
        ssSetDWorkDataType(S, port->dwork_idx-1,
                port->data_type->sl_type);
    }

    return param_idx;
}

#define MDL_SET_WORK_WIDTHS
static void mdlSetWorkWidths(SimStruct *S)
{
    struct ecat_slave *slave = ssGetUserData(S);
    struct parameter *param;
    const struct io_port *port;
    uint_T param_idx = 0;
    int_T i;

    if (!slave)
        return;

    ssSetNumRunTimeParams(S, slave->runtime_param_count);

    ssSetNumDWork(S, slave->dwork_count);

    for (param = slave->parameter; param != slave->parameter_end; ++param)
        param_idx += create_runtime_parameter(S, param_idx, param, false);

    for (port = slave->o_port, i = 0; port != slave->o_port_end; port++, i++)
        param_idx = mdl_set_port_work_widths(S,
                param_idx, port, ssGetOutputPortWidth(S, i));

    for (port = slave->i_port, i = 0; port != slave->i_port_end; port++, i++)
        param_idx = mdl_set_port_work_widths(S,
                param_idx, port, ssGetInputPortWidth(S, i));
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

static int_T
mdlRTWWritePortParameter(struct ecat_slave *slave,
        int_T *param_idx, int32_T *param_spec_idx,
        const struct param_spec* param_spec)
{
    const struct parameter *param = param_spec->ptr;
    if (!param)
        return 0;

    if (param->name) {
        if (!ssWriteRTWParamSettings(slave->S, 2,
                    SSWRITE_VALUE_QSTR, "Name", param->name,

                    SSWRITE_VALUE_DTYPE_NUM, "Element",
                    &param_spec->element_idx, DTINFO(SS_INT32, 0)))
            return -1;
    }
    else if (!ssWriteRTWParamSettings(slave->S, 1,
                SSWRITE_VALUE_VECT, "Value", param->value, param->count))
        return -1;
    *param_spec_idx = (*param_idx)++;

    return 0;
}

static int_T
mdlRTWWritePort(struct ecat_slave *slave,
        const struct io_port *port, int_T *idx)
{
    uint32_T (*pdo_spec)[3];
    int32_T param_idx[3] = {-1,-1,-1};
    const struct port_pdo *pdo;
    size_t i;
    enum {
        PS_PdoEntryIndex = 0,
        PS_PdoEntrySubIndex,
        PS_ElementIndex,
    };

    if (mdlRTWWritePortParameter(slave, idx, param_idx + 0, &port->gain))
        return -1;
    if (mdlRTWWritePortParameter(slave, idx, param_idx + 1, &port->offset))
        return -1;
    if (mdlRTWWritePortParameter(slave, idx, param_idx + 2, &port->filter))
        return -1;

    pdo_spec = mxCalloc(port->pdo_end - port->pdo, sizeof(*pdo_spec));
    for (pdo = port->pdo, i = 0; pdo != port->pdo_end; pdo++, i++) {
        pdo_spec[i][PS_PdoEntryIndex]    = pdo->entry->index;
        pdo_spec[i][PS_PdoEntrySubIndex] = pdo->entry->subindex;
        pdo_spec[i][PS_ElementIndex]     = pdo->element_idx;
    }

    if (!ssWriteRTWParamSettings(slave->S, 6,
                SSWRITE_VALUE_DTYPE_2DMAT, "Pdo",
                pdo_spec, 3, i, DTINFO(SS_UINT32, 0),

                SSWRITE_VALUE_DTYPE_NUM, "PdoDataTypeId",
                &port->data_type->id, DTINFO(SS_UINT32,0),

                SSWRITE_VALUE_DTYPE_NUM, "BigEndian",
                &port->big_endian, DTINFO(SS_BOOLEAN,0),

                SSWRITE_VALUE_DTYPE_VECT, "Param",
                param_idx, 3, DTINFO(SS_INT32, 0),

                SSWRITE_VALUE_NUM, "FullScale",
                port->fullscale,

                SSWRITE_VALUE_DTYPE_NUM, "DWorkIndex",
                &port->dwork_idx, DTINFO(SS_INT32,0)))
        return -1;

    mxFree(pdo_spec);

    return 0;
}

#define MDL_RTW
static void mdlRTW(SimStruct *S)
{
    struct ecat_slave *slave = ssGetUserData(S);
    int_T param_idx = 0;
    int32_T *config_idx;
    size_t n;

    const struct io_port *port;
    const struct sdo_config *sdo;
    const struct soe_config *soe;

    /* General assignments of array indices that form the basis for
     * the S-Function <-> TLC communication
     * DO NOT CHANGE THESE without updating the TLC ec_slave3.tlc
     * as well */
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
        SdoConfigIndex = 0,         /* 0 */
        SdoConfigSubIndex,          /* 1 */
        SdoConfigDataType,          /* 2 */
        SdoConfigValue,             /* 3 */
        SdoConfigMax                /* 4 */
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

    /* Sdo Configuration */
    n = slave->sdo_config_end - slave->sdo_config;
    config_idx = mxCalloc(n, sizeof(*config_idx));
    for (sdo = slave->sdo_config, n = 0;
            sdo != slave->sdo_config_end; sdo++, n++) {
        if (sdo->byte_array) {
            if (!ssWriteRTWParamSettings(slave->S, 3,

                    SSWRITE_VALUE_DTYPE_NUM, "Index",
                    &sdo->index, DTINFO(SS_UINT16, 0),

                    SSWRITE_VALUE_DTYPE_NUM, "SubIndex",
                    &sdo->subindex, DTINFO(SS_INT16, 0),

                    SSWRITE_VALUE_DTYPE_VECT, "ByteArray",
                    sdo->byte_array, (uint_T)sdo->value, DTINFO(SS_UINT8, 0)))
                return;
        }
        else {
            if (!ssWriteRTWParamSettings(slave->S, 4,
                    SSWRITE_VALUE_DTYPE_NUM, "Index",
                    &sdo->index, DTINFO(SS_UINT16, 0),

                    SSWRITE_VALUE_DTYPE_NUM, "SubIndex",
                    &sdo->subindex, DTINFO(SS_INT16, 0),

                    SSWRITE_VALUE_DTYPE_NUM, "DataTypeId",
                    &sdo->datatype->id, DTINFO(SS_UINT32, 0),

                    SSWRITE_VALUE_NUM, "Value",
                    sdo->value))
                return;
        }
        config_idx[n] = param_idx++;
    }
    if (!ssWriteRTWVectParam(S, "SdoConfig", config_idx, SS_INT32, n))
        return;
    mxFree(config_idx);

    /* SoE configuration */
    n = slave->soe_config_end - slave->soe_config;
    config_idx = mxCalloc(n, sizeof(*config_idx));
    for (soe = slave->soe_config, n = 0;
            soe != slave->soe_config_end; soe++, n++) {
        if (!ssWriteRTWParamSettings(slave->S, 2,
                SSWRITE_VALUE_DTYPE_NUM, "Index",
                &soe->index, DTINFO(SS_UINT16, 0),

                SSWRITE_VALUE_DTYPE_VECT, "OctetString",
                soe->octet_string, soe->octet_string_len,
                DTINFO(SS_UINT8, 0)))
            return;
        config_idx[n] = param_idx++;
    }
    if (!ssWriteRTWVectParam(S, "SoeConfig", config_idx, SS_INT32, n))
        return;
    mxFree(config_idx);

    if (slave->sync_manager_end != slave->sync_manager) {
        uint_T sm_count = slave->sync_manager_end - slave->sync_manager;
        uint32_T (*sync_manager)[SM_Max];
        uint32_T (*pdo_info)[PdoInfo_Max];
        uint_T m_pdo_entry_idx = 0;
        uint32_T (*pdo_entry_info)[PdoEI_Max];

        uint_T pdo_idx = 0, pdo_entry_idx = 0;

        const struct sync_manager *sm;

        sync_manager = mxCalloc(sm_count, sizeof(*sync_manager));
        pdo_info = mxCalloc(slave->pdo_count, sizeof(*pdo_info));
        pdo_entry_info =
            mxCalloc(slave->pdo_entry_count, sizeof(*pdo_entry_info));

        for (sm = slave->sync_manager; sm != slave->sync_manager_end; sm++) {
            const struct pdo *pdo;
            size_t sm_idx = sm - slave->sync_manager;

            sync_manager[sm_idx][SM_Index] = sm->index;
            sync_manager[sm_idx][SM_Direction] =
                sm->direction == EC_SM_INPUT;
            sync_manager[sm_idx][SM_PdoCount] = 0;

            for (pdo = sm->pdo; pdo != sm->pdo_end; pdo++) {
                const struct pdo_entry *pdo_entry;

                sync_manager[sm_idx][SM_PdoCount]++;

                pdo_info[pdo_idx][PdoInfo_PdoIndex] =
                    pdo->index;
                pdo_info[pdo_idx][PdoInfo_PdoEntryCount] =
                    pdo->entry_end - pdo->entry;

                for (pdo_entry = pdo->entry; pdo_entry != pdo->entry_end;
                        pdo_entry++, pdo_entry_idx++) {
                    pdo_entry_info[pdo_entry_idx][PdoEI_Index] =
                        pdo_entry->index;
                    pdo_entry_info[pdo_entry_idx][PdoEI_SubIndex] =
                        pdo_entry->subindex;
                    pdo_entry_info[pdo_entry_idx][PdoEI_BitLen] =
                        pdo_entry->bitlen;

                    m_pdo_entry_idx++;
                }
                pdo_idx++;
            }
        }

        if (slave->pdo_entry_count) {
            if (!ssWriteRTW2dMatParam(S, "PdoEntryInfo", pdo_entry_info,
                        SS_UINT32, PdoEI_Max, slave->pdo_entry_count))
                return;
        }
        if (slave->pdo_count) {
            if (!ssWriteRTW2dMatParam(S, "PdoInfo", pdo_info,
                        SS_UINT32, PdoInfo_Max, slave->pdo_count))
                return;
        }
        /* Don't need to check for slave->sync_manager_count here,
         * was checked already */
        if (!ssWriteRTW2dMatParam(S, "SyncManager", sync_manager,
                    SS_UINT32, SM_Max,
                    slave->sync_manager_end - slave->sync_manager))
            return;

        if (slave->dc_opmode.assign_activate != 0) {
            if (!ssWriteRTWParamSettings(slave->S, 4,
                    SSWRITE_VALUE_DTYPE_NUM, "AssignActivate",
                    &slave->dc_opmode.assign_activate, DTINFO(SS_UINT16, 0),

                    SSWRITE_VALUE_DTYPE_NUM, "ShiftTimeSync0Input",
                    &slave->dc_opmode.shift_time_sync0_input,
                    DTINFO(SS_BOOLEAN, 0),

                    SSWRITE_VALUE_DTYPE_VECT, "Factor",
                    slave->dc_opmode.factor, 4, DTINFO(SS_INT32, 0),

                    SSWRITE_VALUE_DTYPE_VECT, "Time",
                    slave->dc_opmode.value, 4, DTINFO(SS_INT32, 0)))
                return;

            if (!ssWriteRTWScalarParam(S, "DcOpMode", &param_idx, SS_INT32))
                return;
            param_idx++;
        }

        mxFree(sync_manager);
        mxFree(pdo_info);
        mxFree(pdo_entry_info);
    }

    n = slave->o_port_end - slave->o_port;
    config_idx = mxCalloc(n, sizeof(*config_idx));
    for (port = slave->o_port, n = 0;
            port != slave->o_port_end; port++, n++) {
        if (mdlRTWWritePort(slave, port, &param_idx))
            return;

        config_idx[n] = param_idx++;
    }
    if (!ssWriteRTWVectParam(S, "OutputPortIdx", config_idx, SS_INT32, n))
        return;
    mxFree (config_idx);

    n = slave->i_port_end - slave->i_port;
    config_idx = mxCalloc(n, sizeof(*config_idx));
    for (port = slave->i_port, n = 0;
            port != slave->i_port_end; port++, n++) {
        if (mdlRTWWritePort(slave, port, &param_idx))
            return;

        config_idx[n] = param_idx++;
    }
    if (!ssWriteRTWVectParam(S, "InputPortIdx", config_idx, SS_INT32, n))
        return;
    mxFree (config_idx);

    if (!ssWriteRTWScalarParam(S,
                "FilterCount", &slave->filter_count, SS_INT32))
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
