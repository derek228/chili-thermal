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
#include "gpio_pwm.h"

static unsigned int gpio_pwm_gpios[] = {
    {198}, /* PG6 : 32*6 + 6*/
  //  {199}, /* PG7 */
};

static struct gpio_pwm_platform_data dev_platform_data = {
	.pwm_chip_idx = 0x20,
    .gpio_nums = sizeof(gpio_pwm_gpios)/sizeof(gpio_pwm_gpios[0]),
	.gpios = gpio_pwm_gpios
};

static void gpio_pwm_release(struct device *dev)
{

}

static struct platform_device gpio_pwm_device = {
	.name = "gpio-pwm",
	.id = -1,
	.dev = {
		.platform_data = &dev_platform_data,
		.release = gpio_pwm_release,
	}
};

static int __init gpio_pwm_dev_init(void)
{
	int ret = 0;

    ret = platform_device_register(&gpio_pwm_device);

	return ret;
}

static void __exit gpio_pwm_dev_exit(void)
{
    platform_device_unregister(&gpio_pwm_device);
}

module_init(gpio_pwm_dev_init);
module_exit(gpio_pwm_dev_exit);
MODULE_DESCRIPTION("gpio pwm platform device");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("nuvoton");

