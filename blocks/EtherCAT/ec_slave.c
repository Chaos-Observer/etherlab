/*
 * $RCSfile: el31xx.c,v $
 * $Revision$
 * $Date$
 *
 * SFunction to implement Beckhoff's series EL31xx of EtherCAT Analog Input 
 * Terminals
 *
 * Copyright (c) 2006, Richard Hacker
 * License: GPL
 */

#define S_FUNCTION_NAME  ec_slave
#define S_FUNCTION_LEVEL 2

#include "simstruc.h"

#define ECMASTER              mxGetScalar(ssGetSFcnParam(S,0))
#define ECDOMAIN              mxGetScalar(ssGetSFcnParam(S,1))
#define ECALIAS               mxGetScalar(ssGetSFcnParam(S,2))
#define ECPOSITION            mxGetScalar(ssGetSFcnParam(S,3))

#define PRODUCT_NAME                     (ssGetSFcnParam(S,4))
#define ECVENDOR              mxGetScalar(ssGetSFcnParam(S,5))
#define ECPRODUCT             mxGetScalar(ssGetSFcnParam(S,6))
#define REVISION              mxGetScalar(ssGetSFcnParam(S,7))

#define PDO_ENTRY                        (ssGetSFcnParam(S,8))
#define PDO_INFO                         (ssGetSFcnParam(S,9))
#define SDO_CONFIG                       (ssGetSFcnParam(S,10))

#define INPUT                            (ssGetSFcnParam(S,11))
#define OUTPUT                           (ssGetSFcnParam(S,12))

#define TSAMPLE               mxGetScalar(ssGetSFcnParam(S,13))
#define PARAM_COUNT                                        14

char errmsg[256];

struct ecat_slave {
    char *name;

        /* One record for every SDO configuration element:
         * 1: SDO Index
         * 2: SDO_Subindex
         * 3: DataType: SS_UINT8, SS_UINT16, SS_UINT32
         * 4: Value */
    uint32_T (*sdo_config)[4];
    uint_T   sdo_config_len;

    int32_T  (*pdo_entry)[3];
    uint_T   pdo_entry_len;

        /* 1 = Input, PdoIndex, start row in PdoEntry, Row Num */
    uint32_T (*pdo_info)[4];
    uint_T   pdo_info_len;

    uint_T port_width;
    uint_T unequal_port_widths;

    struct input_spec {
        uint_T pdo_data_type;
        uint_T port_data_type;
        struct {
            uint32_T pdo_info_idx;
            uint32_T pdo_entry_idx;
        } *map;

        /* Number of signals that makes up the pdo.
         * If pdo_bits == 0, then this is also the number of *map entries,
         * i.e. one PDO for every signal. 
         * If pdo_bits > 1, then the PDO is constructed by bitshifting 
         * max_port_width input signals. In this case, every input signal
         * occupies pdo_bits in the PDO data space. *map only has 1 element.
         */
        uint_T pdo_bits;
        uint_T max_port_width;

        uint_T port_width;      /* Signals entering the port */

        /* The following specs are needed to determine whether the input
         * value needs to be scaled before writing to the PDO. If 
         * pdo_bits != 0, the input is raw anyway and the following values
         * are ignored.
         * When raw == 0, no scaling is done, otherwise the value written
         * to the PDO is:
         * gain_count == 0:  PDO = input * pdo_full_scale
         * gain_count != 0:  PDO = input * gain_values * pdo_full_scale
         *
         * gain_values appears as an application parameter if supplied.
         */
        uint_T pdo_full_scale;
        char_T *gain_name;
        real_T *gain_values;
        int_T gain_count;
    } *input_spec;
    uint_T num_inputs;
    uint_T input_map_count;
    uint_T static_input_gain_count;

    /*
     * One record for every block output port:
     * PDO data type, 
     * Raw, ...       0 -> Block output is float, 1 -> Raw signal
     * GainSpec, OffsetSpec, FilterSpec, ...  0 -> None
     * 1 -> Only one value
     * 2 -> One value for every element
    uint32_T (*output_port_spec)[5];
     */
    struct output_spec {
        uint_T pdo_bits;
        uint_T pdo_data_type;
        uint_T port_data_type;
        struct {
            uint32_T pdo_info_idx;
            uint32_T pdo_entry_idx;
        } *map;
        uint_T port_width;

