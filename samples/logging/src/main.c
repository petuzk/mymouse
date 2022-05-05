#include <zephyr.h>
#include <sys/printk.h>
#include <logging/log.h>

LOG_MODULE_REGISTER(hwmain, LOG_LEVEL_DBG);

void main(void)
{
	const int delay = 1000;

	printk("Hello World! %s\n", CONFIG_BOARD);
	LOG_INF("Starting spam in %d ms", delay);

	k_msleep(delay);

	for (int i = 0; i < 500; i++) {
		LOG_INF("spam #%d: blah blah bruh :p", i);
	}
}
