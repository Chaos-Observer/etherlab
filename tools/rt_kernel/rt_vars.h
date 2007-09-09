typedef enum {
    bool_T,
    sint8_T, uint8_T,
    sint16_T, uint16_T,
    sint32_T, uint32_T,
    sint64_T, uint64_T,
    single_T, double_T,
} dtype_t;

typedef int (*convert_t)(void);

struct rt_var_space;

void *rt_reg_var(struct rt_var_space *rt_var_space, 
        const char *name, const char *dtype, size_t nelem, int st);
void *rt_get_var(struct rt_var_space *rt_var_space, 
        const char *name, const char *dtype, size_t nelem, int st);
struct rt_var_space *rt_get_var_space(const char *application, 
        size_t numst );
void rt_clr_var_space(struct rt_var_space *rt_var_space);