        /* The following specs are needed when raw == 0 */
        uint_T pdo_full_scale;
        char_T *gain_name;
        char_T *offset_name;
        char_T *filter_name;
        real_T *gain_values;
        real_T *offset_values;
        real_T *filter_values;
        int_T gain_count;
        int_T offset_count;
        int_T filter_count;
    } *output_spec;
    uint_T num_outputs;
    uint_T output_map_count;
    uint_T static_output_gain_count;
    uint_T static_output_offset_count;
    uint_T static_output_filter_count;

    uint_T output_filter;
    uint_T runtime_param_count;
};

const char_T* 
getDataType(int_T width, uint_T *data_type)
{
    switch (width) {
        case 1:
            *data_type = SS_BOOLEAN;
            break;
        case 8:
            *data_type = SS_UINT8;
            break;
        case -8:
            *data_type = SS_INT8;
            break;
        case 16:
            *data_type = SS_UINT16;
            break;
        case -16:
            *data_type = SS_INT16;
            break;
        case 32:
            *data_type = SS_UINT32;
            break;
        case -32:
            *data_type = SS_INT32;
            break;
        default:
            return "Unknown data type specified. Allowed are: "
                    "1, -8, 8, -16, 16, -32, 32.";
            break;
    }

    return NULL;
}

const char_T* 
getPdoBitLenAndDir(uint_T *bit_len, uint_T *dir, 
        const struct ecat_slave *slave, uint_T pdo_info_idx, 
        uint_T pdo_entry_idx) 
{
    if (pdo_info_idx >= slave->pdo_info_len) {
        return "pdo_info_idx exceeds matrix dimensions.";
    }

    if (pdo_entry_idx >= slave->pdo_info[pdo_info_idx][3]) {
        return "pdo_entry_idx exceeds bounds defined in pdo_info.";
    }

    /* The pdo_entry_idx passed as an argument to this function still has
     * to be offset by the PDO Entry base as specified in PDO Info */
    pdo_entry_idx += slave->pdo_info[pdo_info_idx][2];
    if (pdo_entry_idx >= slave->pdo_entry_len) {
        return "pdo_entry_idx exceeds matrix dimensions.";
    }

    *bit_len = slave->pdo_entry[pdo_entry_idx][2];
    *dir = slave->pdo_info[pdo_info_idx][0];
    return NULL;
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

    len = mxGetNumberOfElements(plhs) + 1;
    *name = mxMalloc(len);
    if (!*name)
        return ssGetErrorStatus(S);

    if (mxGetString(plhs, *name, len))
        return field;

    return NULL;
}

const char_T* 
get_input_config(SimStruct *S, struct input_spec *input_spec, 
        struct ecat_slave *slave)
{
    uint_T port = input_spec - slave->input_spec;
    const mxArray* input = INPUT;
    const mxArray* pdo_map = mxGetField(input, port, "pdo_map");
    const mxArray* pdo_bits = mxGetField(input, port, "pdo_bits");
    const mxArray* pdo_dtype = mxGetField(input, port, "pdo_data_type");
    const mxArray* pdo_full_scale = mxGetField(input, port, "pdo_full_scale");
    const char_T* errfield = NULL;
    const char_T* err;
    real_T *pdo_info_idx;
    real_T *pdo_entry_idx;
    uint_T pdo_bit_len = 0;
    uint_T i;
    uint_T map_count;

    /* Check that the required fields for input port are specified */
    if (!pdo_map) {
        errfield = "pdo_map";
    } else if (!pdo_dtype) {
        errfield = "pdo_data_type";
    }
    if (errfield) {
        snprintf(errmsg, sizeof(errmsg),
                "Required field '%s' in structure for input port %u "
                "not defined.", errfield, port+1);
        return errmsg;
    }

    /* Make sure matrices have the right dimensions */
    if (mxGetN(pdo_map) != 2 || !mxIsDouble(pdo_map)) {
        /* PDO map must have 2 columns */
        snprintf(errmsg, sizeof(errmsg),
                "Matrix specified for input(%u).%s must be a numeric array "
                "with 2 columns.",
                port+1, "pdo_map");
        return errmsg;
    }

