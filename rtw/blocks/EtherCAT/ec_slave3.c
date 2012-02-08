/*
 * $Id$
 *
 * This SFunction implements a generic EtherCAT Slave.
 *
 * Copyright 2012, Richard Hacker
 * License: GPL
 *
 * Description:
 * This generic SFunction can be used to realise any EtherCAT slave that
 * is used in EtherLab. All the necessary information needed
 * to configure the slave, all the outputs and inputs, scaling,
 * filtering, etc. is specified via the parameters passed to the SFunction.
 *
 * Slave description:
 * This is a struct with the fields:
 *      master:
 *      domain:
 *      alias:
 *      position:
 *
 * Slave configuration:
 * This is a structure with the fields:
 *      vendor: Vendor Id
 *      product: Product Code
 *      description: (optional) Description string
 *
 *      sdo: SDO configuration; OPTIONAL; CellArray{n,4}
 *          { Index, SubIndex, BitLen, Value;...
 *            Index, SubIndex,      0, 'string' }; ...
 *            Index, SubIndex, BitLen, [Value, Value, ...] }
 *          BitLen = {8,16,32}
 *          Row 1 configures a single value
 *          Row 2 configures a string array
 *          Row 3 configures a variable array of uint8
 *
 *      soe: SOE configuration; OPTIONAL; CellArray of vectors
 *          { Index, [Value, Value, ...];...
 *            Index, 'string'}
 *          Row 1 configures a variable array of uint8
 *          Row 2 configures a string array
 *
 *      dc: Distributed Clocks Configuration: OPTIONAL; Vector[5]
 *          Matrix [ AssignActivate, ...
 *                   CycleTimeSync0, ShiftTimeSync0, ...
 *                   CycleTimeSync1, ShiftTimeSync1]
 *
 *      pdo: Optional slave pdo definition. This definition has 3 level of
 *           indirections Sm <- Pdos <- Entries
 *
 *          pdo:        {Sm*}
 *          Sm:         {SmIndex, SmDir, Pdos}
 *          SmIndex:    Index of SyncManager
 *          SmDir:      SyncManager direction, as seen by the Master:
 *                      0 => Output (RxPdo), 1 => Input (TxPdo)
 *
 *          Pdos:       {Pdo*}
 *          Pdo:        {PdoIndex, Entries}
 *          PdoIndex:   Index of Pdo
 *
 *          Entries:    [Entry*]        (Nx4 array)
 *          Entry:      [EntryIndex, EntrySubIndex, BitLen, DataType]
 *          EntryIndex: Index of Pdo Entry (If 0, only BitLen is considered)
 *          EntrySubIndex: SubIndex of Pdo Entry ( >= 0 )
 *          BitLen:     number of bits (>0)
 *          DataType:   0:    Unspecified
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
 *                        output = (PDO / full_scale) * gain + offset
 *
 *          .pdo = PdoSpec
 *          .pdo_data_type = specifies the data type of the PDO.
 *                              The data type specified in the pdo is only a
 *                              hint, and is used if the value in PORT_CONFIG
 *                              is zero or unspecified
 *          .big_endian = True for big endian data type
 *
 *      ParamSpec  := {'Name', vector}   Named value, will be a parameter
 *                   | vector          Constant anonymous real_T value
 *                   | []              Empty, do nothing
 *
 *                   The vector can have none, 1 or the same
 *                   number of elements as there are pdo's
 *
 *      PdoSpec   := [SmIdx, PdoIdx, PdoEntryIdx, DataIdx]
 *
 *      SmIdx, PdoIdx, PdoEntryIdx := Index into the slave configuration
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

enum {
    ADDRESS = 0,
    SLAVE_CONFIG,
    PORT_CONFIG,
    DEBUG,
    TSAMPLE,
    PARAM_COUNT
};

static struct datatype_info {
    uint_T id;
    char *name;
    int_T mant_bits;
    DTypeId sl_type;
} datatype_info[] = {
    { 1001, "Boolean",    1,  SS_BOOLEAN }, /*  0 */
    { 1002, "Bit2",       2,  SS_UINT8   }, /*  1 */
    { 1003, "Bit3",       3,  SS_UINT8   }, /*  2 */
    { 1004, "Bit4",       4,  SS_UINT8   }, /*  3 */
    { 1005, "Bit5",       5,  SS_UINT8   }, /*  4 */
    { 1006, "Bit6",       6,  SS_UINT8   }, /*  5 */
    { 1007, "Bit7",       7,  SS_UINT8   }, /*  6 */
    { 1008, "Unsigned8",  8,  SS_UINT8   }, /*  7 */
    { 1016, "Unsigned16", 16, SS_UINT16  }, /*  8 */
    { 1024, "Unsigned24", 24, SS_UINT32  }, /*  9 */
    { 1032, "Unsigned32", 32, SS_UINT32  }, /* 10 */
    { 1040, "Unsigned40", 40, SS_DOUBLE  }, /* 11 */
    { 1048, "Unsigned48", 48, SS_DOUBLE  }, /* 12 */
    { 1056, "Unsigned56", 56, SS_DOUBLE  }, /* 13 */
    { 1064, "Unsigned64", 64, SS_DOUBLE  }, /* 14 */

    { 2008, "Integer8",   8,  SS_INT8    }, /* 15 */
    { 2016, "Integer16",  16, SS_INT16   }, /* 16 */
    { 2032, "Integer32",  32, SS_INT32   }, /* 17 */
    { 2064, "Integer64",  64, SS_DOUBLE  }, /* 18 */

    { 3032, "Real32",     32, SS_SINGLE  }, /* 19 */
    { 3064, "Real64",     64, SS_DOUBLE  }, /* 20 */
    {    0,                                           },
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
    uint32_T product_code;
    char_T *type;

    struct sdo_config {
        uint16_T index;
        uint16_T subindex;

        /* Data type. One of SS_UINT8, SS_UINT16, SS_UINT32 */
        struct datatype_info *datatype;

        /* Configuration value or array length if it is used */
        uint_T value;
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

            struct sync_manager *sm;

            /* PDO Entries */
            struct pdo_entry {
                struct pdo *pdo;

                uint16_T index;
                uint8_T subindex;
                uint_T bitlen;
                struct datatype_info *data_type;
            } *entry, *entry_end;
        } *pdo, *pdo_end;

    } *sync_manager, *sync_manager_end;

    /* Distributed clocks */
    struct dc_opmode {
        uint16_T assign_activate;

        uint32_T sync0_cycle;
        uint32_T sync0_shift;
        uint32_T sync1_cycle;
        uint32_T sync1_shift;
    } dc_opmode;

    struct io_port {
        /* Structure for setting a port parameter */
        struct output_param {
            char_T *name;
            int32_T  count;
            real_T *value;
        } *gain, *offset, *filter;

        struct port_pdo {
            struct pdo_entry *entry;
            size_t element_idx;
        } *pdo, *pdo_end;

        int32_T  dwork_idx;
        int32_T filter_idx;

        real_T fullscale;

        /* Data type how to interpret PDO */
        struct datatype_info *data_type;

        /* Switches for manually setting port values */
        boolean_T big_endian;

        boolean_T port_debug;

        /* Simulink port data type */
        DTypeId sl_port_data_type;
    } *o_port, *o_port_end, *i_port, *i_port_end;

    /* Runtime parameters are used to store parameters where a name was
     * supplied for gain_name, offset_name and filter_name. These
     * parameters are exported in the C-API */
    uint_T runtime_param_count;

    int_T filter_count;
    int_T dwork_count;
    int_T const_count;
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

    if (slave->debug_level < 1)
        return;

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
    uint_T entry_count, i, data_type_id;
    const real_T *pval;
    real_T val;

    if (!mxIsCell(pdo_def) || mxGetNumberOfElements(pdo_def) != 2) {
        pr_error(slave, NULL, p_ctxt, __LINE__,
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

    /* Check that Pdo Entry is a Nx4 array */
    if (mxGetN(pdo_cell) != 4
            || !mxIsDouble(pdo_cell) || !(pval = mxGetPr(pdo_cell))) {
        snprintf(ctxt, sizeof(ctxt), "%s{3}", p_ctxt);
        pr_error(slave, NULL, ctxt, __LINE__,
                "Value is not a Nx4 numeric array");
        return -1;
    }

    CHECK_CALLOC(slave->S, entry_count,
            sizeof(struct pdo_entry), pdo->entry);
    pdo->entry_end = pdo->entry + entry_count;

    for (i = 0; i < entry_count; i++) {
        struct pdo_entry *pdo_entry = pdo->entry + i;

        pdo_entry->pdo = pdo;

        pdo_entry->index = pval[i];
        pdo_entry->bitlen = pval[i + 2*entry_count];

        /* Without a PdoEntryIndex, the rest is irrelevant */
        if (!pdo_entry->index)
            continue;

        pdo_entry->subindex = pval[i + entry_count];

        data_type_id = pval[i + 3*entry_count];
        if (!data_type_id)
            continue;

        for (pdo_entry->data_type = datatype_info;
                data_type_id != pdo_entry->data_type->id;
                pdo_entry->data_type++) {
            if (!pdo_entry->data_type->id) {
                snprintf(ctxt, sizeof(ctxt), "%s{3}(%u,4) = %i",
                        p_ctxt, i+1, (int_T)data_type_id);
                pr_error(slave, ctxt, NULL, __LINE__, "Unknown data type");
                return -1;
            }
        }
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
        pr_error(slave, NULL, p_ctxt, __LINE__,
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

        snprintf(ctxt, sizeof(ctxt), "%s{3}{%u}", p_ctxt, i+1);

        sm->pdo[i].sm = sm;
        if (get_sync_manager_pdo(slave,
                    sm->pdo + i, ctxt, mxGetCell(sm_cell, i)))
            return -1;
    }
    slave->pdo_count += pdo_count;

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
    size_t i, m,n;
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

    /***********************
     * Get SDO
     ***********************/
    if ((array = mxGetField( slave_config, 0, "sdo"))
            && mxGetNumberOfElements(array)) {
        const mxArray *cell, *valueCell;
        uint_T j;

        if (!mxIsCell(array)
                || !(m = mxGetM(array)) || (n = mxGetN(array)) != 4) {
            pr_error(slave, context, "sdo", __LINE__,
                    "SDO configuration is not a Mx4 cell array");
            return -1;
        }

        CHECK_CALLOC(slave->S, m,
                sizeof(struct sdo_config), slave->sdo_config);
        slave->sdo_config_end = slave->sdo_config + m;

        for (i = 0; i < m; i++) {
            real_T *pval;
            char_T ctxt[50];

            snprintf(ctxt, sizeof(ctxt), "%s.sdo{Row=%zu}", context, i+1);

            /* Index */
            RETURN_ON_ERROR (get_numeric_scalar(slave, ctxt, __LINE__,
                        i, array, &val));
            slave->sdo_config[i].index = val;

            /* SubIndex */
            RETURN_ON_ERROR (get_numeric_scalar(slave, ctxt, __LINE__,
                        i+m, array, &val));
            slave->sdo_config[i].subindex = val;

            /* Value */
            valueCell = mxGetCell(array, i + 3*m);
            if (valueCell && mxIsChar(valueCell)) {
                slave->sdo_config[i].value = mxGetNumberOfElements(valueCell);
                if (!slave->sdo_config[i].value) {
                    char_T sdo_ctxt[20];

                    snprintf(sdo_ctxt, sizeof(sdo_ctxt), "sdo{%zu,4}", i+1);
                    pr_error(slave, context, sdo_ctxt, __LINE__,
                            "SDO string value is empty");
                    return -1;
                }

                CHECK_CALLOC(slave->S, slave->sdo_config[i].value, 1,
                        slave->sdo_config[i].byte_array);

                slave->sdo_config[i].datatype = type_uint8_info;

                if (!mxGetString(valueCell, slave->sdo_config[i].byte_array,
                            slave->sdo_config[i].value)) {
                    char_T sdo_ctxt[20];

                    snprintf(sdo_ctxt, sizeof(sdo_ctxt), "sdo{%zu,4}", i+1);
                    pr_error(slave, context, sdo_ctxt, __LINE__,
                            "SDO string value is invalid");
                    return -1;
                }

                /* String does not need BitLen */
                continue;
            }

            /* BitLen */
            RETURN_ON_ERROR (get_numeric_scalar(slave, ctxt, __LINE__,
                        i + 2*m, array, &val));
            switch ((int)val) {
                case 8:
                    slave->sdo_config[i].datatype = type_uint8_info;
                    break;

                case 16:
                    slave->sdo_config[i].datatype = type_uint16_info;
                    break;

                case 32:
                    slave->sdo_config[i].datatype = type_uint32_info;
                    break;

                default:
                    {
                        char_T sdo_ctxt[20];

                        snprintf(sdo_ctxt, sizeof(sdo_ctxt), "sdo{%zu,3} = %i",
                                i+1, (int)val);
                        pr_error(slave, context, sdo_ctxt, __LINE__,
                                "SDO BitLen must be one of 8,16,32");
                    }
                    return -1;
            }

            if (!(cell = mxGetCell(array, i + 3*m))
                    || !(pval = mxGetPr(cell))) {
                char_T sdo_ctxt[30];

                snprintf(sdo_ctxt, sizeof(sdo_ctxt), "sdo{%zu,4}", i+1);
                pr_error(slave, context, sdo_ctxt, __LINE__,
                        "SDO BitLen not a valid number");
                return -1;
            }

            if ((n = mxGetNumberOfElements(cell)) == 1)
                slave->sdo_config[i].value = *pval;
            else {
                slave->sdo_config[i].value = n;
                CHECK_CALLOC(slave->S, n,
                        sizeof(uint8_T), slave->sdo_config[i].byte_array);

                for (j = 0; j < n; j++)
                    slave->sdo_config[i].byte_array[j] = pval[j];
            }
        }

        pr_debug(slave, NULL, "", 1, "SDO count %u\n", m);
        for (i  = 0; i < m; i++) {
            pr_debug(slave, NULL, "", 2,
                    "Index=#x%04X SubIndex=%u BitLen=%u Value=",
                    slave->sdo_config[i].index, slave->sdo_config[i].subindex,
                    slave->sdo_config[i].datatype->mant_bits);
            if (slave->sdo_config[i].byte_array) {
                for (j = 0; j < slave->sdo_config[i].value; j++)
                    pr_debug(slave, NULL, NULL, 0,
                            "%02x, ", slave->sdo_config[i].byte_array[j]);
            }
            else {
                pr_debug(slave, NULL, NULL, 0,
                        "%u", slave->sdo_config[i].value);
            }
            pr_debug(slave, NULL, NULL, 0, "\n");
        }
    }

    /***********************
     * Get SoE
     ***********************/
    if ((array = mxGetField( slave_config, 0, "soe"))
            && mxGetNumberOfElements(array)) {
        const mxArray *valueCell;
        uint_T j;

        if (!mxIsCell(array)
                || !(m = mxGetM(array)) || (n = mxGetN(array)) != 2) {
            pr_error(slave, context, "soe", __LINE__,
                    "SoE configuration is not a Mx2 cell array");
            return -1;
        }

        CHECK_CALLOC(slave->S, m,
                sizeof(struct soe_config), slave->soe_config);
        slave->soe_config_end = slave->soe_config + m;

        for (i = 0; i < m; i++) {
            char_T ctxt[20];
            real_T val, *pval;

            snprintf(ctxt, sizeof(ctxt), "%s.soe{%zu,1}", context, i+1);

            /* Index */
            RETURN_ON_ERROR (get_numeric_scalar(slave, ctxt, __LINE__,
                        i, array, &val));
            slave->soe_config[i].index = val;

            /* Value */
            valueCell = mxGetCell(array, i + m);
            if (!valueCell || !(n = mxGetNumberOfElements(valueCell))) {
                char_T ctxt[20];

                snprintf(ctxt, sizeof(ctxt), "soe{%zu,1}", i+1);
                pr_error(slave, context, ctxt, __LINE__,
                        "SoE value is empty");
                return -1;
            }

            CHECK_CALLOC(slave->S, n, sizeof(uint8_T),
                    slave->soe_config[i].octet_string);

            if (mxIsChar(valueCell)) {
                if (!mxGetString(valueCell,
                            slave->soe_config[i].octet_string, n)) {
                    char_T ctxt[20];

                    snprintf(ctxt, sizeof(ctxt), "soe{%zu,2}", i+1);
                    pr_error(slave, context, ctxt, __LINE__,
                            "SoE string value is invalid");
                    return -1;
                }
                slave->soe_config[i].octet_string_len = n - 1;
            }
            else if (mxIsDouble(valueCell)) {
                if (!(pval = mxGetPr(valueCell))) {
                    char_T ctxt[30];

                    snprintf(ctxt, sizeof(ctxt), "soe{%zu,2}", i+1);
                    pr_error(slave, context, ctxt, __LINE__,
                            "SoE value not a valid number");
                    return -1;
                }
                for (j = 0; j < n; j++)
                    slave->soe_config[i].octet_string[j] = pval[j];
                slave->soe_config[i].octet_string_len = n;
            }
            else {
                char_T ctxt[30];

                snprintf(ctxt, sizeof(ctxt), "soe{%zu,2}", i+1);
                pr_error(slave, context, ctxt, __LINE__,
                        "SoE value is neither a string nor numeric array");
                return -1;
            }
        }

        pr_debug(slave, NULL, "", 1, "SoE count %u\n", m);
        for (i  = 0; i < m; i++) {
            pr_debug(slave, NULL, "", 2,
                    "Index=#x%04X Value=", slave->soe_config[i].index);
            for (j = 0; j < slave->soe_config[i].octet_string_len; j++)
                pr_debug(slave, NULL, NULL, 0,
                        "%02x, ", slave->soe_config[i].octet_string[j]);
            pr_debug(slave, NULL, NULL, 0, "\n");
        }
    }

    /***********************
     * Get DC
     ***********************/
    if ((array = mxGetField( slave_config, 0, "dc"))
            && mxGetNumberOfElements(array)) {
        const real_T *val;

        if (!mxIsDouble(array)
                || mxGetNumberOfElements(array) != 5
                || !(val = mxGetPr(array))) {
            pr_error(slave, context, "dc", __LINE__,
                    "DC configuration is not a vector[5]");
            return -1;
        }

        slave->dc_opmode.assign_activate = val[0];
        slave->dc_opmode.sync0_cycle     = val[1];
        slave->dc_opmode.sync0_shift     = val[2];
        slave->dc_opmode.sync1_cycle     = val[3];
        slave->dc_opmode.sync1_shift     = val[4];

        pr_debug(slave, NULL, "", 1,
                "DC AssignActivate=%u "
                "Sync0Cycle=%u, Sync0Shift=%u "
                "Sync1Cycle=%u, Sync1Shift=%u\n",
                slave->dc_opmode.assign_activate,
                slave->dc_opmode.sync0_cycle,
                slave->dc_opmode.sync0_shift,
                slave->dc_opmode.sync1_cycle,
                slave->dc_opmode.sync1_shift);
    }

    /***********************
     * Get PDO's
     ***********************/
    if ((array = mxGetField( slave_config, 0, "pdo"))
            && (sm_count = mxGetNumberOfElements(array))) {

        if (!mxIsCell(array)) {
            pr_error(slave, context, "pdo", __LINE__,
                    "PDO configuration is not a Mx3 cell array");
            return -1;
        }

        CHECK_CALLOC(slave->S, sm_count,
                sizeof(struct sync_manager), slave->sync_manager);
        slave->sync_manager_end = slave->sync_manager + sm_count;

        for (i = 0; i < sm_count; i++) {
            char_T ctxt[30];
            snprintf(ctxt, sizeof(ctxt), "SLAVE_CONFIG.pdo{%u}", i+1);
            if (get_sync_manager(slave, slave->sync_manager + i,
                        ctxt, mxGetCell(array, i)))
                return -1;
        }

        pr_debug(slave, NULL, "", 1, "SM count %u\n", sm_count);
        for (i  = 0; i < sm_count; i++) {
            struct sync_manager *sm = slave->sync_manager + i;
            struct pdo *pdo;

            pr_debug(slave, NULL, "", 2,
                    "SMIndex=%u Dir=%s PdoCount=%i\n",
                    sm->index,
                    sm->direction == EC_SM_OUTPUT ? "OP (RxPdo)" : "IP (TxPdo)",
                    sm->pdo_end - sm->pdo);

            for (pdo = sm->pdo; pdo != sm->pdo_end; pdo++) {
                struct pdo_entry *entry;

                pr_debug(slave, NULL, "", 3,
                        "PdoIndex=#x%04X EntryCount=%u\n",
                        pdo->index, pdo->entry_end - pdo->entry);

                for (entry = pdo->entry; entry != pdo->entry_end; entry++) {
                    pr_debug(slave, NULL, "", 4,
                            "Index=#x%04X SubIndex=%u BitLen=%2u DT=%s\n",
                            entry->index,
                            entry->subindex,
                            entry->bitlen,
                            (entry->data_type
                             ?  entry->data_type->name : "none"));
                }
            }
        }
    }

    return 0;
}

/****************************************************************************
 * parameter specification is of the form:
 *    []
 *    | value
 *    | { 'string', value }
 * anything else is an error
 ****************************************************************************/
static int_T
get_port_parameter(struct ecat_slave *slave, const char_T *p_ctxt,
        const mxArray *port_spec, struct io_port *port, uint_T count,
        uint_T element, struct output_param **param, const char_T *name)
{
    const mxArray *spec = mxGetField(port_spec, element, name);
    char_T ctxt[30];
    real_T *val;
    int_T i;
    size_t n;

    if (!spec || !(n = mxGetNumberOfElements(spec)))
        return 0;

    CHECK_CALLOC(slave->S, 1, sizeof(struct output_param), *param);

    if (mxIsCell(spec)) {
        const mxArray *param_name;

        if (n != 2) {
            pr_error(slave, p_ctxt, name, __LINE__,
                    "Expected a cell array with 2 elements {'name', [value]}");
            return -1;
        }

        param_name = mxGetCell(spec, 0);
        if (!param_name || !mxIsChar(param_name)
                || !(n = mxGetNumberOfElements(param_name))) {
            snprintf(ctxt, sizeof(ctxt), "%s{1}", name);
            pr_error(slave, p_ctxt, ctxt, __LINE__,
                    "Parameter name not a valid string");
            return -1;
        }
        CHECK_CALLOC(slave->S, n+1, sizeof(char_T), (*param)->name);
        if (mxGetString(param_name, (*param)->name, n+1)) {
            snprintf(ctxt, sizeof(ctxt), "%s{1}", name);
            pr_error(slave, p_ctxt, ctxt, __LINE__,
                    "Parameter name not a valid string");
            return -1;
        }

        spec = mxGetCell(spec, 1);
        snprintf(ctxt, sizeof(ctxt), "%s{2}", name);
        if (!(n = mxGetNumberOfElements(spec))) {
            pr_error(slave, p_ctxt, ctxt, __LINE__,
                    "Parameter value vector is empty");
            return -1;
        }
        slave->runtime_param_count++;
    }
    else {
        snprintf(ctxt, sizeof(ctxt), "%s", name);
        slave->const_count += n;
    }

    if (n && mxIsDouble(spec) && (val = mxGetPr(spec))) {
        if (n != 1 && n != count) {
            pr_error(slave, p_ctxt, ctxt, __LINE__,
                    (count > 1
                     ? "Parameter value must have 1 or %u elements"
                     : "Parameter value must have 1 element only"),
                    count);
            return -1;
        }

        CHECK_CALLOC(slave->S, n, sizeof(real_T), (*param)->value);
        (*param)->count = n;

        for (i = 0; i < n; ++i)
            (*param)->value[i] = val[i];
    }
    else {
        pr_error(slave, p_ctxt, ctxt, __LINE__,
                 "Parameter value is not a valid vector");
        return -1;
    }

    pr_debug(slave, NULL, "", 3, "%s parameter: ", name);
    pr_debug(slave, NULL, NULL, 0,
            (*param)->name ? "name: '%s' :" : "constant: ", (*param)->name);
    for (i = 0; i < (*param)->count; ++i)
        pr_debug(slave, NULL, NULL, 0, "%f,", (*param)->value[i]);
    pr_debug(slave, NULL, NULL, 0, "\n");

    return 0;
}

/****************************************************************************/
static int_T
get_section_config(struct ecat_slave *slave, const char_T *section,
        enum sm_direction dir, struct io_port **port_begin)
{
    const mxArray * const io_spec = ssGetSFcnParam(slave->S, PORT_CONFIG);
    const mxArray *port_spec;
    const char_T *param = "PORT_CONFIG";
    uint_T count = 0;

    if (!io_spec) {
        pr_info(slave, NULL, NULL, 0,
                "No block input and output configuration spec "
                "was found\n");
        return 0;
    }

    port_spec = mxGetField(io_spec, 0, section);

    if (port_spec && (count = mxGetNumberOfElements(port_spec))) {
        struct io_port *port;
        size_t i;

        pr_debug(slave, NULL, NULL, 0,
                "--------------- Parsing %s IOSpec ------------\n", section);

        CHECK_CALLOC(slave->S, count, sizeof(struct io_port), port);
        *port_begin = port;

        pr_debug(slave, NULL, "", 1, "Port count %u\n", count);
        for (i = 0; i < count; i++, port++) {
            char_T ctxt[50];
            const mxArray *pdo_spec;
            real_T *val, real;
            size_t width, j;

            snprintf(ctxt, sizeof(ctxt), "%s.%s(%u)", param, section, i+1);

            pr_debug(slave, NULL, "", 2, "Port %u\n", i+1);

            /* Read the PDO data type if specified */
            real = 0;
            RETURN_ON_ERROR(get_numeric_field(slave, ctxt, __LINE__,
                        port_spec, i, 1, 1, 0, "pdo_data_type", &real));
            if (real > 0) {
                uint_T dt_id = real;
                for (port->data_type = datatype_info;
                        port->data_type->id != dt_id; port->data_type++) {
                    if (!port->data_type->id) {
                        pr_error(slave, ctxt, "pdo_data_type", __LINE__,
                                "Unknown data type %u", dt_id);
                        return -1;
                    }
                }
            }

            /* Read the endianness if specified */
            real = 0;
            RETURN_ON_ERROR(get_numeric_field(slave, ctxt, __LINE__,
                        port_spec, i, 1, 1, 0, "big_endian", &real));
            port->big_endian = real != 0.0;

            pdo_spec = mxGetField(port_spec, i, "pdo");
            if (!pdo_spec
                    || !(val = mxGetPr(pdo_spec))
                    || !(width = mxGetM(pdo_spec))
                    || mxGetN(pdo_spec) != 4) {
                pr_error(slave, ctxt, "pdo", __LINE__,
                        "Pdo specification is not a [Mx4] array");
                return -1;
            }

            CHECK_CALLOC(slave->S, width, sizeof(struct port_pdo), port->pdo);
            port->pdo_end = port->pdo + width;
            for (j = 0; j < width; j++) {
                struct sync_manager *sm;
                struct pdo *pdo;
                char_T element[20];

                sm = slave->sync_manager + (size_t)val[j];
                if (val[j] < 0
                        || sm >= slave->sync_manager_end) {
                    snprintf(element, sizeof(element), "pdo(%zu,1)", j+1);
                    pr_error(slave, ctxt, element, __LINE__,
                            "SyncManager row index %i out of range [0,%zu)",
                            (ssize_t)val[j],
                            slave->sync_manager_end - slave->sync_manager);
                    return -1;
                }

                if (sm->direction != dir) {
                    snprintf(element, sizeof(element), "pdo(%zu,1)", j+1);
                    pr_warn(slave, ctxt, element, __LINE__,
                            "SyncManager direction is incorrect\n");
                }

                pdo = sm->pdo + (size_t)val[j + width];
                if (val[j + width] < 0
                        || pdo >= sm->pdo_end) {
                    snprintf(element, sizeof(element), "pdo(%zu,2)", j+1);
                    pr_error(slave, ctxt, element, __LINE__,
                            "Pdo row index %i out of range [0,%zu)",
                            (ssize_t)val[j + width],
                            sm->pdo_end - sm->pdo);
                    return -1;
                }

                port->pdo[j].entry = pdo->entry + (size_t)val[j + 2*width];
                if (val[j + 2*width] < 0
                        || port->pdo[j].entry >= pdo->entry_end) {
                    snprintf(element, sizeof(element), "pdo(%zu,3)", j+1);
                    pr_error(slave, ctxt, element, __LINE__,
                            "PdoEntry row index %i out of range [0,%zu)",
                            (ssize_t)val[j + 2*width],
                            pdo->entry_end - pdo->entry);
                    return -1;
                }

                if (!port->pdo[j].entry->index) {
                    snprintf(element, sizeof(element), "pdo(%zu,3)", j+1);
                    pr_error(slave, ctxt, element, __LINE__,
                            "Cannot choose Pdo Entry #x0000");
                    return -1;
                }

                if (!port->data_type)
                    port->data_type = port->pdo[0].entry->data_type;

                if (port->pdo[0].entry->data_type
                        != port->pdo[j].entry->data_type) {
                    pr_error(slave, ctxt, "pdo", __LINE__,
                            "Cannot mix PDO data types on the same port");
                    return -1;
                }

                if (port->pdo[j].entry->data_type->mant_bits
                        % port->data_type->mant_bits) {
                    snprintf(element, sizeof(element), "pdo(%zu,3)", j+1);
                    pr_error(slave, ctxt, element, __LINE__,
                            "Data type specified for port (%s) does not "
                            "match the pdo's bit length (%u)",
                            port->data_type->name,
                            port->pdo[j].entry->data_type->mant_bits);
                    return -1;
                }

                port->pdo[j].element_idx = val[j + 3*width];
                if (val[j + 3*width] < 0
                        || (port->data_type->mant_bits
                            * (port->pdo[j].element_idx + 1)
                            > port->data_type->mant_bits)) {
                    snprintf(element, sizeof(element), "pdo(%zu,4)", j+1);
                    pr_error(slave, ctxt, element, __LINE__,
                            "Element index %i out of range [0,%zu)",
                            (ssize_t)val[j + 3*width],
                            (port->pdo[j].entry->bitlen
                             / port->pdo[j].entry->data_type->mant_bits));
                    return -1;
                }

                pr_debug(slave, NULL, "", 3,
                        "Pdo Entry #x%04X.%u element %s[%u]\n",
                        port->pdo[j].entry->index,
                        port->pdo[j].entry->subindex,
                        port->data_type->name,
                        port->pdo[j].element_idx);
            }

            RETURN_ON_ERROR (get_port_parameter(slave, ctxt, port_spec,
                        port, width, i, &port->gain, "gain"));
            RETURN_ON_ERROR (get_port_parameter(slave, ctxt, port_spec,
                        port, width, i, &port->offset, "offset"));

            if (dir == EC_SM_INPUT) {
                RETURN_ON_ERROR (get_port_parameter(slave, ctxt, port_spec,
                            port, width, i, &port->filter, "filter"));
                if (port->filter) {
                    port->filter_idx = slave->filter_count;
                    slave->filter_count += port->pdo_end - port->pdo;
                }
            }

            RETURN_ON_ERROR(get_numeric_field(slave, ctxt, __LINE__,
                        port_spec, i, 0, 1, 0,
                        "full_scale", &port->fullscale));

            if (port->gain || port->offset || port->filter
                    || port->fullscale) {
                /* Data type is always double if gain, offset, filter
                 * or fullscale is used */
                port->sl_port_data_type = SS_DOUBLE;

                /* Temporary storage will be needed for data */
                port->dwork_idx = ++slave->dwork_count;
            }
            else
                /* Input ports (with PDO dir = EC_SM_OUTPUT) have their port
                 * data types set dynamically, output port data types are
                 * fixed to PDO's data type */
                port->sl_port_data_type = dir == EC_SM_OUTPUT
                    ? DYNAMICALLY_TYPED : port->data_type->sl_type;
        }
    }

    return count;
}

/****************************************************************************/
static int_T
get_ioport_config(struct ecat_slave *slave)
{
    int_T n;

    /* Note: a simulink's output port corresponds to an input for
     * the master */
    RETURN_ON_ERROR ( n = get_section_config(slave,
                "output", EC_SM_INPUT, &slave->o_port));
    slave->o_port_end = slave->o_port + n;

    RETURN_ON_ERROR (n = get_section_config(slave,
                "input", EC_SM_OUTPUT, &slave->i_port));
    slave->i_port_end = slave->i_port + n;

    return 0;
}

/* This function is used to operate on the allocated memory with a generic
 * operator. This function is used to fix or release allocated memory.
 */
static void
slave_mem_op(struct ecat_slave *slave, void (*method)(void*))
{
    struct io_port *port;
    struct sync_manager *sm;
    struct pdo *pdo;
    struct soe_config *soe;
    struct sdo_config *sdo;

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

    for (port = slave->o_port; port != slave->o_port_end; port++) {
        if (port->gain) {
            (*method)(port->gain->name);
            (*method)(port->gain->value);
        }
        (*method)(port->gain);

        if (port->offset) {
            (*method)(port->offset->name);
            (*method)(port->offset->value);
        }
        (*method)(port->offset);

        if (port->filter) {
            (*method)(port->filter->name);
            (*method)(port->filter->value);
        }
        (*method)(port->filter);

        (*method)(port->pdo);
    }
    (*method)(slave->o_port);

    for (port = slave->i_port; port != slave->i_port_end; port++) {
        if (port->gain) {
            (*method)(port->gain->name);
            (*method)(port->gain->value);
        }
        (*method)(port->gain);

        if (port->offset) {
            (*method)(port->offset->name);
            (*method)(port->offset->value);
        }
        (*method)(port->offset);

        if (port->filter) {
            (*method)(port->filter->name);
            (*method)(port->filter->value);
        }
        (*method)(port->filter);

        (*method)(port->pdo);
    }
    (*method)(slave->i_port);

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
    uint_T i;
    struct ecat_slave *slave;
    struct io_port *port;

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

    if (get_slave_info(slave)) return;

    if (get_slave_config(slave)) return;

    if (get_ioport_config(slave)) return;

    /* Process input ports */
    if (!ssSetNumInputPorts(S, slave->i_port_end - slave->i_port))
        return;
    for (i = 0, port = slave->i_port; port != slave->i_port_end; port++, i++) {
        ssSetInputPortWidth   (S, i, DYNAMICALLY_SIZED);
        ssSetInputPortDataType(S, i, port->sl_port_data_type);
    }

    /* Process output ports */
    if (!ssSetNumOutputPorts(S, slave->o_port_end - slave->o_port))
        return;
    for (i = 0, port = slave->o_port; port != slave->o_port_end; port++, i++) {
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

    /* Check whether the data type is compatable with the PDO data type or is
     * SS_SINGLE or SS_DOUBLE */
    if (port->data_type->sl_type != id) {
        if (id != SS_DOUBLE && id != SS_SINGLE) {
            snprintf(msg, sizeof(msg),
                    "Trying to set data type of input port %i to %s,\n"
                    "whereas PDO has data type %s. Choose PDO data type\n"
                    "or SS_DOUBLE or SS_SINGLE",
                    p+1, ssGetDataTypeName(S, id), port->data_type->name);
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
    uint_T max_width = slave->i_port[port].pdo_end - slave->i_port[port].pdo;

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

/* This function is called when some ports are still DYNAMICALLY_SIZED
 * even after calling mdlSetInputPortWidth(). This occurs when input
 * ports are not connected. */
#define MDL_SET_DEFAULT_PORT_DIMENSION_INFO
static void mdlSetDefaultPortDimensionInfo(SimStruct *S)
{
    struct ecat_slave *slave = ssGetUserData(S);
    struct io_port *port;
    uint_T i = 0;

    if (!slave)
        return;

    for (port = slave->i_port; port != slave->i_port_end; port++, i++) {
        if (ssGetInputPortWidth(S, i) == DYNAMICALLY_SIZED) {
            ssSetInputPortWidth(S, i, port->pdo_end - port->pdo);
        }
    }
}

uint_T
create_runtime_parameter(SimStruct *S, int_T idx, struct output_param *p)
{
    ssParamRec rtp;
    if (!p || !p->name)
        return 0;

    rtp.name = p->name;
    rtp.nDimensions = 1;
    rtp.dimensions = &p->count;
    rtp.dataTypeId = SS_DOUBLE;
    rtp.complexSignal = 0;
    rtp.data = p->value;
    rtp.dataAttributes = NULL;
    rtp.nDlgParamIndices = 0;
    rtp.dlgParamIndices = NULL;
    rtp.transformed = RTPARAM_TRANSFORMED;
    rtp.outputAsMatrix = 0;
    ssSetRunTimeParamInfo(S, idx, &rtp);

    return 1;
}

#define MDL_SET_WORK_WIDTHS
static void mdlSetWorkWidths(SimStruct *S)
{
    struct ecat_slave *slave = ssGetUserData(S);
    struct io_port *port;
    uint_T param_idx = 0;

    if (!slave)
        return;

    ssSetNumRunTimeParams(S, slave->runtime_param_count);

    ssSetNumDWork(S, slave->dwork_count);

    for (port = slave->o_port; port != slave->o_port_end; port++) {
        param_idx += create_runtime_parameter (S, param_idx, port->gain);
        param_idx += create_runtime_parameter (S, param_idx, port->offset);
        param_idx += create_runtime_parameter (S, param_idx, port->filter);

        if (port->dwork_idx) {
            ssSetDWorkWidth(S, port->dwork_idx-1,
                    ssGetOutputPortWidth(S, port - slave->o_port));
            ssSetDWorkDataType(S, port->dwork_idx-1, port->data_type->sl_type);
            /*ssSetDWorkRTWIdentifier(S, port->dwork_idx-1, "KKKKK");*/
        }
    }

    for (port = slave->i_port; port != slave->i_port_end; port++) {
        param_idx += create_runtime_parameter (S, param_idx, port->gain);
        param_idx += create_runtime_parameter (S, param_idx, port->offset);

        if (port->dwork_idx) {
            ssSetDWorkWidth(S, port->dwork_idx-1,
                    ssGetInputPortWidth(S, port - slave->i_port));
            ssSetDWorkDataType(S, port->dwork_idx-1, port->data_type->sl_type);
            /*ssSetDWorkRTWIdentifier(S, port->dwork_idx-1, "KKKKK");*/
        }
    }
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
mdlRTWWritePort(struct ecat_slave *slave, struct io_port *port,
        int_T *param_idx, int_T *const_idx, real_T *constants)
{
    uint32_T (*pdo_spec)[3];
    int32_T param[3] = {-1, -1, -1};
    struct port_pdo *pdo;
    size_t i;

    if (port->gain) {
        if (!ssWriteRTWParamSettings(slave->S, 3,
                    SSWRITE_VALUE_QSTR, "Name",
                    (port->gain->name ? port->gain->name : ""),

                    SSWRITE_VALUE_DTYPE_NUM, "ConstIndex",
                    const_idx, DTINFO(SS_INT32, 0),

                    SSWRITE_VALUE_DTYPE_NUM, "NumConst",
                    &port->gain->count, DTINFO(SS_INT32, 0)))
            return -1;
        param[0] = (*param_idx)++;

        if (!port->gain->name) {
            memcpy(constants + *const_idx, port->gain->value,
                    port->gain->count * sizeof(real_T));
            *const_idx += port->gain->count;
        }
    }
    if (port->offset) {
        if (!ssWriteRTWParamSettings(slave->S, 3,
                    SSWRITE_VALUE_QSTR, "Name",
                    (port->offset->name ? port->offset->name : ""),

                    SSWRITE_VALUE_DTYPE_NUM, "ConstIndex",
                    const_idx, DTINFO(SS_INT32, 0),

                    SSWRITE_VALUE_DTYPE_NUM, "NumConst",
                    &port->offset->count, DTINFO(SS_INT32, 0)))
            return -1;
        param[1] = (*param_idx)++;

        if (!port->offset->name) {
            memcpy(constants + *const_idx, port->offset->value,
                    port->offset->count * sizeof(real_T));
            *const_idx += port->offset->count;
        }
    }
    if (port->filter) {
        if (!ssWriteRTWParamSettings(slave->S, 4,
                    SSWRITE_VALUE_QSTR, "Name",
                    (port->filter->name ? port->filter->name : ""),

                    SSWRITE_VALUE_DTYPE_NUM, "FilterIndex",
                    &port->filter_idx, DTINFO(SS_INT32, 0),

                    SSWRITE_VALUE_DTYPE_NUM, "ConstIndex",
                    const_idx, DTINFO(SS_INT32, 0),

                    SSWRITE_VALUE_DTYPE_NUM, "NumConst",
                    &port->filter->count, DTINFO(SS_INT32, 0)))
            return -1;
        param[2] = (*param_idx)++;

        if (!port->filter->name) {
            memcpy(constants + *const_idx, port->filter->value,
                    port->filter->count * sizeof(real_T));
            *const_idx += port->filter->count;
        }
    }

    pdo_spec = mxCalloc(port->pdo_end - port->pdo, sizeof(*pdo_spec));
    for (pdo = port->pdo, i = 0; pdo != port->pdo_end; pdo++, i++) {
        pdo_spec[i][0] = pdo->entry->index;
        pdo_spec[i][1] = pdo->entry->subindex;
        pdo_spec[i][2] = pdo->element_idx;
    }

    if (!ssWriteRTWParamSettings(slave->S, 5,
                SSWRITE_VALUE_DTYPE_2DMAT, "Pdo",
                pdo_spec, 3, i, DTINFO(SS_UINT32, 0),

                SSWRITE_VALUE_DTYPE_NUM, "PdoDataTypeId",
                &port->data_type->id, DTINFO(SS_UINT32,0),

                SSWRITE_VALUE_DTYPE_VECT, "Param",
                param, 3, DTINFO(SS_INT32,0),

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
    size_t sm_idx = 0;
    int_T param_idx = 0;
    int_T const_idx = 0;
    int32_T *config_idx;
    real_T *constants;
    size_t n;

    struct io_port *port;
    struct sdo_config *sdo;
    struct soe_config *soe;

    /* General assignments of array indices that form the basis for
     * the S-Function <-> TLC communication
     * DO NOT CHANGE THESE without updating the TLC ec_test2.tlc
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
                    &sdo->subindex, DTINFO(SS_UINT16, 0),

                    SSWRITE_VALUE_DTYPE_VECT, "ByteArray",
                    sdo->byte_array, sdo->value, DTINFO(SS_UINT8, 0)))
                return;
        }
        else {
            if (!ssWriteRTWParamSettings(slave->S, 4,
                    SSWRITE_VALUE_DTYPE_NUM, "Index",
                    &sdo->index, DTINFO(SS_UINT16, 0),

                    SSWRITE_VALUE_DTYPE_NUM, "SubIndex",
                    &sdo->subindex, DTINFO(SS_UINT16, 0),

                    SSWRITE_VALUE_DTYPE_NUM, "DataTypeId",
                    &sdo->datatype->id, DTINFO(SS_UINT32, 0),

                    SSWRITE_VALUE_DTYPE_NUM, "Value",
                    &sdo->value, DTINFO(SS_UINT32, 0)))
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
                soe->octet_string, soe->octet_string_len, DTINFO(SS_UINT8, 0)))
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

        struct sync_manager *sm;

        sync_manager = mxCalloc(sm_count, sizeof(*sync_manager));
        pdo_info = mxCalloc(slave->pdo_count, sizeof(*pdo_info));
        pdo_entry_info =
            mxCalloc(slave->pdo_entry_count, sizeof(*pdo_entry_info));

        for (sm = slave->sync_manager; sm != slave->sync_manager_end; sm++) {
            struct pdo *pdo;

            sync_manager[sm_idx][SM_Index] = sm->index;
            sync_manager[sm_idx][SM_Direction] =
                sm->direction == EC_SM_INPUT;
            sync_manager[sm_idx][SM_PdoCount] = 0;

            for (pdo = sm->pdo; pdo != sm->pdo_end; pdo++) {
                struct pdo_entry *pdo_entry;

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
            if (sync_manager[sm_idx][SM_PdoCount])
                sm_idx++;
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
            uint32_T opmode[5] = {
                slave->dc_opmode.assign_activate,
                slave->dc_opmode.sync0_cycle,
                slave->dc_opmode.sync0_shift,
                slave->dc_opmode.sync1_cycle,
                slave->dc_opmode.sync1_shift
            };

            if (!ssWriteRTWVectParam(S, "DcOpMode", opmode, SS_UINT32, 5))
                return;
        }

        mxFree(sync_manager);
        mxFree(pdo_info);
        mxFree(pdo_entry_info);
    }

    constants = mxCalloc(slave->const_count, sizeof(real_T));
    n = slave->o_port_end - slave->o_port;
    config_idx = mxCalloc(n, sizeof(*config_idx));
    for (port = slave->o_port, n = 0; port != slave->o_port_end; port++, n++) {
        if (mdlRTWWritePort(slave, port, &param_idx, &const_idx, constants))
            return;

        config_idx[n] = param_idx++;
    }
    if (!ssWriteRTWVectParam(S, "OutputPortIdx", config_idx, SS_INT32, n))
        return;
    mxFree (config_idx);

    n = slave->i_port_end - slave->i_port;
    config_idx = mxCalloc(n, sizeof(*config_idx));
    for (port = slave->i_port, n = 0; port != slave->i_port_end; port++, n++) {
        if (mdlRTWWritePort(slave, port, &param_idx, &const_idx, constants))
            return;

        config_idx[n] = param_idx++;
    }
    if (!ssWriteRTWVectParam(S, "InputPortIdx", config_idx, SS_INT32, n))
        return;
    mxFree (config_idx);

    if (!ssWriteRTWVectParam(S, "ConstVector", constants, SS_DOUBLE, const_idx))
        return;
    mxFree (constants);

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
