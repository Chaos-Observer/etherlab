/******************************************************************************
 *
 *           Autor: Richard Hacker
 *
 *           (C) Copyright IgH 2005
 *           Ingenieurgemeinschaft IgH
 *           Heinz-Baecker Str. 34
 *           D-45356 Essen
 *           Tel.: +49 201/36014-0
 *           Fax.: +49 201/36014-14
 *           E-mail: info@igh-essen.com
 *
 * 	This file uses functions in rt_kernel.c to start the model.
 *
 *
 *****************************************************************************/ 

#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#error "Don't have config.h"
#endif

#define DEBUG

/* Kernel header files */
#include <linux/module.h>
#include <linux/random.h>
#include <linux/jiffies.h>

/* Local headers */
#include "rt_vars.h"          /* Public functions exported by manager */

#define CHECK_RETURN(cond, val) \
    if (cond) return val;

#define CHECK_ERR(cond, jumpto, reason, args...)                \
    if (cond) {                                                 \
        pr_info("%s(): " reason "\n", __func__, ##args);        \
        goto jumpto;                                            \
    }

struct rt_var_space *rtv;
double signal;
const void *ptr;

void *s[100];
unsigned char *c1;
unsigned char c2[100];

void __exit mod_cleanup(void)
{
    rt_clr_var_space(rtv);
}

int __init mod_init(void)
{

    rtv = rt_get_var_space("Test App", 3);
    CHECK_ERR(!rtv, out_get_var_space, "Failed rt_get_var_space");
    rt_copy_buf(rtv,1);
    rt_copy_buf(rtv,0);
    rt_copy_vars(rtv,1);


    CHECK_ERR(!rt_reg_var(rtv, "SIG01", "double_T", 2000, 1),
            out_reg_var, "Could not register variable");
    CHECK_ERR(!rt_reg_var(rtv, "SIG02", "double_T", 1, 1),
            out_reg_var, "Could not register variable");

    CHECK_ERR(rt_get_var(rtv, "SIG03", "double_T", &signal, 1, 1),
            out_reg_var, "Could not register variable");

    CHECK_ERR(rt_get_var(rtv, "SIG03", "double_T", &signal, 1, 1),
            out_reg_var, "Could not register variable");

    CHECK_ERR(!rt_reg_var(rtv, "SIG03", "double_T", 1, 1),
            out_reg_var, "Could not register variable");

    CHECK_ERR(rt_get_var(rtv, "SIG04", "double_T", &signal, 1, 1),
            out_reg_var, "Could not register variable");

    CHECK_ERR(!rt_reg_var(rtv, "SIG04", "double_T", 1, 1),
            out_reg_var, "Could not register variable");

    CHECK_ERR(rt_get_var(rtv, "SIG05", "double_T", &signal, 1, 1),
            out_reg_var, "Could not register variable");
    CHECK_ERR(rt_get_var(rtv, "SIG05", "double_T", &signal, 1, 1),
            out_reg_var, "Could not register variable");

    c1 = (unsigned char*)rt_reg_var(rtv, "SIG06", "uint8_T", 100, 1);
    CHECK_ERR(!c1, out_reg_var, "Could not register variable");

    CHECK_ERR(rt_get_var(rtv, "SIG06", "uint8_T", c2, 100, 1),
            out_reg_var, "Could not register variable");

    rt_copy_buf(rtv,1);
    rt_copy_buf(rtv,0);
    rt_copy_vars(rtv,1);

    c1[3] = 3;
    pr_info("c2[3] = %u\n", c2[3]);
    rt_copy_buf(rtv,1);
    pr_info("c2[3] = %u\n", c2[3]);
    rt_copy_vars(rtv,1);
    pr_info("c2[3] = %u\n", c2[3]);

//    pr_info("signal = %p, Ptr = %p\n", &signal, ptr);
    pr_info("Successfully started Variable Test\n");

    return 0;

out_reg_var:
    rt_clr_var_space(rtv);
out_get_var_space:
    return -1;
}


MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Test RT Vars");
MODULE_AUTHOR("Richard Hacker");

module_init(mod_init);
module_exit(mod_cleanup);