    if (!mxIsDouble(pdo_dtype)) {
        snprintf(errmsg, sizeof(errmsg),
                "Value specified for input(%u).%s must be a numeric value.",
                port+1, "pdo_data_type");
        return errmsg;
    }

    if ((err = getDataType( mxGetScalar(pdo_dtype), 
                    &input_spec->pdo_data_type))) {
        snprintf(errmsg, sizeof(errmsg),
                "Error occurred while determinig PDO data type "
                "for input %u: %s", port+1, err);
        return errmsg;
    }
    if (input_spec->pdo_data_type != SS_UINT8 
            && input_spec->pdo_data_type != SS_INT8
            && input_spec->pdo_data_type != SS_UINT16
            && input_spec->pdo_data_type != SS_INT16
            && input_spec->pdo_data_type != SS_UINT32
            && input_spec->pdo_data_type != SS_INT32 
            ) {
        snprintf(errmsg, sizeof(errmsg),
                "PDO data type for input %u not one of the known types:\n"
                "8, -8, 16, -16, 32, -32\n",
                port+1);
        return errmsg;
    }

    map_count = mxGetM(pdo_map);

    /* pdo_info_idx is the first column of pdo_map;
     * pdo_entry_idx is the second column of pdo_map */
    pdo_info_idx = mxGetPr(pdo_map);
    pdo_entry_idx = pdo_info_idx + map_count;

    input_spec->map = mxCalloc(map_count, sizeof(*input_spec->map));
    if (!input_spec->map)
        return ssGetErrorStatus(S);
    slave->input_map_count += map_count;

    for (i = 0; i < map_count; i++) {
        uint_T bl, dir;

        if ((err = getPdoBitLenAndDir(&bl, &dir, slave,  
                        pdo_info_idx[i], pdo_entry_idx[i]))) {
            snprintf(errmsg, sizeof(errmsg),
                    "Error occurred while determinig data type "
                    "of a PDO[%u] for input %u: %s", i, port+1, err);
            return errmsg;
        }

        if (dir != 1) {
            snprintf(errmsg, sizeof(errmsg),
                    "PDO direction has wrong direction for input %u",
                    port+1);
            return errmsg;
        }

        if (pdo_bit_len) {
            if (bl != pdo_bit_len) {
                snprintf(errmsg, sizeof(errmsg),
                        "Cannot mix different PDO bit lengths on "
                        "input %u.", port+1);
                return errmsg;
            }
        }
        else {
            pdo_bit_len = bl;
        }

        input_spec->map[i].pdo_info_idx = pdo_info_idx[i];
        input_spec->map[i].pdo_entry_idx = pdo_entry_idx[i];
    }

    /* When specifying pdo_bits, only one pdo_map is allowed, the
     * port width is determined by the first element of pdo_bits and
     * the number of bits that is operated on is the second element thereof
     */
    if (pdo_bits) {
        const mxArray* channels = mxGetField(pdo_bits, 0, "channels");
        const mxArray* bitcount = mxGetField(pdo_bits, 0, "bitcount");
        const mxArray* datatype = mxGetField(pdo_bits, 0, "port_datatype");

        if (!mxIsStruct(pdo_bits) || !channels || !bitcount || !datatype
                || !mxIsDouble(channels) || !mxIsDouble(bitcount) 
                || !mxIsDouble(datatype)) {
            snprintf(errmsg, sizeof(errmsg),
                    "SFunction parameter input(%u).pdo_bits must be "
                    "specified as a structure with the "
                    "scalar numeric fields:\n"
                    "'channels' 'bitcount' and 'port_datatype'", port+1);
            return errmsg;
        }

        if (map_count > 1) {
            snprintf(errmsg, sizeof(errmsg),
                    "Only allowed to specify one pdo_map "
                    "when using field 'pdo_bits' for input %u.",
                    port+1);
            return errmsg;
        }

        input_spec->max_port_width = mxGetScalar(channels);
        input_spec->pdo_bits = mxGetScalar(bitcount);
        if ((err = getDataType( mxGetScalar(datatype), 
                        &input_spec->port_data_type))) {
            snprintf(errmsg, sizeof(errmsg),
                    "Error occurred while determinig PDO data type "
                    "for input(%u).pdo_bits.datatype: %s", port+1, err);
            return errmsg;
        }
    }
    else {
        input_spec->max_port_width = map_count;
        input_spec->port_data_type = input_spec->pdo_data_type;
    }

