#define DEBUG

#include <linux/stddef.h>     // NULL
#include <linux/rbtree.h>
#include <linux/list.h>
#include <linux/slab.h>         // get_free_page
#include <linux/string.h>       // strcpy
#include <linux/kernel.h>       // pr_info
#include <linux/module.h>       // EXPORT_SYMBOL_GPL
#include <linux/err.h>          // ERR_PTR
#include <asm/semaphore.h>

#include <rtai_sem.h>           // RTAI semaphores

#include "rt_vars.h"

#define CHECK_RETURN(cond, val) \
    if (cond) return val;

#define CHECK_ERR(cond, jumpto, reason, args...)                \
    if (cond) {                                                 \
        pr_info("%s(): " reason "\n", __func__, ##args);        \
        goto jumpto;                                            \
    }

#define ALIGN_INT(x) ALIGN((x),sizeof(int))

typedef enum {
    bool_T,
    sint8_T, uint8_T,
    sint16_T, uint16_T,
    sint32_T, uint32_T,
    sint64_T, uint64_T,
    single_T, double_T,
} dtype_t;

static struct dtype {
    const char *name;
    dtype_t id;
    size_t data_type_len;
} dtype_list[] = {
    {"bool_T",   bool_T,   sizeof(uint8_t)},
    {"boolean_T",   bool_T,   sizeof(uint8_t)},

    {"uint8_T",  uint8_T,  sizeof(uint8_t)},
    {"sint8_T",  sint8_T,  sizeof(int8_t)},
    {"int8_T",  sint8_T,  sizeof(int8_t)},

    {"uint16_T", uint16_T, sizeof(uint16_t)},
    {"sint16_T", sint16_T, sizeof(int16_t)},
    {"int16_T", sint16_T, sizeof(int16_t)},

    {"uint32_T", uint32_T, sizeof(uint32_t)},
    {"sint32_T", sint32_T, sizeof(int32_t)},
    {"int32_T", sint32_T, sizeof(int32_t)},

    {"uint64_T", uint64_T, sizeof(uint64_t)},
    {"sint64_T", sint64_T, sizeof(int64_t)},
    {"int64_T", sint64_T, sizeof(int64_t)},

    {"single_T", single_T, sizeof(float)},
    {"real32_T", single_T, sizeof(float)},

    {"double_T", double_T, sizeof(double)},
    {"real_T", double_T, sizeof(double)},
    {NULL}
};

struct rt_var {

    /* Pointer to the application's data space */
    struct rt_var_space *rt_var_space;

    /* Variable name */
    char                *name;

    /* Sample time domain of this variable */
    unsigned int        st;

    /* List of all sources and destinations for an application
     * In list rt_var_space->subscriber_head/var_head */
    struct list_head    var_list;
    
    /* List of all destinations for an application that has to be copied for
     * a sample time */
    struct list_head    st_copy_list;
    
    /* This structure has 2 functions:
     *  1) it is used to string together a variable source with all its
     *     subscribers 
     *  2) When a destination is not connected to a source, this list
     *     strings together all destination registrations of a variable. The
     *     variable whose rb_node is in the rbtree is the head of the list.
     */
    struct list_head    subscriber_list;

    /* For a source, this is the list var_root
     * For a destination, this rb_node is used to place the destination
     * on unsubscribed_root */
    struct rb_node      node;

    /* rt_var name, vector length and data type
     * this variable wants to connect to */
    size_t              nelem;
    size_t              data_len;
    struct dtype        *dtype;

    /* Only used by destinations, points to the variable and data source */
    struct rt_var       *src_var;

    /* Pointer to where the actual data is stored */
    void                *src_addr_ptr;

    /* Destination variables store their data here */
    void                *dst_addr_ptr;
};

/* Header for every page that is allocated */
struct my_page {
    /* List of all pages
     * In list rt_var_space->page_head */
    struct list_head list;

    /* Set to 1 if the page was vmalloced() */
    int vmalloced;

    size_t page_fill;

    /* index in ->data where the next space is free */
    unsigned int free_idx;

    /* Pointer to memory location just past this page. Used to decide
     * whether there is still some free memory in this page */
    void *last;

    /* Start of data */
    int *data;         // Use int, so it is aligned automatically
};

struct subscriber_data {
    /* Space for the linked list */
    struct list_head list;

    /* Application pointer where the data comes from */
    struct rt_var_space *rt_var_space;

    /* This array has the length rt_var->numst and contains a list of all
     * variables in the time domain of the source application that this
     * application has subscribed to*/
    struct list_head dst_list[];
};

struct rt_var_space {
    char *application; // Application name
    int numst;          // Number of sample times

    /* All allocated pages are linked in here */
    struct my_page page_head;

    /* All variable sources/destinations are linked in here */
    struct list_head src_head;
    struct list_head dst_head;

    struct {
        /* General RTAI lock for the page */
        SEM lock;

        /* All data sources have their actual data space here */
        struct my_page rt_data;

        /* Here is a list of applications where destinations get their data
         * from. The linked structure is of type "struct subscriber_data" */
        struct list_head subscriber_appl;
    } st_data[];
};


DECLARE_MUTEX(rt_var_lock);
static struct rb_root var_root = RB_ROOT;
static struct rb_root unsubscribed_root = RB_ROOT;

/* Private malloc function. */
int get_mem(struct my_page *page_head, void **ptr, void **mirr_ptr,
        size_t len)
{
    struct my_page *page = 
        list_entry( page_head->list.next, struct my_page, list);
    void *p = NULL;

    pr_debug("Entering %s():"
            " page_head %p, ptr %p, mirr_ptr %p, len %u\n",
            __func__,
            page_head, ptr, mirr_ptr, len);

    /* For large chunks of memory use vmalloc() */
    if (len > page_head->page_fill) {
        len = ALIGN_INT(len);
        p = vmalloc(mirr_ptr ? len*2 : len);
        CHECK_ERR(!p, out_nomem, "No memory");
        pr_debug("+++ vmalloc %p line %i\n", p, __LINE__);
        page = kmalloc(sizeof(struct my_page), GFP_KERNEL);
        CHECK_ERR(!page, out_nomem, "No memory");
        pr_debug("+++ kmalloc %p line %i\n", page, __LINE__);

        *ptr = p;
        if (mirr_ptr)
            *mirr_ptr = (char*)p + len;

        /* Setup page structure so that it is firstly full and secondly
         * can be copied using rt_copy_buf() */
        page->data = (int*)p;
        page->last = (char*)p + len;
        page->page_fill = len;
        page->free_idx = len/sizeof(int);
        page->vmalloced = 1;

        /* Because this memory allocation is by definition already full,
         * append this page to the list */
        list_add_tail(&page->list, &page_head->list);

        return 0;
    }

    /* Get the pointer to free space */
    p = &page->data[page->free_idx];

    /* Check that the required block is still inside the old page
     * otherwise get a fresh one */
    if (p + len >= page->last) {
        /* Need a new page */
        p = (void*)__get_free_page(GFP_KERNEL);
        CHECK_ERR(!p, out_nomem, "No memory");
        pr_debug("+++ get_free_page %p line %i\n", p, __LINE__);
        page = kmalloc(sizeof(struct my_page), GFP_KERNEL);
        CHECK_ERR(!page, out_nomem, "No memory");
        pr_debug("+++ kmalloc %p line %i\n", page, __LINE__);

        *page = *page_head;
        page->data = (int*)p;
        page->last = (char*)p + page->page_fill;
        page->free_idx = 0;

        list_add(&page->list, &page_head->list);
        pr_debug("IIIIIIIIIIinserting into %p\n", &page_head->list);
    }
    
    *ptr = p;
    if (mirr_ptr)
        *mirr_ptr = (char*)p + page->page_fill;
    
    /* Update the index to the next free space */
    page->free_idx += (len + (sizeof(int) - 1))/sizeof(int);

    return 0;

out_nomem:
    return -ENOMEM;
}

/* This function is called at the beginning of a calculation interval to 
 * copy data into the local variable space */
void rt_copy_vars(struct rt_var_space *rtv, int st)
{
    struct subscriber_data *rt_var_appl;
    struct rt_var *rt_dst;

    /* Go through our list of applications where we subscribed data from */
    list_for_each_entry(rt_var_appl, &rtv->st_data[st].subscriber_appl, list) {
        unsigned int st;

        /* Go through the application's sample times to check for 
         * subscriptions */
        for (st = 0; st < rt_var_appl->rt_var_space->numst; st++) {
            if (list_empty(&rt_var_appl->dst_list[st]))
                continue;
            rt_sem_wait(&rt_var_appl->rt_var_space->st_data[st].lock);
            list_for_each_entry(rt_dst, &rt_var_appl->dst_list[st], 
                    st_copy_list) {
                memcpy(rt_dst->dst_addr_ptr, rt_dst->src_addr_ptr, 
                        rt_dst->data_len);
            }
            rt_sem_signal(&rt_var_appl->rt_var_space->st_data[st].lock);
        }
    }
}

/* This function is called at the end of a calculation interval to duplicate
 * all output variables into a buffer so that readers can access it at
 * will
 */
void rt_copy_buf(struct rt_var_space *rtv, int st)
{
    struct my_page *p;

    rt_sem_wait(&rtv->st_data[st].lock);
    list_for_each_entry(p, &rtv->st_data[st].rt_data.list, list) {
        memcpy(p->last, p->data, p->page_fill);
    }
    rt_sem_signal(&rtv->st_data[st].lock);
}

/* Use this function to initialise the head of all privately allocated pages.
 * Parameters: 
 *      p: structure to initialise
 *      page_fill: The fill factor of a page. Pages are allocated with size 
 *            PAGE_SIZE. However this algorithm will not use more than 
 *            page_fill bytes in a page. This is used when space must be 
 *            reserved for mirroring data within a page. In this case call
 *            this function with page_fill = PAGE_SIZE/2
 */
void init_page_head(struct my_page *p, size_t page_fill)
{
    INIT_LIST_HEAD(&p->list);
    p->page_fill = page_fill;
    p->last = p->data = NULL;
    p->free_idx = 0;
    p->vmalloced = 0;
}

/* Find a node called "name" in a rbtree */
static struct rt_var *find_var(struct rb_root *root, const char *name)
{
    struct rb_node * n = root->rb_node;
    struct rt_var *rt_var;

    pr_debug("Line %i %p\n", __LINE__, n);
    while (n) {
        int cmp;

        rt_var = rb_entry(n, struct rt_var, node);
        pr_debug("Line %i %p\n", __LINE__, rt_var);

        cmp = strcmp(name, rt_var->name);
        pr_debug("Line %i %p\n", __LINE__, rt_var);
        if (cmp < 0)
            n = n->rb_left;
        else if (cmp > 0)
            n = n->rb_right;
        else
            return rt_var;
    }
    return NULL;
}

/* Insert a rt_var into a rbtree 
 * Returns: rt_var if the variable name exists already
 *          NULL if the variable was inserted successfully
 */
static struct rt_var *insert_node(struct rb_root *root, struct rt_var *new_var)
{
    struct rb_node ** p = &root->rb_node;
    struct rb_node * parent = NULL;
    struct rt_var *rt_var;

    while (*p)
    {
        int cmp;

        parent = *p;
        rt_var = rb_entry(parent, struct rt_var, node);

        cmp = strcmp(new_var->name, rt_var->name);
        if (cmp < 0) {
            pr_debug("Moving left in tree\n");
            p = &(*p)->rb_left;
        } else if (cmp > 0) {
            pr_debug("Moving right in tree\n");
            p = &(*p)->rb_right;
        } else {
            pr_debug("Variable %s exists\n", new_var->name);
            return rt_var;
        }
    }

    pr_debug(">>> Inserting node %s %p\n", new_var->name, root);
    rb_link_node(&new_var->node, parent, p);
    rb_insert_color(&new_var->node, root);
    return NULL;
}

/* This is called by a variable destination (rt_dst) to subscribe to a variable
 * source (rt_var) */
static int subscribe(struct rt_var *rt_dst, struct rt_var *rt_src, int st)
{
    struct subscriber_data *s;
    pr_debug("Called %s(%p, %p, %i):%i; Subscribing %s:%s to %s:%s\n", 
            __func__, rt_dst, rt_src, st, __LINE__,
            rt_src->rt_var_space->application, rt_src->name,
            rt_dst->rt_var_space->application, rt_dst->name);

    CHECK_ERR(rt_dst->nelem != rt_src->nelem, out_incompatible,
            "Vector sizes for signal %s do not match: "
            "Source (%s) has length %u, Destination (%s) has length %u",
            rt_src->name,
            rt_src->rt_var_space->application, rt_src->nelem,
            rt_dst->rt_var_space->application, rt_dst->nelem);
    CHECK_ERR(rt_dst->dtype != rt_src->dtype, out_incompatible,
            "Data types for signal %s do not match: "
            "Source (%s) has type %s, Destination (%s) has type %s",
            rt_src->name,
            rt_src->rt_var_space->application, rt_src->dtype->name,
            rt_dst->rt_var_space->application, rt_dst->dtype->name);

    /* Look in the registered application list whether this application
     * is known. */
    pr_debug("Line %i\n", __LINE__);
    list_for_each_entry(s, 
            &rt_dst->rt_var_space->st_data[st].subscriber_appl, list) {
        if (s->rt_var_space == rt_src->rt_var_space) {
            pr_debug("Line %i: Source application '%s' is known\n", 
                    __LINE__, rt_src->rt_var_space->application);
            list_add(&rt_dst->st_copy_list, &s->dst_list[rt_src->st]);
            break;
        }
    }

    /* If s points to the list head, then the application of rt_src
     * was not yet registered. Do this now */
    pr_debug("Line %i\n", __LINE__);
    if (list_entry(&rt_dst->rt_var_space->st_data[st].subscriber_appl,
                struct subscriber_data, list) == s) {
        int i;

        pr_debug("Line %i: Source application '%s' is unknown; creating\n", 
                __LINE__, rt_src->rt_var_space->application);

        CHECK_RETURN(
                get_mem(&rt_dst->rt_var_space->page_head, (void*)&s, NULL,
                    (sizeof(struct subscriber_data) + 
                     sizeof(struct list_head) * rt_src->rt_var_space->numst)),
                -ENOMEM);

        s->rt_var_space = rt_src->rt_var_space;
        for (i = 0; i < rt_src->rt_var_space->numst; i++) {
            INIT_LIST_HEAD(&s->dst_list[i]);
        }

        list_add(&s->list, 
                &rt_dst->rt_var_space->st_data[st].subscriber_appl);
        list_add(&rt_dst->st_copy_list, &s->dst_list[rt_src->st]);
    }

    pr_debug("Line %i: Adding destination %s:%s to the source %s:%s\n", 
            __LINE__,
            rt_dst->rt_var_space->application, rt_dst->name,
            rt_src->rt_var_space->application, rt_src->name);
    list_add(&rt_dst->subscriber_list, &rt_src->subscriber_list);

    /* Remove this node from unsubscribed_root if it exists in that tree.
     * There are good reasons for not existing in this tree:
     *     - The variable is brand new
     *     - Another variable with the same name existed already.
     */
    if (rt_dst->node.rb_parent || unsubscribed_root.rb_node == &rt_dst->node) {
        pr_debug("<<< Erasing node %s from %p\n", 
                rt_dst->name, &unsubscribed_root);
        rb_erase(&rt_dst->node, &unsubscribed_root);
    }
    rt_dst->src_addr_ptr = rt_src->src_addr_ptr;
    rt_dst->src_var = rt_src;

    return 0;

out_incompatible:
    return -1;
}

static void unsubscribe(struct rt_var *rt_dst, int put_on_unsubscribers_tree)
{
    pr_debug("Called %s(%p, %i): Unsubscribing %s:%s\n",
            __func__, rt_dst, put_on_unsubscribers_tree,
            rt_dst->rt_var_space->application, rt_dst->name);

    /* Make the client's pointer point to itself - otherwise will get a 
     * segmentation violation */
    rt_dst->src_addr_ptr = rt_dst->dst_addr_ptr;
    memset(rt_dst->dst_addr_ptr, 0, rt_dst->data_len);

    if (rt_dst->src_var) {
        pr_debug("Removing subscription of %s:%s from %s:%s\n", 
                rt_dst->rt_var_space->application, rt_dst->name,
                rt_dst->src_var->rt_var_space->application, 
                rt_dst->src_var->name);
        /* Remove this variable destination from the source's 
         * subscription list */
        list_del(&rt_dst->subscriber_list);

    }
    rt_dst->src_var = NULL;

    /* When an application ends, it is not necessary to place this variable
     * on the unsubscribed list, thus saving some processing of the rbtrees.
     * In this case the function would be called with 
     * put_on_unsubscribers_tree = 0
     */
    if (put_on_unsubscribers_tree) {
        struct rt_var *rt_var;

        rt_var = insert_node(&unsubscribed_root, rt_dst);
        if (rt_var) {
            /* Variable with this name exists already. So now misuse the field
             * rt_var->subscriber_list to attach this variable to it */
            pr_info("Variable %s existed as unsubscribed, adding to the list\n",
                    rt_var->name);
            list_add(&rt_dst->subscriber_list, &rt_var->subscriber_list);
            rt_dst->node.rb_parent = NULL;
        }
        else {
            pr_debug("Putting %s on unsubscribed list \n", rt_dst->name);
            INIT_LIST_HEAD(&rt_dst->subscriber_list);
        }
    }
}

/* Try to subscribe to a source variable. It is not an error if the 
 * variable does not exist yet - it may appear later with another application
 *
 * Returns:
 *      0       : No error
 *      -EINVAL : st is unknown
 *                dtype is unknown
 *      -ENOMEM : Out of memory
 */
int rt_get_var(struct rt_var_space *rt_var_space,
        const char *name, const char *dtype, void *dst, size_t nelem, int st)
{
    struct rt_var *rt_src, *rt_dst;
    struct dtype *dtype_entry = dtype_list;
    
    pr_debug("\nCalled %s(%p, %s, %s, %p, %u, %i): Registering a destination "
            "variable %s:%s\n",
            __func__, rt_var_space, name, dtype, dst, nelem, st,
            rt_var_space->application, name);

    CHECK_ERR(st >= rt_var_space->numst, out_st_err,
            "Trying to get a variable from an unknown sample time domain %i", 
            st);

    /* Locate the data type */
    while (dtype_entry->name && strcmp(dtype_entry->name, dtype))
        dtype_entry++;
    CHECK_ERR(!dtype_entry->name, out_unknown_dtype, 
            "Data type %s unknown", dtype);
    pr_debug("Line %i: Found data %s has data length %u\n", 
            __LINE__, dtype_entry->name, dtype_entry->data_type_len);

    /* Get some memory */
    CHECK_RETURN(
            get_mem( &rt_var_space->page_head, (void *)&rt_dst, NULL,
                sizeof(*rt_dst) + strlen(name) + 1),
            -ENOMEM);

    /* Initialise some variables */
    rt_dst->dtype = dtype_entry;
    rt_dst->src_var = NULL;
    rt_dst->dst_addr_ptr = dst;
    rt_dst->nelem = nelem;
    rt_dst->data_len = nelem * dtype_entry->data_type_len;
    rt_dst->rt_var_space = rt_var_space;
    rt_dst->st = st;

    /* Variable name goes just behind the data space */
    rt_dst->name = (char*)(rt_dst + 1);
    strcpy(rt_dst->name, name);

    /* The following initialisations are a hint to the subscribe() and
     * unsubscribe() functions that this is a fresh variable that does
     * not yet exist on any list */
    rt_dst->node.rb_parent = NULL;
    list_add(&rt_dst->var_list, &rt_var_space->dst_head);

    /* Try to find the source with this name. If found, subscribe to it
     * otherwise put this list on the application's unsubscribed list */
    down(&rt_var_lock);
    rt_src = find_var(&var_root, name);
    if (rt_src) {
        int err;

        pr_debug("Line %i: Source variable exists in application %s; "
                "subscribing\n",
                __LINE__, rt_src->rt_var_space->application);
        if ((err = subscribe(rt_dst, rt_src, st))) {
            up(&rt_var_lock);
            return err;
        }
    }
    else {
        pr_debug("Source variable does not exist\n");
        unsubscribe(rt_dst, 1);
    }
    up(&rt_var_lock);

    return 0;

out_unknown_dtype:
out_st_err:
    return -EINVAL;
}

/* Register a source variable. These variables are outputs from an 
 * application that destination variables can subscribe to. 
 *
 * Returns: Pointer to data space where the application can write to 
 *          NULL on error
 */
void *rt_reg_var(struct rt_var_space *rt_var_space,
        const char *name, const char *dtype, size_t nelem, int st)
{
    struct dtype *dtype_entry = dtype_list;
    struct rt_var *new_var, *rt_src, *rt_dst;
    void *dst_addr;
    size_t data_len;

    pr_debug("\nCalled %s(%p, %s, %s, %u, %i): Registering a source "
            "variable %s:%s\n",
            __func__, rt_var_space, name, dtype, nelem, st,
            rt_var_space->application, name);

    CHECK_ERR(st >= rt_var_space->numst, out_st_err,
            "Trying to register a variable in unknown sample time domain %i", 
            st);

    /* Find out whether the data type is known */
    while (dtype_entry->name && strcmp(dtype_entry->name, dtype))
        dtype_entry++;
    CHECK_ERR(!dtype_entry->name, out_unknown_dtype, 
            "Data type %s unknown", dtype);
    pr_debug("Line %i: Found data %s has data length %u\n", 
            __LINE__, dtype_entry->name, dtype_entry->data_type_len);

    /* Get some memory where variable attributes are stored */
    CHECK_RETURN(
            get_mem( &rt_var_space->page_head, (void*)&new_var, NULL,
                sizeof(*new_var) + strlen(name) + 1),
            NULL);

    /* Variable name goes just behind this structure - there is no 
     * local variable data space */
    new_var->name = (char*)(new_var + 1);
    strcpy(new_var->name, name);

    pr_debug("Line %i\n", __LINE__);
    /* Initialise some variables */
    new_var->nelem = nelem;
    new_var->rt_var_space = rt_var_space;
    INIT_LIST_HEAD(&new_var->subscriber_list);
    new_var->st = st;
    new_var->dtype = dtype_entry;

    pr_debug("Line %i\n", __LINE__);
    /* Put this source variable in the global tree and also in
     * the application's list */
    CHECK_ERR((rt_src = insert_node(&var_root, new_var)), out_var_exists, 
            "Variable %s exists in application %s", name, 
            rt_src->rt_var_space->application);
    list_add(&new_var->var_list, &rt_var_space->src_head);

    pr_debug("Line %i\n", __LINE__);
    data_len = dtype_entry->data_type_len * nelem;
    CHECK_RETURN( 
            get_mem(
                &rt_var_space->st_data[st].rt_data,
                (void*)&dst_addr,
                (void*)&new_var->src_addr_ptr,
                data_len),
            NULL
            );
    memset(dst_addr, 0, data_len);
    memset(new_var->src_addr_ptr, 0, data_len);

    pr_debug("Line %i %p\n", __LINE__, unsubscribed_root.rb_node);
    /* Go through the list of unsubscribed destinations
     * to find whether this variable is required somewhere */
    down(&rt_var_lock);
    if ((rt_dst = find_var(&unsubscribed_root, name))) {
        struct rt_var *n, *n1;

        pr_debug("There were subscribers waiting for %s\n", name);
        list_for_each_entry_safe( n, n1, 
                &rt_dst->subscriber_list, subscriber_list) {
            subscribe(n, new_var, n->st);
        }
        if (subscribe(rt_dst, new_var, rt_dst->st)) {
            goto out_subscribe_err;
        }
    }
    up(&rt_var_lock);

    pr_debug("Line %i\n", __LINE__);
    return dst_addr;

out_subscribe_err:
    up(&rt_var_lock);
out_unknown_dtype:
out_var_exists:
out_st_err:
    return NULL;
}

/* When the application leaves, call this function */
void rt_clr_var_space(struct rt_var_space *rt_var_space)
{
    struct my_page *page, *n;
    struct rt_var *rt_src, *rt_dst, *rts_n;
    int st;

    pr_debug("\nCalled %s(%p): Logging off %s\n",
            __func__, rt_var_space, rt_var_space->application);

    down(&rt_var_lock);

    /* First go through the list of variable sources. Make sure that
     * no one subscribes to the variables any more and then remove 
     * this variable from var_root */
    pr_debug("Going thru my list of vars and detach destinations\n");
    list_for_each_entry( rt_src, &rt_var_space->src_head, var_list) {
        pr_debug("   Checking %s...\n", rt_src->name);
        list_for_each_entry_safe( rt_dst, rts_n, 
                &rt_src->subscriber_list, subscriber_list) {
            pr_debug("      Unsubscribing %s:%s\n", 
                    rt_dst->rt_var_space->application, rt_dst->name);

            /* Only unsubscribe destinations from other variable spaces */
            if (rt_dst->rt_var_space != rt_var_space) {
                unsubscribe(rt_dst, 1);
            }
        }
        pr_debug("<<< Erasing node %s from %p\n", rt_src->name, &var_root);
        rb_erase(&rt_src->node, &var_root);
    }

    /* Check that destination variables in this variable space are detached
     * from their sources */
    pr_debug("Going thru my list of destinations and detach\n");
    list_for_each_entry( rt_dst, &rt_var_space->dst_head, var_list) {
        pr_debug("   Checking %s...\n", rt_dst->name);
        if (rt_dst->src_var) {
            /* Destination variable is subscribed. Only unsubscribe those
             * variables whose source lies within another variable space
             */
            if (rt_dst->src_var->rt_var_space != rt_var_space)
                unsubscribe(rt_dst, 0);
        }
        else {
            /* Destination variable is not subscribed and somewhere on the 
             * unsubscribed list - either with its node directly in the 
             * unsubscribed_root itself or only linked into the variable that 
             * is.
             */
            if (rt_dst->node.rb_parent || 
                    unsubscribed_root.rb_node == &rt_dst->node) {
                struct rt_var *new_dst;

                new_dst = list_entry(rt_dst->subscriber_list.next, 
                        struct rt_var, subscriber_list);

                if (rt_dst == new_dst) {
                    pr_debug("<<< Erasing node %s from %p\n", 
                            rt_dst->name, &unsubscribed_root);
                    rb_erase(&rt_dst->node, &unsubscribed_root);
                }
                else {
                    pr_debug("<<< Replacing node %s from %p\n", 
                            rt_dst->name, &unsubscribed_root);
                    rb_replace_node(&rt_dst->node, &new_dst->node, 
                            &unsubscribed_root);
                }
            }
            list_del(&rt_dst->subscriber_list);
        }
    }

    /* Go thru the list of variable subscribers and detach them from
     * the source. */
    pr_debug("Going thru my list of sample times to free data\n");
    for (st = 0; st < rt_var_space->numst; st++) {
        pr_debug("   Checking sample time %i...\n", st);

        list_for_each_entry_safe( page, n, 
                &rt_var_space->st_data[st].rt_data.list, list) {
            pr_debug("      Freeing src variable data space\n");
            if (page->vmalloced) {
                vfree(page->data);
                pr_debug("--- vfree %p line %i\n", page->data, __LINE__);
            }
            else {
                free_page((unsigned long)page->data);
                pr_debug("--- free_page %p line %i\n", page->data, __LINE__);
            }
            kfree(page);
            pr_debug("--- kfree %p line %i\n", page, __LINE__);
        }
    }
    up(&rt_var_lock);

    /* Throw away all allocated pages */
    pr_debug("Throwing away memory used to manage variables\n");
    list_for_each_entry_safe( page, n, &rt_var_space->page_head.list, list) {
        if (page->vmalloced) {
            vfree(page->data);
            pr_debug("--- vfree %p line %i\n", page->data, __LINE__);
        }
        else {
            free_page((unsigned long)page->data);
            pr_debug("--- free_page %p line %i\n", page->data, __LINE__);
        }
        kfree(page);
        pr_debug("--- kfree %p line %i\n", page, __LINE__);
    }

    kfree(rt_var_space);
    pr_debug("--- kfree %p line %i\n", rt_var_space, __LINE__);
}

/* This is the first function to call before using this module. 
 * All subsequent calls must pass the return value to the function as a 
 * first argument 
 */
struct rt_var_space *rt_get_var_space(const char *application,
        size_t numst)
{
    struct rt_var_space *rt_var_space;
    int st;

    pr_debug("\nCalled %s(%s, %i): Logging in %s\n",
            __func__, application, numst, application);

    pr_debug("Line %i %p\n", __LINE__, unsubscribed_root.rb_node);
    pr_debug("Line %i %p\n", __LINE__, var_root.rb_node);
    /* Get some memory for application's data. Make sure there is enough
     * space for sample time data and the application's name */
    rt_var_space = kmalloc(sizeof(struct rt_var_space)
            + sizeof(rt_var_space->st_data[0]) * numst
            + strlen(application) + 1, GFP_KERNEL);
    CHECK_ERR(!rt_var_space, out_kmalloc, "No memory");
    pr_debug("+++ kmalloc %p line %i\n", rt_var_space, __LINE__);
    pr_debug("Allocated rt_var_space %p for Application %s\n", 
            rt_var_space, application);

    /* Space for application name is after last entry of ->rt_var[] */
    rt_var_space->application = (char*)&rt_var_space->st_data[numst];
    strcpy(rt_var_space->application, application);

    /* Initialise some variables */
    rt_var_space->numst = numst;
    INIT_LIST_HEAD(&rt_var_space->src_head);
    INIT_LIST_HEAD(&rt_var_space->dst_head);

    /* Initialise headers where private memory allocation is performed */
    init_page_head(&rt_var_space->page_head, PAGE_SIZE);
    for (st = 0; st < numst; st++) {
        rt_sem_init(&rt_var_space->st_data[st].lock, 1);
        init_page_head(&rt_var_space->st_data[st].rt_data, PAGE_SIZE/2);
        INIT_LIST_HEAD(&rt_var_space->st_data[st].subscriber_appl);
    }

    return rt_var_space;

out_kmalloc:
    return NULL;
}

EXPORT_SYMBOL_GPL(rt_copy_vars);
EXPORT_SYMBOL_GPL(rt_copy_buf);
EXPORT_SYMBOL_GPL(rt_get_var_space);
EXPORT_SYMBOL_GPL(rt_clr_var_space);
EXPORT_SYMBOL_GPL(rt_get_var);
EXPORT_SYMBOL_GPL(rt_reg_var);
