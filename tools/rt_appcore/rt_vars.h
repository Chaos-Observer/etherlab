
struct rt_var_space;

/* This is the first function to call before using this module. 
 * All subsequent calls must pass the return value to the function as a 
 * first argument 
 * Arguments: application: Name of the application
 *            numst      : number of sample times the application has
 *
 * Returns: Pointer to the variable space if successful
 *          NULL otherwise
 */
struct rt_var_space *rt_get_var_space(const char *application, 
        size_t numst );

/* When the application leaves, call this function */
void rt_clr_var_space(struct rt_var_space *rt_var_space);

/* Register a source variable. These variables are outputs from an 
 * application that destination variables can subscribe to. 
 *
 * Arguments: rt_var_space: pointer to variable space
 *            name: system wide unique variable name
 *            dtype: data type; one of bool_T, uint8_T, sin8_T ... sint32_T,
 *                              single_T, double_T
 *            nelem: number of elements in the vector
 *            st   : sample time of variable
 *                   
 * Returns: Pointer to data space where the application can write to 
 *          NULL on error
 */
void *rt_reg_var(struct rt_var_space *rt_var_space, 
        const char *name, const char *dtype, size_t nelem, int st);

/* Try to subscribe to a source variable. It is not an error if the 
 * variable does not exist yet - it may appear later with another application
 *
 * Arguments: rt_var_space: pointer to variable space
 *            name: variable name to subscribe to
 *            dtype: data type; one of bool_T, uint8_T, sin8_T ... sint32_T,
 *                              single_T, double_T
 *            dst: pointer where data is written to when calling rt_copy_vars
 *            nelem: number of elements in the vector
 *            st   : sample time of variable
 *                   
 * Returns:
 *      0       : No error
 *      -EINVAL : st is unknown
 *                dtype is unknown
 *      -ENOMEM : Out of memory
 */
int rt_get_var(struct rt_var_space *rt_var_space, 
        const char *name, const char *dtype, void *dst, size_t nelem, int st);

/* When the application has finished writing its outputs at the end of a 
 * calculation cycle, call this function so that the output variables are
 * copied into a separate buffer from where the variables are read again.
 *
 * Arguments: rt_var_space: pointer to variable space
 *            st: sample time to copy
 */
void rt_copy_buf(struct rt_var_space *rtv, int st);

/* Before the application starts its calculation cycle, call this function to
 * copy all input variables to the local space pointed to by *dst (passed
 * when calling rt_get_var()
 *
 * Arguments: rt_var_space: pointer to variable space
 *            st: sample time to copy
 */
void rt_copy_vars(struct rt_var_space *rtv, int st);