    if (pdo_bits || !pdo_full_scale)
        return NULL;

    /* Output is double. This means that the source value must be scaled 
     * so that the source value range is mapped to [0..1] */
    if (!mxIsDouble(pdo_full_scale)) {
        snprintf(errmsg, sizeof(errmsg),
                "Specified field input(%u).%s must be a numeric scalar.",
                port+1, "pdo_full_scale");
        return errmsg;
    }
    input_spec->pdo_full_scale = mxGetScalar(pdo_full_scale);

    /* Check that the vector length is either 1 or port_width
     * for the fields 'gain' if they are specified */
    errfield = get_parameter_values(S, input, port, "gain",
            &input_spec->gain_count, map_count, 
            &input_spec->gain_values);
    if (errfield) {
        snprintf(errmsg, sizeof(errmsg),
                "Vector for input(%u).%s is not a valid numeric array "
                "with 1 or %u elements.", 
                port+1, errfield, input_spec->port_width);
        return errmsg;
    }

    if (!input_spec->gain_count) {
        snprintf(errmsg, sizeof(errmsg),
                "Required field '%s' in structure for input port %u "
                "not defined.", "gain", port+1);
        return errmsg;
    }

    /* Get the names for the parameters */
    if (input_spec->gain_count) {
        errfield = get_parameter_name(S, input, port, "gain_name",
                &input_spec->gain_name);
    }
    if (errfield) {
        snprintf(errmsg, sizeof(errmsg),
                "No valid string for input(%u).%s supplied.", 
                port+1, errfield);
        return errmsg;
    }

    return NULL;
}

const char_T* 
get_output_config(SimStruct *S, struct output_spec *output_spec, 
        struct ecat_slave *slave)
{
    uint_T port = output_spec - slave->output_spec;
    const mxArray* output = OUTPUT;
    const mxArray* pdo_map = mxGetField(output, port, "pdo_map");
    const mxArray* pdo_dtype = mxGetField(output, port, "pdo_data_type");
    const mxArray* pdo_bits = mxGetField(output, port, "pdo_bits");
    const mxArray* pdo_full_scale = mxGetField(output, port, "pdo_full_scale");
    const char_T* errfield = NULL;
    const char_T* err;
    real_T *pdo_info_idx;
    real_T *pdo_entry_idx;
    uint_T pdo_bit_len = 0;
    uint_T map_count;
    uint_T i;

    /* Check that the required fields for output port are specified */
    if (!pdo_map) {
        errfield = "pdo_map";
    } else if (!pdo_dtype) {
        errfield = "pdo_data_type";
    }
    if (errfield) {
        snprintf(errmsg, sizeof(errmsg),
                "Required field '%s' in structure for output port %u "
                "not defined.", errfield, port+1);
        return errmsg;
    }

    /* Make sure matrices have the right dimensions */
    if (mxGetN(pdo_map) != 2 || !mxIsDouble(pdo_map)) {
        /* PDO map must have 2 columns */
        snprintf(errmsg, sizeof(errmsg),
                "Matrix specified for output(%u).%s must be a numeric array "
                "with 2 columns.",
                port+1, "pdo_map");
        return errmsg;
    }

    if (!mxIsDouble(pdo_dtype)) {
        snprintf(errmsg, sizeof(errmsg),
                "Value specified for output(%u).%s must be a numeric value.",
                port+1, "pdo_data_type");
        return errmsg;
    }

