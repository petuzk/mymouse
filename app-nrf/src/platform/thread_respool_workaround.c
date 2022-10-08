/* This is a workaround for the issue #41482
 * https://github.com/zephyrproject-rtos/zephyr/issues/41482
 *
 * I've encountered it myself and found the related issue after debugging
 * *noises of proudness*
 *
 * Since zephyr version is tied to nRF SDK (and I prefer to not mess it up),
 * this workaround implements the behaviour of mentioned issue's official fix.
 * The function below has to be called somewhere in PRE_KERNEL stages as they
 * are executed before real threads (not dummy one) are created.
 */

#include <init.h>

static int thread_respool_workaround(const struct device *dev) {
    ARG_UNUSED(dev);

    struct k_thread* dummy_thread = k_current_get();

#if (CONFIG_HEAP_MEM_POOL_SIZE > 0)
    k_thread_system_pool_assign(dummy_thread);
#else
    dummy_thread->resource_pool = NULL;
#endif

    return 0;
}

SYS_INIT(thread_respool_workaround, PRE_KERNEL_1, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
