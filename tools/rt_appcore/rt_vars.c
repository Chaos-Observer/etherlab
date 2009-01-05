#define DEBUG

#include <linux/stddef.h>     // NULL
#include <linux/rbtree.h>
#include <linux/list.h>
#include <linux/string.h>       // strcpy
#include <linux/kernel.h>       // pr_info
#include <linux/module.h>       // EXPORT_SYMBOL_GPL

#include <rtai_sem.h>           // RTAI semaphores

#include "rt_vars.h"
#include "rt_main.h"
#include "rt_app.h"

#define CHECK_RETURN(cond, val) \
    if (cond) return val;

#define CHECK_ERR(cond, jumpto, reason, args...)                \
    if (cond) {                                                 \
        pr_info("%s(): " reason "\n", __func__, ##args);        \
        goto jumpto;                                            \
    }

struct rt_output_var {
    struct list_head list;
    struct rb_node node;
    struct app *app;

    size_t offset;

    char *name;
    enum si_datatype_t dtype;
    unsigned int st_id;
    void **addr;
    size_t memsize;
    size_t dim_count;

    size_t dim[];
};

/* Input variables are signal sources as seen from the rt_application.
 * Before every calculation step, the values of output variables are copied
 * straight into the data area of the input signal.
 */
struct rt_input_var {
    struct list_head list;
    struct rb_node node;

    struct app *app;    /* Application that this variable belongs to */

    /* The output variable that this variable is connected to */
    const struct rt_output_var *rt_output_var;
    unsigned int connected;

    /* Properties of this signal */
    char *name;
    enum si_datatype_t dtype;
    unsigned int st_id;
    void *addr;
    size_t dim_count;
    size_t dim[];
};

struct rt_app_copy_list {
    struct list_head list;
    const struct app *app;
    SEM *output_data_sem;

    struct copy_list {
        void *dst;
        const void *src;
        size_t len;
    } copy_list[];
};

struct rt_vars {
    struct list_head input_vars;
    struct list_head output_vars;

    void *output_data;

    struct {
        struct list_head output_vars;
        size_t output_data_len;
        void *output_data;
        void *output_data_buf;

        SEM output_data_sem;

        SEM copy_list_sem;

        // Temporary and work variables need to set up the copy list
        size_t var_count;
        struct rt_app_copy_list *copy_list;

        struct list_head rt_app_copy_list;

    } st[];
};

/******************************************************************************/

void copy_input_variables(struct rt_vars *rt_vars, unsigned int st_id)
{
    struct rt_app_copy_list *rt_app_copy_list;

    if (!rt_vars)
        return;

    rt_sem_wait(&rt_vars->st[st_id].copy_list_sem);

    list_for_each_entry(rt_app_copy_list,
            &rt_vars->st[st_id].rt_app_copy_list, list) {
        struct copy_list *copy_list;

        rt_sem_wait(rt_app_copy_list->output_data_sem);
        for ( copy_list = rt_app_copy_list->copy_list; 
                copy_list->dst; copy_list++) {
            memcpy(copy_list->dst, copy_list->src, copy_list->len);
        }
        rt_sem_signal(rt_app_copy_list->output_data_sem);
    }

    rt_sem_signal(&rt_vars->st[st_id].copy_list_sem);
}

void copy_output_buffer(struct rt_vars *rt_vars, unsigned int st_id)
{
    if (!rt_vars)
        return;

    rt_sem_wait(&rt_vars->st[st_id].output_data_sem);
    memcpy(rt_vars->st[st_id].output_data,
            rt_vars->st[st_id].output_data_buf,
            rt_vars->st[st_id].output_data_len);
    rt_sem_signal(&rt_vars->st[st_id].output_data_sem);
}

struct rb_root input_root = RB_ROOT;
struct rb_root output_root = RB_ROOT;

static struct rt_input_var *find_first(struct rt_input_var *v)
{
    const char *name = v->name;
    int cmp;
    struct rb_node *p = rb_prev(&v->node);

    while (p) {
        cmp = strcmp(name, rb_entry(p, struct rt_input_var, node)->name);
        if (cmp)
            break;
        v = rb_entry(p, struct rt_input_var, node);
        p = rb_prev(p);
    }

    return v;
}

static struct rt_input_var *find_input_var(const char *name, struct rb_node *p)
{
    int cmp;
    struct rt_input_var *v;

    while (p) {
        v = rb_entry(p, struct rt_input_var, node);
        cmp = strcmp(name, v->name);
        if (cmp)
            p = cmp < 1 ? p->rb_left : p->rb_right;
        else {
            return v;
        }
    }

    return NULL;
}

static void disconnect_input_variables(struct app *self, 
        struct app *old_app)
{
    int i;
    struct rt_input_var *i_v;
    struct rt_app_copy_list *copy_list;

    for (i = 0; i < self->rt_app->num_st; i++) {

        rt_sem_wait(&self->rt_vars->st[i].copy_list_sem);
        list_for_each_entry(copy_list,
                &self->rt_vars->st[i].rt_app_copy_list, list) {
            if (copy_list->app == old_app) {
                list_del(&copy_list->list);
                kfree(copy_list);
            }
        }
        rt_sem_signal(&self->rt_vars->st[i].copy_list_sem);
    }

    list_for_each_entry(i_v, &self->rt_app->rt_vars->input_vars, list) {
        if (i_v->connected && i_v->rt_output_var->app == old_app) {
            i_v->connected = 0;
            i_v->rt_output_var = NULL;
            memset(i_v->addr, 0, i_v->rt_output_var->memsize);
        }
    }
}

void rt_var_end(struct app *app)
{
    struct rt_input_var *i_v;
    struct rt_output_var *o_v, *o_v1;
    struct rt_vars *rt_vars = app->rt_app->rt_vars;
    int i;

    if (!rt_vars)
        return;

    list_for_each_entry(i_v, &rt_vars->input_vars, list) {
        if (i_v->app) {
            rb_erase(&i_v->node, &input_root);
            pr_debug("Removing %s\n", i_v->name);
        }
    }

    for( i = 0; i < app->rt_app->num_st; i++) {
        struct rt_app_copy_list *copy_list, *n;

        list_for_each_entry_safe(o_v, o_v1,
                &app->rt_vars->st[i].output_vars, list) {
            if (o_v->app) {
                rb_erase(&o_v->node, &output_root);
                pr_debug("Removing %s\n", o_v->name);
            }

            /* Tell applications that the variable is deleted */
            i_v = NULL;
            while ((i_v = find_input_var(o_v->name, 
                            i_v 
                            ? rb_next(&i_v->node)
                            : input_root.rb_node))) {
                if (i_v->connected)
                    disconnect_input_variables(i_v->app, app);
            }

            kfree(o_v);
        }

        list_for_each_entry_safe(copy_list, n,
                &app->rt_vars->st[i].rt_app_copy_list, list)
            kfree(copy_list);
    }

    kfree(app->rt_vars->output_data);
    kfree(app->rt_vars);
}

static struct rt_output_var *find_output_var(const char *name)
{
    struct rb_node *p = output_root.rb_node;
    int cmp;
    struct rt_output_var *v;

    while (p) {
        v = rb_entry(p, struct rt_output_var, node);
        cmp = strcmp(name, v->name);
        if (cmp)
            p = cmp < 1 ? p->rb_left : p->rb_right;
        else
            return v;
    }

    return NULL;
}

static int connect_input_variables(struct app *self, 
        struct rt_vars *new_rt_vars, struct list_head *output_list)
{
    struct rt_input_var *i_v;
    const struct rt_output_var *o_v;
    struct rt_app_copy_list *rt_app_copy_list = NULL;
    unsigned int st;

    if (list_empty(output_list))
        return 0;

    // Go through output_list to find, mark and count the input variables
    // belonging to application *self that can be connected.
    list_for_each_entry(o_v, output_list, list) {
        i_v = find_input_var(o_v->name, input_root.rb_node);

        if (!i_v)
            continue;

        // Since there can be more than one variable with the same name,
        // get hold of the first one in the list
        i_v = find_first(i_v);

        do {
            if (i_v->app != self)
                continue;

            // Mark and count the input variable
            i_v->rt_output_var = o_v;
            self->rt_vars->st[i_v->st_id].var_count++;

            CHECK_ERR(o_v->dtype != i_v->dtype, out_incompatible,
                    "Incompatible data types for signal %s", i_v->name);
            CHECK_ERR(
                    o_v->dim_count != i_v->dim_count 
                    || memcmp(o_v->dim, i_v->dim,
                        o_v->dim_count * sizeof(*i_v->dim)),
                    out_incompatible, "Dimensions for signal %s are incompatible",
                    i_v->name);
        } while ((i_v = find_input_var(o_v->name, rb_next(&i_v->node))));
    }

    // Allocate memory for the copy lists
    for (st = 0; st < self->rt_app->num_st; st++) {
        size_t len = 
            (void*)&rt_app_copy_list->copy_list[self->rt_vars->st[st].var_count+1] 
            - (void*)rt_app_copy_list;

        rt_app_copy_list = kzalloc( len, GFP_KERNEL);
        CHECK_ERR(!rt_app_copy_list, out_nomem, "No memory");
        rt_app_copy_list->app = new_app;
        rt_app_copy_list->output_data_sem = &new_rt_vars->st[st].output_data_sem;

        self->rt_vars->st[st].var_count = 0;
        self->rt_vars->st[st].copy_list = rt_app_copy_list;
    }

    // Go through the list of marked input variables and place them on the 
    // respective copy list.
    list_for_each_entry(i_v, &self->rt_app->rt_vars->input_vars, list) {
        struct copy_list *copy_list;
        size_t var_count;

        o_v = i_v->rt_output_var;

        if (!o_v || i_v->connected)
            continue;

        i_v->connected = 1;

        var_count = self->rt_vars->st[i_v->st_id].var_count++;
        copy_list = &self->rt_vars->st[i_v->st_id].copy_list->copy_list[var_count];

        copy_list->dst = i_v->addr;
        copy_list->src = 
            o_v->app->rt_vars->st[o_v->st_id].output_data + o_v->offset;
        copy_list->len = o_v->memsize;
        pr_debug("%s src = %p, dst = %p, len = %u\n", i_v->name,
                copy_list->src,
                copy_list->dst,
                copy_list->len);
    }

    // Put the newly created copy list on the queue
    for (st = 0; st < self->rt_app->num_st; st++) {
        rt_sem_wait(&self->rt_vars->st[st].copy_list_sem);
        list_add_tail(&self->rt_vars->st[st].copy_list->list,
                &self->rt_vars->st[st].rt_app_copy_list);
        rt_sem_signal(&self->rt_vars->st[st].copy_list_sem);
        self->rt_vars->st[st].var_count = 0;
    }

    return 0;

out_nomem:
    return -ENOMEM;
out_incompatible:
    return -EINVAL;
}

int rt_var_start(struct app *app)
{
    struct rt_vars *rt_vars = app->rt_app->rt_vars;
    struct rb_node **p;
    struct rb_node *parent = NULL;
    struct rt_input_var *i_v;
    struct rt_output_var *o_v, *o_v1;
    unsigned int i;
    size_t len = 0;
    int rv;

    if (!rt_vars)
        return 0;

    app->rt_vars = kzalloc(
            (void*)&rt_vars->st[app->rt_app->num_st] - (void*)rt_vars,
            GFP_KERNEL);
    CHECK_ERR(!app->rt_vars, out_nomem, "No memory");

    for (i = 0; i < app->rt_app->num_st; i++) {
        rt_sem_init(&app->rt_vars->st[i].output_data_sem, 1);
        rt_sem_init(&app->rt_vars->st[i].copy_list_sem, 1);
        INIT_LIST_HEAD(&app->rt_vars->st[i].rt_app_copy_list);
        INIT_LIST_HEAD(&app->rt_vars->st[i].output_vars);
    }

    // Put all input variables on the input_root tree
    // Note that it is explicitly allowed, and thus catered for, that more
    // than one entry exists on the input_vars list having the same name.
    p = &input_root.rb_node;
    parent = NULL;
    list_for_each_entry(i_v, &rt_vars->input_vars, list) {
        pr_debug("Inserting input %s %p\n", i_v->name, i_v);

        CHECK_ERR(i_v->st_id >= app->rt_app->num_st, out_invalid_st_id, 
                "Invalid sample time id %u for Input Signal %s",
                i_v->st_id, i_v->name);

        while (*p) {
            parent = *p;
            p = strcmp( 
                    i_v->name,
                    rb_entry(parent, struct rt_input_var, node)->name) < 0
                ?  &parent->rb_left
                :  &parent->rb_right;
        }
        rb_link_node(&i_v->node, parent, p);
        rb_insert_color(&i_v->node, &input_root);

        i_v->app = app;
    }

    // Put output variables on the output_vars tree. The names of output 
    // variables have to be unique
    p = &output_root.rb_node;
    parent = NULL;
    list_for_each_entry_safe(o_v, o_v1, &rt_vars->output_vars, list) {
        pr_debug("Inserting output %s\n", o_v->name);

        CHECK_ERR(o_v->st_id >= app->rt_app->num_st, out_invalid_st_id, 
                "Invalid sample time id %u for Output Signal %s",
                o_v->st_id, o_v->name);

        while (*p) {
            int cmp = strcmp( 
                        o_v->name,
                        rb_entry(*p, struct rt_output_var, node)->name);
            parent = *p;
            if (cmp) {
                p = cmp < 0 ? &parent->rb_left : &parent->rb_right;
            }
            else {
                pr_info("Error inserting signal %s: "
                        "signal already exists in model %s.\n",
                        o_v->name,
                        rb_entry(*p, struct rt_output_var, node)->app->rt_app->name
                        );
                return -EEXIST;
            }
        }
        rb_link_node(&o_v->node, parent, p);
        rb_insert_color(&o_v->node, &output_root);

        o_v->app = app;
        o_v->offset = roundup(app->rt_vars->st[o_v->st_id].output_data_len, 
                si_data_width[o_v->dtype]);
        app->rt_vars->st[o_v->st_id].output_data_len = o_v->offset + o_v->memsize;
        list_move(&o_v->list, &app->rt_vars->st[o_v->st_id].output_vars);
    }

    // Make sure the output_variables are all pointer aligned and at the same
    // time calculate the total memory requirement of the output variable area
    len = 0;
    for (i = 0; i < app->rt_app->num_st; i++) {
        app->rt_vars->st[i].output_data_len =
            roundup(app->rt_vars->st[i].output_data_len, sizeof(void*));
        len += app->rt_vars->st[i].output_data_len;
    }
    pr_debug("output data length = %u\n", len);

    // Allocate the memory for the output variables. Note that since the output
    // data is double buffered, a buffer twice as big is allocated.
    app->rt_vars->output_data = kzalloc(2*len, GFP_KERNEL);
    CHECK_ERR(!app->rt_vars->output_data, out_nomem, "No memory");

    // Distribute the newly allocated buffer to all the sample times for both
    // the read buffer (output_data) and write buffer (output_data_buf)
    for (i = 0; i < app->rt_app->num_st; i++) {
        app->rt_vars->st[i].output_data = app->rt_vars->output_data 
            + (i ? app->rt_vars->st[i-1].output_data_len : 0);
        app->rt_vars->st[i].output_data_buf = 
            app->rt_vars->st[i].output_data + len;

        // Go through the list of output variables and set the pointer in the
        // rt_application to point to the memory location where the data can be
        // written to
        list_for_each_entry(o_v, &app->rt_vars->st[i].output_vars, list) {
            *o_v->addr = 
                app->rt_vars->st[o_v->st_id].output_data_buf + o_v->offset;
            pr_debug("data addr %p\n", *o_v->addr);
        }

        // Go through the list of output_vars and look whether there are 
        // variables on input_root waiting to be connected
        list_for_each_entry(o_v, &app->rt_vars->st[i].output_vars, list) {
            i_v = NULL;
            while ( (i_v = find_input_var(o_v->name, 
                            i_v 
                            ? rb_next(&i_v->node) 
                            : input_root.rb_node))) {
                pr_debug("Connect input %s\n", o_v->name);
                rv = connect_input_variables(i_v->app, app->rt_vars,
                        &app->rt_vars->st[i].output_vars);
                CHECK_RETURN(rv, rv);
            }
        }
    }

    // Go through the list of input_vars to check whether the corresponding 
    // output variable exists already
    list_for_each_entry(i_v, &rt_vars->input_vars, list) {
        if (i_v->connected) {
            // The input variable has been connected already
            continue;
        }

        o_v = find_output_var(i_v->name);
        if (o_v) {
            pr_debug("Connect output %s\n", o_v->name);
            rv = connect_input_variables(app, o_v->app->rt_vars,
                    &o_v->app->rt_vars->st[o_v->st_id].output_vars);
            CHECK_RETURN(rv, rv);
        }
    }

    return 0;

out_nomem:
    return -ENOMEM;
out_invalid_st_id:
    return -EINVAL;
}

/*******************************************************************************
 * Initialise the rt_vars structure for rt_app->rt_vars.
 * This is the primary structure required early during the initialisation 
 * of an application. At this point in time, the number of sample times may
 * still be unknown. Essentially this structure is required only to link
 * together all input and output variables in the input_vars and output_vars 
 * list respectively. The other structure elements are not used here.
 *
 * Note that this structure different as that for app->rt_vars, where the 
 * other structure members such as output_data and st[] are used.
 *******************************************************************************/
static int init_rt_vars(struct rt_app *rt_app, unsigned int n_st)
{
    if (!rt_app->rt_vars) {
        rt_app->rt_vars = kzalloc(sizeof(*rt_app->rt_vars), GFP_KERNEL);
        CHECK_ERR(!rt_app->rt_vars, out_kmalloc, "No memory");

        INIT_LIST_HEAD(&rt_app->rt_vars->input_vars);
        INIT_LIST_HEAD(&rt_app->rt_vars->output_vars);
    }

    return 0;

out_kmalloc:
    return -ENOMEM;
}

/*******************************************************************************
 * Register an output variable.
 *******************************************************************************/
int rt_var_reg_output(struct rt_app *rt_app, const char *name, 
        enum si_datatype_t dtype, unsigned int *dim, unsigned int st_id,
        void **addr)
{
    struct rt_output_var *v = NULL;
    size_t i;
    size_t dim_count = 0;
    size_t element_count;
    int rv;

    CHECK_RETURN((rv = init_rt_vars(rt_app, 0)), rv);

    /* Count the number of dimensions in zero terminated list */
    if (dim)
        while (dim[++dim_count]);
    else
        dim_count = 1;
    
    /* Get enough memory to hold the dimensions as well as the variable's name */
    v = kzalloc(((void*)&v->dim[dim_count] - (void*)v) + strlen(name) + 1, 
            GFP_KERNEL);
    CHECK_ERR(!v, out_kmalloc, "No memory");
    v->name = (char*)&v->dim[dim_count];

    /* Copy variable's data */
    v->dtype = dtype;
    v->dim_count = dim_count;
    v->st_id = st_id;
    v->addr = addr;
    strcpy(v->name, name);
    element_count = 1;
    if (dim)
        for (i = 0; i < dim_count; i++) {
            v->dim[i] = dim[i];
            element_count *= dim[i];
        }
    else
        v->dim[0] = 1;
    v->memsize = element_count * si_data_width[v->dtype];

    /* Add the new variable to the list of output variables */
    list_add_tail(&v->list, &rt_app->rt_vars->output_vars);

    return 0;

out_kmalloc:
    return -ENOMEM;
}
EXPORT_SYMBOL_GPL(rt_var_reg_output);

/*******************************************************************************
 * Register an input variable.
 *******************************************************************************/
int rt_var_reg_input(struct rt_app *rt_app, const char *name, 
        enum si_datatype_t dtype, unsigned int *dim, unsigned int st_id,
        void *addr)
{
    struct rt_input_var *v = NULL;
    size_t i;
    size_t dim_count = 0;
    int rv;

    CHECK_RETURN((rv = init_rt_vars(rt_app, 0)), rv);

    /* Count the number of dimensions in zero terminated list */
    if (dim)
        while (dim[++dim_count]);
    else
        dim_count = 1;
    
    /* Get enough memory to hold the dimensions as well as the variable's name */
    v = kzalloc(((void*)&v->dim[dim_count] - (void*)v) + strlen(name) + 1, 
            GFP_KERNEL);
    CHECK_ERR(!v, out_kmalloc, "No memory");
    v->name = (char*)&v->dim[dim_count];

    /* Copy variable's data */
    v->dtype = dtype;
    v->dim_count = dim_count;
    v->st_id = st_id;
    v->addr = addr;
    strcpy(v->name, name);
    if (dim)
        for (i = 0; i < dim_count; i++)
            v->dim[i] = dim[i];
    else
        v->dim[0] = 1;

    /* Add the new variable to the list of input variables */
    list_add_tail(&v->list, &rt_app->rt_vars->input_vars);
    
    return 0;

out_kmalloc:
    return -ENOMEM;
}
EXPORT_SYMBOL_GPL(rt_var_reg_input);

/*******************************************************************************
 * Free up the memory that was allocated during registration above
 *******************************************************************************/
void rt_var_exit(struct rt_app *rt_app)
{
    struct rt_vars *rt_vars = rt_app->rt_vars;
    struct rt_input_var *i_v, *i_v1;
    struct rt_output_var *o_v, *o_v1;

    if (!rt_vars)
        return;

    list_for_each_entry_safe(i_v, i_v1, &rt_vars->input_vars, list) 
        kfree(i_v);

    // Remove output variables from the list
    // Unless some error occurred duting initialisation, this list should be empty.
    // In this case, the variables are free()ed in rt_var_end()
    list_for_each_entry_safe(o_v, o_v1, &rt_vars->output_vars, list) 
        kfree(o_v);

    kfree(rt_vars);

    rt_app->rt_vars = NULL;
}
EXPORT_SYMBOL_GPL(rt_var_exit);
