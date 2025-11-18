#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
//#include <linux/fpu/api.h>  // for struct fpu
#include <linux/slab.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tony");
MODULE_DESCRIPTION("FPU curfps NULL simulation test");

struct guest_fpu {
    struct fpstate *fpstate;
};

static int __init test_fpu_null_init(void)
{
    struct guest_fpu fake_guest;
    struct fpu *fpu;
    struct fpstate *curfps;

    pr_info("Starting simulated FPU NULL test\n");

    // Setup fake guest FPU without initializing fpstate
    fake_guest.fpstate = NULL;

    // Normal task FPU
    fpu = &current->thread.fpu;

    // Simulate code path:
    curfps = fake_guest.fpstate ? fake_guest.fpstate : fpu->fpstate;

    WARN_ON(!curfps);  // Should trigger only if fpu->fpstate is NULL

    if (curfps)
        pr_info("curfps is valid, xfeatures\n");
    else
        pr_warn("curfps is NULL!\n");

    return 0;
}

static void __exit test_fpu_null_exit(void)
{
    pr_info("Exiting simulated FPU NULL test module\n");
}

module_init(test_fpu_null_init);
module_exit(test_fpu_null_exit);

