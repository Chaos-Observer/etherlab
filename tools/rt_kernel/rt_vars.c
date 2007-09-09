#define DEBUG

#include <linux/stddef.h>     // NULL
#include <linux/rbtree.h>
#include <linux/list.h>
#include <linux/slab.h>         // get_free_page
#include <linux/string.h>       // strcpy
#include <linux/kernel.h>       // pr_info
#include <linux/module.h>       // EXPORT_SYMBOL_GPL
#include <linux/err.h>          // ERR_PTR

#include "rt_vars.h"

#define CHECK_RETURN(cond, val) \
    if (cond) return val;

#define CHECK_ERR(cond, jumpto, reason, args...)                \
    if (cond) {                                                 \
        pr_info("%s(): " reason "\n", __func__, ##args);        \
        goto jumpto;                                            \
    }

static struct dtype {
    const char *name;
    dtype_t id;
    size_t data_len;
} dtype_list[] = {
    {"bool_T",   bool_T,   sizeof(uint8_t)},
    {"uint8_T",  uint8_T,  sizeof(uint8_t)},
    {"sint8_T",  sint8_T,  sizeof(int8_t)},
    {"uint8_T",  uint8_T,  sizeof(uint8_t)},
    {"sint16_T", sint16_T, sizeof(int16_t)},
    {"uint16_T", uint16_T, sizeof(uint16_t)},
    {"sint32_T", sint32_T, sizeof(int32_t)},
    {"uint32_T", uint32_T, sizeof(uint32_t)},
    {"sint64_T", sint64_T, sizeof(int64_t)},
    {"uint64_T", uint64_T, sizeof(uint64_t)},
    {"single_T", single_T, sizeof(float)},
    {"double_T", double_T, sizeof(double)},
    {NULL}
};

static char zero[16];

struct rt_var {
    /* Pointer to the application's data space */
    struct rt_var_space *rt_var_space;

    /* Variable name */
    char                *name;

    /* Sample time domain of this variable */
    char                st;

    /* List of all sources and destinations for an application
     * In list rt_var_space->subscriber_head/var_head */
    struct list_head    list;
    
    /* For a source, this is the list head for all subscribers
     * For a destination, this is the linked list of the above list */
    struct list_head    subscriber_list;

    /* For a source, this is the list var_root
     * For a destination, this rb_node is used to place the destination
     * on unsubscribed_root */
    struct rb_node      node;

    /* rt_var name, vector length and data type
     * this variable wants to connect to */
    size_t              nelem;
    struct dtype        *dtype;

    /* Only used by destinations, points to the variable and data source */
    struct rt_var       *src;

    /* Pointer to where the actual data is stored */
    void                *data_addr;

    /* Destination variables store their data here */
    char                data[];
};

/* Header for every page that is allocated */
struct my_page {
    /* List of all pages
     * In list rt_var_space->page_head */
    struct list_head list;

    /* index in ->data where the next space is free */
    unsigned int free_idx;

    /* Pointer to memory location just past this page. Used to decide
     * whether there is still some free memory in this page */
    void *last;

    /* Start of data */
    int data[];         // Use int, so it is aligned automatically
};

struct rt_var_space {
    struct list_head list;

    int numst;

    /* All allocated pages are linked in here */
    struct list_head page_head;

    /* All variable sources are linked in here */
    struct list_head var_head;

    /* All variable desinations are linked in here */
    struct list_head subscriber_head;

    char *application; // Application name

    struct data_list {
        struct list_head data_head;
        struct list_head buffered_data_head;
    } data_list[];
};


static struct rb_root var_root = RB_ROOT;
static struct rb_root unsubscribed_root = RB_ROOT;
static LIST_HEAD(application_list);

static struct my_page *init_new_page(struct list_head *list_head)
{
    struct my_page *p = (struct my_page*)__get_free_page(GFP_KERNEL);
    CHECK_ERR(!p, out_gfp, "No pages free");

    p->free_idx = 0;
    p->last = (char*)p + PAGE_SIZE;
    list_add(&p->list, list_head);
    return p;

out_gfp:
    return NULL;
}

/* Private malloc function. This function cannot handle mallocs that
 * exceed the size of a page */
static void *get_mem(struct list_head *page_head, size_t len)
{
    struct my_page *page;
    void *ptr;

    if (list_empty(page_head)) {
        page = init_new_page(page_head);
        CHECK_RETURN(!page, NULL);
    }
    else {
        page = (struct my_page*)page_head->next;
    }

    /* Get the pointer to free space */
    ptr = &page->data[page->free_idx];

    /* Check that the required block is still inside the old page
     * otherwise get a fresh one */
    if (ptr+len >= page->last) {
        /* Need a new page */
        page = init_new_page(page_head);
        CHECK_RETURN(!page, ERR_PTR(-ENOMEM));

        ptr = page->data;
        CHECK_ERR(ptr+len >= page->last, out_too_large, 
                "Could not allocate %u bytes; too large", len);
    }
    
    /* Update the index to the next free space */
    page->free_idx += (len + (sizeof(int) - 1))/sizeof(int);

    return ptr;

out_too_large:
    return ERR_PTR(-E2BIG);
}

/* Find a node called "name" in a rbtree */
static struct rt_var *find_var(struct rb_root *root, const char *name)
{
    struct rb_node * n = root->rb_node;
    struct rt_var *rt_var;

    while (n) {
        int cmp;

        rt_var = rb_entry(n, struct rt_var, node);

        cmp = strcmp(name, rt_var->name);
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

static int subscribe(struct rt_var *rts, struct rt_var *rt_var)
{
    CHECK_ERR(rts->nelem != rt_var->nelem, out_incompatible,
            "Vector sizes for signal %s do not match: "
            "Source (%s) has length %u, Destination (%s) has length %u",
            rt_var->name,
            rt_var->rt_var_space->application, rt_var->nelem,
            rts->rt_var_space->application, rts->nelem);
    CHECK_ERR(rts->dtype != rt_var->dtype, out_incompatible,
            "Data types for signal %s do not match: "
            "Source (%s) has type %s, Destination (%s) has type %s",
            rt_var->name,
            rt_var->rt_var_space->application, rt_var->dtype->name,
            rts->rt_var_space->application, rts->dtype->name);

    list_add(&rts->subscriber_list, &rt_var->subscriber_list);
    if (rts->node.rb_color != -1) {
        pr_debug("<<< Erasing node %s from %p\n", 
                rts->name, &unsubscribed_root);
        rb_erase(&rts->node, &unsubscribed_root);
    }
    rts->data_addr = rt_var->data_addr;
    rts->src = rt_var;

    return 0;

out_incompatible:
    return -1;
}

static void unsubscribe(struct rt_var *rts, int put_on_unsubscribers_tree)
{
    /* Make the client's pointer read a zero - otherwise will get a 
     * segmentation violation */
    rts->data_addr = zero;

    if (rts->src) {
        // FIXME: lock rts->src here
        
        pr_debug("Removing dest from %s\n", rts->src->name);
        /* Remove this variable destination from the source's 
         * subscription list */
        list_del(&rts->subscriber_list);

    }
    rts->src = NULL;

    /* When an application ends, it is not necessary to place this variable
     * on the unsubscribed list, thus saving some processing of the rbtrees.
     * In this case the function would be called with 
     * put_on_unsubscribers_tree = 0
     */
    if (put_on_unsubscribers_tree) {
        struct rt_var *rt_var;

        rt_var = insert_node(&unsubscribed_root, rts);
        if (rt_var) {
            /* Variable with this name exists already. So now misuse the field
             * rt_var->subscriber_list to attach this variable to it */
            pr_info("Variable %s existed as unsubscribed, adding to the list\n",
                    rt_var->name);
            list_add(&rts->subscriber_list, &rt_var->subscriber_list);
            rts->node.rb_color = -1;
        }
        else {
            pr_debug("Putting %s on unsubscribed list \n", rts->name);
            INIT_LIST_HEAD(&rts->subscriber_list);
        }
    }
}

/* Try to subscribe to a source variable. It is not an error if the 
 * variable does not exist yet - it may appear later with another application
 */
void *rt_get_var(struct rt_var_space *rt_var_space,
        const char *name, const char *dtype, size_t nelem, int st)
{
    struct rt_var *rt_var;
    struct dtype *dtype_entry = dtype_list;
    struct rt_var *rts;
    
    CHECK_ERR(st >= rt_var_space->numst, out_st_err,
            "Trying to register an unknown sample time domain %i", st);

    /* Locate the data type */
    while (dtype_entry->name && !strcmp(dtype_entry->name, dtype))
        dtype_entry++;
    CHECK_ERR(!dtype_entry->name, out_unknown_dtype, 
            "Data type %s unknown", dtype);

    /* Get some memory */
    rts = (struct rt_var*) get_mem( &rt_var_space->page_head, 
            sizeof(struct rt_var) + 
            + dtype_entry->data_len * nelem
            + strlen(name) + 1);
    CHECK_RETURN(IS_ERR(rts), NULL);

    pr_debug("Registering new destination %s, dtype %s\n", 
            name, dtype);

    /* Initialise some variables */
    list_add(&rts->list, &rt_var_space->subscriber_head);
    rts->dtype = dtype_entry;
    rts->src = NULL;
    rts->nelem = nelem;
    rts->rt_var_space = rt_var_space;
    rts->st = st;

    /* Variable name goes just behind the data space */
    rts->name = rts->data + dtype_entry->data_len * nelem;
    strcpy(rts->name, name);

    /* The following initialisations are a hint to the subscribe() and
     * unsubscribe() functions that this is a fresh variable that does
     * not yet exist on any list */
    rts->node.rb_color = -1;

    /* Try to find the source with this name. If found, subscribe to it
     * otherwise put this list on the application's unsubscribed list */
    rt_var = find_var(&var_root, name);
    if (rt_var) {
        pr_debug("Variable exists, subscribing\n");
        subscribe(rts, rt_var);
    }
    else {
        pr_debug("Variable does not exist\n");
        unsubscribe(rts, 1);
    }
    pr_debug("XXXX %p\n", rts->node.rb_parent);

    return rts->data;

out_unknown_dtype:
out_st_err:
    return NULL;
}

/* Register a source variable. These variables are outputs from an 
 * application that destination variables can subscribe to. */
void *rt_reg_var(struct rt_var_space *rt_var_space,
        const char *name, const char *dtype, size_t nelem, int st)
{
    struct dtype *dtype_entry = dtype_list;
    struct rt_var *new_var, *rt_var, *rts;
    void *dst_addr;

    CHECK_ERR(st >= rt_var_space->numst, out_st_err,
            "Trying to register an unknown sample time domain %i", st);

    /* Get some memory */
    new_var = (struct rt_var*)get_mem( &rt_var_space->page_head, 
            sizeof(struct rt_var) + strlen(name) + 1);
    CHECK_RETURN(IS_ERR(new_var), NULL);

    pr_debug("Registering new variable %s, dtype %s\n", name, dtype);

    /* Variable name goes just behind this structure - there is no data */
    new_var->name = (char*)new_var->data;

    /* Initialise some variables */
    strcpy(new_var->name, name);
    new_var->nelem = nelem;
    new_var->rt_var_space = rt_var_space;
    INIT_LIST_HEAD(&new_var->subscriber_list);
    new_var->st = st;

    /* Put this source variable in the global tree and also in
     * the application's list */
    CHECK_ERR((rt_var = insert_node(&var_root, new_var)), out_var_exists, 
            "Variable %s exists in application %s", name, 
            rt_var->rt_var_space->application);
    list_add(&new_var->list, &rt_var_space->var_head);

    /* Find out whether the data type is known */
    while (dtype_entry->name && !strcmp(dtype_entry->name, dtype))
        dtype_entry++;
    CHECK_ERR(!dtype_entry->name, out_unknown_dtype, 
            "Data type %s unknown", dtype);
    new_var->dtype = dtype_entry;

    dst_addr = get_mem(&rt_var_space->data_list[st].data_head,
            dtype_entry->data_len * nelem);
    new_var->data_addr = get_mem(
            &rt_var_space->data_list[st].buffered_data_head,
            dtype_entry->data_len * nelem);
    CHECK_RETURN(!dst_addr, NULL);
    CHECK_RETURN(!new_var->data_addr, NULL);

    /* Go through each application's list of unsubscribed destinations
     * to find whether this variable is required somewhere */
    if ((rts = find_var(&unsubscribed_root, name))) {
        struct rt_var *n, *n1;

        pr_debug("There were subscribers waiting for %s\n", name);
        list_for_each_entry_safe( n, n1, 
                &rts->subscriber_list, subscriber_list) {
            pr_debug("Adding from application %s\n", 
                    n->rt_var_space->application);
            subscribe(n, new_var);
        }
        pr_debug("Adding from application %s\n", 
                rts->rt_var_space->application);
        subscribe(rts, new_var);
    }

    return dst_addr;

out_unknown_dtype:
out_var_exists:
out_st_err:
    return NULL;
}

/* When the application leaves, call this function */
void rt_clr_var_space(struct rt_var_space *rt_var_space)
{
    struct my_page *page, *n;
    struct rt_var *rt_var;
    struct rt_var *rts, *rts_n;
    int i;

    /* First go through the list of variable sources. Make sure that
     * no one subscribes to the variables any more and then remove 
     * this variable from var_root */
    pr_debug("Going thru my list of vars and detach destinations\n");
    list_for_each_entry( rt_var, &rt_var_space->var_head, list) {
        pr_debug("   Checking %s...\n", rt_var->name);
        list_for_each_entry_safe( rts, rts_n, 
                &rt_var->subscriber_list, subscriber_list) {
            pr_debug("      Unsubscribing %s\n", rts->name);
            unsubscribe(rts,
                    rts->rt_var_space == rts->src->rt_var_space ? 0 : 1);
        }
        pr_debug("<<< Erasing node %s from %p\n", rt_var->name, &var_root);
        rb_erase(&rt_var->node, &var_root);
    }

    /* Go thru the list of variable subscribers and detach them from
     * the source. */
    pr_debug("Going thru my list of destinations and detach from vars\n");
    list_for_each_entry( rts, &rt_var_space->subscriber_head, list) {
        unsubscribe(rts, 0);
    }

    /* Throw away all allocated pages */
    list_for_each_entry_safe( page, n, &rt_var_space->page_head, list) {
        free_page((unsigned long)page);
        pr_debug("Freed up private mem at %p\n", page);
    }

    for (i = 0; i < rt_var_space->numst; i++) {
        list_for_each_entry_safe( page, n, 
                &rt_var_space->data_list[i].data_head, list) {
            free_page((unsigned long)page);
            pr_debug("Freed up variable data at %p\n", page);
        }
        list_for_each_entry_safe( page, n, 
                &rt_var_space->data_list[i].buffered_data_head, list) {
            free_page((unsigned long)page);
            pr_debug("Freed up variable data at %p\n", page);
        }
    }

    list_del(&rt_var_space->list);

    kfree(rt_var_space);
    pr_debug("Freed up rt_var_space at %p\n", rt_var_space);
}

/* This is the first function to call before using this module. 
 * All subsequent calls must pass the return value to the function as a 
 * first argument 
 */
struct rt_var_space *rt_get_var_space(const char *application,
        size_t numst)
{
    struct rt_var_space *rt_var_space;
    int i;

    rt_var_space = kmalloc(sizeof(struct rt_var_space)
            + sizeof(struct data_list) * numst
            + strlen(application) + 1, GFP_KERNEL);
    CHECK_ERR(!rt_var_space, out_kmalloc, "No memory");
    pr_debug("Allocated rt_var_space %p for Application %s\n", 
            rt_var_space, application);

    list_add(&rt_var_space->list, &application_list);

    rt_var_space->application = (char*)&rt_var_space->data_list[numst];
    rt_var_space->numst = numst;
    strcpy(rt_var_space->application, application);
    INIT_LIST_HEAD(&rt_var_space->page_head);
    INIT_LIST_HEAD(&rt_var_space->var_head);
    INIT_LIST_HEAD(&rt_var_space->subscriber_head);
    for (i = 0; i < numst; i++) {
        INIT_LIST_HEAD(&rt_var_space->data_list[i].data_head);
        INIT_LIST_HEAD(&rt_var_space->data_list[i].buffered_data_head);
    }

    return rt_var_space;

out_kmalloc:
    return NULL;
}

EXPORT_SYMBOL_GPL(rt_get_var_space);
EXPORT_SYMBOL_GPL(rt_clr_var_space);
EXPORT_SYMBOL_GPL(rt_get_var);
EXPORT_SYMBOL_GPL(rt_reg_var);
