#include <linux/fs.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/cdev.h>
#include <linux/gpio.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/of_address.h>
#include <linux/gpio.h>


#include "gpio_capture.h"

static int gpios[] = {
//    {198}, /* PG6 : 32*6 + 6*/
    {67}, /* PC3 */
};

static struct gpio_capture_platform_device_data dev_platform_data = {
	.gpio_nums = sizeof(gpios)/sizeof(gpios[0]),
    .irq_type = IRQ_TYPE_EDGE_RISING,
	.debounce_us = 90,
    .pgpio = gpios,
};

static void gpio_capture_release(struct device *dev)
{

}

static struct platform_device gpio_capture_device = {
	.name = "gpio_cap",
	.id = -1,
	.dev = {
		.platform_data = &dev_platform_data,
		.release = gpio_capture_release,
	}
};

static int __init gpio_capture_dev_init(void)
{
	int ret = 0;
	int i = 0;
	struct gpio_capture_platform_device_data *pdata = gpio_capture_device.dev.platform_data;

	printk("%s:%d\n", __FUNCTION__, __LINE__);
/*
    for(i=0; i<pdata->gpio_nums; i++) {

		printk("%d\n", pdata->pgpio[i]);
	}
*/
    ret = platform_device_register(&gpio_capture_device);

	return ret;
}

static void __exit gpio_capture_dev_exit(void)
{
    printk("%s:%d\n", __FUNCTION__, __LINE__);
    platform_device_unregister(&gpio_capture_device);
}

module_init(gpio_capture_dev_init);
module_exit(gpio_capture_dev_exit);
MODULE_DESCRIPTION("gpio capture platform device");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("nuvoton");