    if ((err = getDataType( mxGetScalar(pdo_dtype), 
                    &output_spec->pdo_data_type))) {
        snprintf(errmsg, sizeof(errmsg),
                "Error occurred while determinig PDO data type "
                "for output %u: %s", port+1, err);
        return errmsg;
    }
    if (output_spec->pdo_data_type != SS_UINT8 
            && output_spec->pdo_data_type != SS_INT8
            && output_spec->pdo_data_type != SS_UINT16
            && output_spec->pdo_data_type != SS_INT16
            && output_spec->pdo_data_type != SS_UINT32
            && output_spec->pdo_data_type != SS_INT32 
            ) {
        snprintf(errmsg, sizeof(errmsg),
                "PDO data type for output %u not one of the known types:\n"
                "8, -8, 16, -16, 32, -32\n",
                port+1);
        return errmsg;
    }

    map_count = mxGetM(pdo_map);

    pdo_info_idx = mxGetPr(pdo_map);
    pdo_entry_idx = pdo_info_idx + map_count;

    output_spec->map = mxCalloc(map_count, sizeof(*output_spec->map));
    if (!output_spec->map)
        return ssGetErrorStatus(S);
    slave->output_map_count += map_count;

    for (i = 0; i < map_count; i++) {
        uint_T bl, dir;

        if ((err = getPdoBitLenAndDir(&bl, &dir, slave,  
                        pdo_info_idx[i], pdo_entry_idx[i]))) {
            snprintf(errmsg, sizeof(errmsg),
                    "Error occurred while determinig data type "
                    "of a PDO[%u] for output %u: %s", i, port+1, err);
            return errmsg;
        }

        if (dir != 0) {
            snprintf(errmsg, sizeof(errmsg),
                    "PDO direction has wrong direction for output %u",
                    port+1);
            return errmsg;
        }

        if (pdo_bit_len) {
            if (bl != pdo_bit_len) {
                snprintf(errmsg, sizeof(errmsg),
                        "Cannot mix different PDO bit lengths on "
                        "output %u.", port+1);
                return errmsg;
            }
        }
        else {
            pdo_bit_len = bl;
        }

        output_spec->map[i].pdo_info_idx = pdo_info_idx[i];
        output_spec->map[i].pdo_entry_idx = pdo_entry_idx[i];
    }

