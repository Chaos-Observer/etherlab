#include <etl_data_info.h>

struct app;
struct rt_app;
struct rt_vars;
extern struct rt_app rt_app;
void copy_input_variables(struct rt_vars *rt_vars, unsigned int st_id);
void copy_output_buffer(struct rt_vars *rt_vars, unsigned int st_id);
int rt_var_reg_input(struct rt_app *rt_app, const char *name, 
        enum si_datatype_t dtype, unsigned int *dim, unsigned int st_id,
        void *addr);
int rt_var_reg_output(struct rt_app *rt_app, const char *name, 
        enum si_datatype_t dtype, unsigned int *dim, unsigned int st_id,
        void **addr);
int rt_var_start(struct app *app);
void rt_var_end(struct app *app);
void rt_var_exit(struct rt_app *rt_app);