    if (pdo_bits) {
        const mxArray* channels = mxGetField(pdo_bits, 0, "channels");
        const mxArray* bitcount = mxGetField(pdo_bits, 0, "bitcount");
        const mxArray* datatype = mxGetField(pdo_bits, 0, "port_datatype");

        if (!mxIsStruct(pdo_bits) || !channels || !bitcount || !datatype
                || !mxIsDouble(channels) || !mxIsDouble(bitcount) 
                || !mxIsDouble(datatype)) {
            snprintf(errmsg, sizeof(errmsg),
                    "SFunction parameter output(%u).pdo_bits must be "
                    "specified as a structure with the "
                    "scalar numeric fields:\n"
                    "'channels' 'bitcount' and 'port_datatype'", port+1);
            return errmsg;
        }

        if (map_count > 1) {
            snprintf(errmsg, sizeof(errmsg),
                    "Only allowed to specify one pdo_map "
                    "when using field 'pdo_bits' for output %u.",
                    port+1);
            return errmsg;
        }

        output_spec->port_width = mxGetScalar(channels);
        output_spec->pdo_bits = mxGetScalar(bitcount);
        if ((err = getDataType( mxGetScalar(datatype), 
                        &output_spec->port_data_type))) {
            snprintf(errmsg, sizeof(errmsg),
                    "Error occurred while determinig PDO data type "
                    "for output(%u).pdo_bits.datatype: %s", port+1, err);
            return errmsg;
        }
    }
    else {
        output_spec->port_width = map_count;
        output_spec->port_data_type = output_spec->pdo_data_type;
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

    if (pdo_bits || !pdo_full_scale)
        return NULL;

    /* Output is double. This means that the source value must be scaled 
     * so that the source value range is mapped to [0..1] */
    if (!mxIsDouble(pdo_full_scale)) {
        snprintf(errmsg, sizeof(errmsg),
                "Specified field output(%u).%s must be a numeric scalar.",
                port+1, "pdo_full_scale");
        return errmsg;
    }
    output_spec->pdo_full_scale = mxGetScalar(pdo_full_scale);

    /* Check that the vector length is either 1 or port_width
     * for the fields 'gain', 'offset' and 'filter' if they are specified */
    do {
        errfield = get_parameter_values(S, output, port, "gain",
            &output_spec->gain_count, output_spec->port_width, 
            &output_spec->gain_values);
        if (errfield) break;

        errfield = get_parameter_values(S, output, port, "offset",
            &output_spec->offset_count, output_spec->port_width, 
            &output_spec->offset_values);
        if (errfield) break;

        errfield = get_parameter_values(S, output, port, "filter",
            &output_spec->filter_count, output_spec->port_width, 
            &output_spec->filter_values);
        if (errfield) break;

    } while(0);
    if (errfield) {
        snprintf(errmsg, sizeof(errmsg),
                "Vector for output(%u).%s is not a valid numeric array "
                "with 1 or %u elements.", 
                port+1, errfield, output_spec->port_width);
        return errmsg;
    }

    /* Get the names for the parameters */
    do {
        if (output_spec->gain_count) {
            errfield = get_parameter_name(S, output, port, "gain_name",
                    &output_spec->gain_name);
            if (errfield) break;
        }

        if (output_spec->offset_count) {
            errfield = get_parameter_name(S, output, port, "offset_name",
                    &output_spec->offset_name);
            if (errfield) break;
        }

        if (output_spec->filter_count) {
            errfield = get_parameter_name(S, output, port, "filter_name",
                    &output_spec->filter_name);
            if (errfield) break;
        }
    } while(0);
    if (errfield) {
        snprintf(errmsg, sizeof(errmsg),
                "No valid string for output(%u).%s supplied.", 
                port+1, errfield);
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
    real_T *pdo_info_value, *pdo_entry_value, *sdo_config_value;
    struct output_spec *output_spec;
    struct input_spec *input_spec;
    
    ssSetNumSFcnParams(S, PARAM_COUNT);  /* Number of expected parameters */
    if (ssGetNumSFcnParams(S) != ssGetSFcnParamsCount(S)) {
        /* Return if number of expected != number of actual parameters */
        return;
    }

    for( i = 0; i < PARAM_COUNT; i++) 
        ssSetSFcnParamTunable(S,i,SS_PRM_NOT_TUNABLE);

    /* allocate memory for slave structure */
    if (!(slave = mxCalloc(1,sizeof(*slave)))) {
        ssSetErrorStatus(S, "Could not allocate memory");
        return;
    }
    ssSetUserData(S,slave);

    /* Fetch the PDO Entries from Simulink. This matrix has 3 columns, and 
     * describes which objects can be mapped */
    slave->pdo_entry_len = mxGetM(PDO_ENTRY);
    if (slave->pdo_entry_len) {
        if (!mxIsDouble(PDO_ENTRY) || mxGetN(PDO_ENTRY) != 3 
                || !slave->pdo_entry_len) {
            ssSetErrorStatus(S, "SFunction parameter pdo_entry must be "
                    "a n-by-3 numeric matrix");
            return;
        }

        slave->pdo_entry = 
            mxCalloc(slave->pdo_entry_len, sizeof(*slave->pdo_entry));
        if (!slave->pdo_entry)
            return;
        pdo_entry_value = mxGetPr(PDO_ENTRY);
        for (i = 0; i < slave->pdo_entry_len; i++) {
            for (j = 0; j < 3; j++) {
                /* Note: mxArrays are stored column major! */
                slave->pdo_entry[i][j] = 
                    pdo_entry_value[i + j * slave->pdo_entry_len];
            }
        }
    }

    /* Fetch the PDO Info from Simulink. This matrix has 4 columns.
     * It describes the PDO mapping */
    slave->pdo_info_len = mxGetM(PDO_INFO);
    if (slave->pdo_info_len) {
        if (!mxIsDouble(PDO_INFO) || mxGetN(PDO_INFO) != 4
                || !slave->pdo_info_len) {
            ssSetErrorStatus(S, "SFunction parameter pdo_info must be "
                    "a n-by-4 numeric matrix");
            return;
        }
        slave->pdo_info = 
            mxCalloc(slave->pdo_info_len, sizeof(*slave->pdo_info));
        pdo_info_value = mxGetPr(PDO_INFO);
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
        if (!mxIsNumeric(SDO_CONFIG) || mxGetN(SDO_CONFIG) != 4) {
            ssSetErrorStatus(S, "SFunction parameter sdo_config must be "
                    "a n-by-4 numeric matrix");
            return;
        }
        slave->sdo_config = mxCalloc(slave->sdo_config_len,
                    sizeof(*slave->sdo_config));
        sdo_config_value = mxGetPr(SDO_CONFIG);
        for (i = 0; i < slave->sdo_config_len; i++) {
            const char_T *err;

            for (j = 0; j < 4; j++) {
                /* Note: mxArrays are stored column major! */
                slave->sdo_config[i][j] = 
                    sdo_config_value[i + j * slave->sdo_config_len];
            }
            if ((err = getDataType( slave->sdo_config[i][2], 
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

            if ((err = get_input_config(S, input_spec, slave))) {
                ssSetErrorStatus(S, err);
                return;
            }

            ssSetInputPortWidth(S, port, DYNAMICALLY_SIZED);
            if (input_spec->pdo_full_scale) {
                ssSetInputPortDataType(S, port, DYNAMICALLY_TYPED);
            }
            else {
                ssSetInputPortDataType(S, port, input_spec->port_data_type);
            }

            if (input_spec->gain_name) {
                slave->runtime_param_count += input_spec->gain_count;
            } else {
                slave->static_input_gain_count += input_spec->gain_count;
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

            if ((err = get_output_config(S, output_spec, slave))) {
                ssSetErrorStatus(S, err);
                return;
            }

            /* port width is determined by now. Set it */
            ssSetOutputPortWidth(S, port, output_spec->port_width);

            if (output_spec->pdo_full_scale) {
                /* Double output */
                ssSetOutputPortDataType(S, port, SS_DOUBLE);
            }
            else {
                /* Raw output. */
                ssSetOutputPortDataType(S, port, 
                        output_spec->port_data_type);
            }

            if (output_spec->gain_name) {
                slave->runtime_param_count += output_spec->gain_count;
            } else {
                slave->static_output_gain_count += output_spec->gain_count;
            }

            if (output_spec->offset_name) {
                slave->runtime_param_count += output_spec->offset_count;
            } else {
                slave->static_output_offset_count += output_spec->offset_count;
            }

            if (output_spec->filter_name) {
                slave->runtime_param_count += output_spec->filter_count;
            } else {
                slave->static_output_filter_count += output_spec->filter_count;
            }

            slave->output_filter += output_spec->filter_count;
        }
    }

    ssSetNumSampleTimes(S, 1);
    ssSetNumPWork(S, slave->output_map_count + slave->input_map_count);
    if (slave->output_filter) {
        if (TSAMPLE) {
            ssSetNumDiscStates(S, slave->output_filter);
        } else {
            ssSetNumContStates(S, slave->output_filter);
        }
    }

    mexMakeMemoryPersistent(slave);
    mexMakeMemoryPersistent(slave->pdo_entry);
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
    struct input_spec *input_spec = &slave->input_spec[port];

    if (width > input_spec->max_port_width) {
        snprintf(errmsg, sizeof(errmsg),
                "Maximum port width for input port %u is %u.\n"
                "Tried to set it to %i.", 
                port+1, input_spec->max_port_width, width);
        ssSetErrorStatus(S, errmsg);
        return;
    }

    input_spec->port_width = width;
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
    struct input_spec *input_spec;
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
    struct output_spec *output_spec;
    struct input_spec *input_spec;
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
    struct output_spec *output_spec;
    struct input_spec *input_spec;

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
    mxFree(slave->pdo_entry);
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
    uint_T revision = REVISION;
    char_T *product_name;
    uint_T port, i;
    uint_T len;
    uint32_T map_idx;
    struct output_spec *output_spec;
    struct input_spec *input_spec;
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

        input_port_spec[0][port] = input_spec->pdo_data_type;
        input_port_spec[1][port] = input_spec->pdo_bits;
        input_port_spec[2][port] = input_spec->pdo_full_scale;
        input_port_spec[3][port] = input_spec->gain_count;
        input_gain_name_ptr[port] = input_spec->gain_count
            ? input_spec->gain_name : "";

        for (i = 0; !input_spec->gain_name && i < input_spec->gain_count;
                i++) {
            input_gain[gain_idx++] = input_spec->gain_values[i];
        }

        for (i = 0; 
                i < (input_spec->pdo_bits 
                    ? 1 : input_spec->max_port_width);
                i++, map_idx++) {
            input_map[0][map_idx] = input_spec->map[i].pdo_info_idx;
            input_map[1][map_idx] = input_spec->map[i].pdo_entry_idx;
        }
    }
    input_gain_name_ptr[slave->num_inputs] = NULL;
    input_gain_str = createStrVect(input_gain_name_ptr);

    gain_idx = offset_idx = filter_idx = map_idx = 0;
    for (output_spec = slave->output_spec, port = 0; 
            output_spec != &slave->output_spec[slave->num_outputs]; 
            output_spec++, port++) {

        output_port_spec[0][port] = output_spec->pdo_data_type;
        output_port_spec[1][port] = output_spec->pdo_bits;
        output_port_spec[2][port] = output_spec->pdo_full_scale;
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

        for (i = 0; i < (output_spec->pdo_bits
                    ? 1 : output_spec->port_width); 
                i++, map_idx++) {
            output_map[0][map_idx] = output_spec->map[i].pdo_info_idx;
            output_map[1][map_idx] = output_spec->map[i].pdo_entry_idx;
        }
    }
    output_gain_name_ptr[slave->num_outputs] = NULL;
    output_offset_name_ptr[slave->num_outputs] = NULL;
    output_filter_name_ptr[slave->num_outputs] = NULL;
    output_gain_str = createStrVect(output_gain_name_ptr);
    output_offset_str = createStrVect(output_offset_name_ptr);
    output_filter_str = createStrVect(output_filter_name_ptr);

    len = mxGetN(PRODUCT_NAME) + 1;
    product_name = mxCalloc(len, 1);
    if (mxGetString(PRODUCT_NAME, product_name, len)) {
        ssSetErrorStatus(S, "Invalid product name parameter supplied.");
        return;
    }

    if (!ssWriteRTWScalarParam(S, "MasterId", &master, SS_UINT32))
        return;
    if (!ssWriteRTWScalarParam(S, "DomainId", &domain, SS_UINT32))
        return;
    if (!ssWriteRTWScalarParam(S, "SlaveAlias", &alias, SS_UINT32))
        return;
    if (!ssWriteRTWScalarParam(S, "SlavePosition", &position, SS_UINT32))
        return;

    if (!ssWriteRTWStrParam(S, "ProductName", product_name))
        return;
    if (!ssWriteRTWScalarParam(S, "VendorId", &vendor, SS_UINT32))
        return;
    if (!ssWriteRTWScalarParam(S, "ProductCode", &product, SS_UINT32))
        return;
    if (!ssWriteRTWScalarParam(S, "ProductRevision", &revision, SS_UINT32))
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
    { /* Transpose slave->pdo_entry */
        uint32_T pdo_entry[3][slave->pdo_entry_len];
        uint_T i, j;

        for (i = 0; i < slave->pdo_entry_len; i++) {
            for (j = 0; j < 3; j++) {
                pdo_entry[j][i] = slave->pdo_entry[i][j];
            }
        }

        if (!ssWriteRTW2dMatParam(S, "PdoEntryInfo", pdo_entry, 
                    SS_UINT32, slave->pdo_entry_len, 3))
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

    if (slave->input_map_count && slave->output_map_count) {
        if (!ssWriteRTWWorkVect(S, "PWork", 2, 
                    "DstPtr", slave->input_map_count,
                    "SrcPtr", slave->output_map_count))
            return;
    }
    else if (slave->input_map_count) {
        if (!ssWriteRTWWorkVect(S, "PWork", 1, 
                    "DstPtr", slave->input_map_count))
            return;
    }
    else if (slave->output_map_count) {
        if (!ssWriteRTWWorkVect(S, "PWork", 1, 
                    "SrcPtr", slave->output_map_count))
            return;
    }
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
