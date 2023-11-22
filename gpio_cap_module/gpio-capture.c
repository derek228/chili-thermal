
/*
  nuvoton expand to muti-line gpio capture  
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/pm.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/time.h>

#include <asm/uaccess.h>
#include <linux/uaccess.h>      
#include <linux/ioctl.h>
#include <linux/interrupt.h>
#include <linux/signal.h>

#include "gpio_capture.h"

#define DRV_NAME "gpio_cap"


/********************************************/
struct gpio_data {
    int gpio;
    int irq;
    unsigned int count;
    volatile int state;
    int valid_state;
    unsigned long volatile time;
    unsigned long volatile last_time;
    struct delayed_work work;
    unsigned int debounce_time;
	struct hrtimer timer;
    spinlock_t lock;
// struct mutex lock;
    /* user space register task */
    struct task_struct *notify_task;
};

struct gpio_capture_platform_driver_data {
    int gpio_nums;
    int irq_type;
    struct gpio_data pgdata[];
};

struct gpio_capture_drv {
    struct gpio_capture_platform_driver_data *pdrvdata;
    struct miscdevice miscdev;    
};

/*********************************************************/

#define CAP_MAX_LINE                32
#define CAP_INFO_LEN_UNIT_BYTE      12
#define CAP_MAX_LEN                 (CAP_MAX_LINE * CAP_INFO_LEN_UNIT_BYTE)

unsigned int read_buf[CAP_MAX_LEN];

static ssize_t capture_info_read(struct file *file, char __user * userbuf, size_t count, loff_t * ppos)
{
    struct miscdevice *miscdev = file->private_data;
    struct device *dev = miscdev->parent;
    struct gpio_capture_drv *capture = dev_get_drvdata(dev);
    struct gpio_capture_platform_driver_data *pdrvdata = capture->pdrvdata;
    int read_len;
    int i; 
    unsigned long flags;

    int cap_data_len = pdrvdata->gpio_nums * CAP_INFO_LEN_UNIT_BYTE;
    
    if (count < cap_data_len) {
        return -EINVAL;
    }

    /* cap info use int unit */
    count = count / 4;
    read_len = 0;
    for (i=0; i<pdrvdata->gpio_nums; i++) {
     //   mutex_lock(&pdrvdata->pgdata[i].lock);
        spin_lock_irqsave(&pdrvdata->pgdata[i].lock, flags);

        if (pdrvdata->pgdata[i].valid_state == 1) {
            read_buf[read_len++] = (pdrvdata->pgdata[i].state << 16) | pdrvdata->pgdata[i].gpio;
            read_buf[read_len++] = pdrvdata->pgdata[i].count;
            read_buf[read_len++] = pdrvdata->pgdata[i].time;
            pdrvdata->pgdata[i].valid_state = 0;
        }

        spin_unlock_irqrestore(&pdrvdata->pgdata[i].lock, flags);
//    mutex_unlock(&pdrvdata->pgdata[i].lock);
    }

    if (copy_to_user(userbuf, read_buf, (read_len*4)))
        return -EINVAL;
 
    *ppos = (read_len*4);
  
    return (read_len*4);
}

static ssize_t capture_info_write(struct file *file, const char __user * userbuf, size_t count, loff_t * ppos)
{
    return 0;
}

#define CAP_SIG_ID          0x0a
#define CMD_SIG_TASK_REG    _IOW(CAP_SIG_ID, 0, int32_t*)

void capture_notify_register(struct gpio_capture_drv *capture)
{
    int i;    
    
    for (i=0; i<capture->pdrvdata->gpio_nums; i++) {
        capture->pdrvdata->pgdata[i].notify_task = get_current();
    }
}

void capture_notify_deregister(struct gpio_capture_drv *capture)
{
    int i;    
    
    for (i=0; i<capture->pdrvdata->gpio_nums; i++) {
        capture->pdrvdata->pgdata[i].notify_task = NULL;
    }
}

static int capture_open(struct inode *inode, struct file *file)
{
    struct miscdevice *miscdev = file->private_data;
    struct device *dev = miscdev->parent;
    struct gpio_capture_drv *capture = dev_get_drvdata(dev);

    capture_notify_register(capture);
    return 0;
}

static long capture_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct miscdevice *miscdev = file->private_data;
    struct device *dev = miscdev->parent;
    struct gpio_capture_drv *capture = dev_get_drvdata(dev);

//    printk("capture_ioctl\n");
    if ( cmd == CMD_SIG_TASK_REG ) {
        printk(KERN_INFO "CMD_SIG_TASK_REG\n");
        
        capture_notify_register(capture);
    }
    return 0;
}

static int capture_release(struct inode *inode, struct file *file)
{
    struct miscdevice *miscdev = file->private_data;
    struct device *dev = miscdev->parent;
    struct gpio_capture_drv *capture = dev_get_drvdata(dev);

    capture_notify_deregister(capture);
    
    return 0;
}

static struct file_operations gpio_capture_fops = {
    .owner              = THIS_MODULE,
    .read               = capture_info_read,
    .write              = capture_info_write,
    .open               = capture_open,
    .unlocked_ioctl     = capture_ioctl,
    .release            = capture_release,
};

/**********************************************/

#define GPIO_DEBOUNCE_ENABLE        1
#define GPIO_DEBUG                  1

static u32 gpio_get_state(struct gpio_data *pgpio)
{
    pgpio->state = gpio_get_value(pgpio->gpio);
    pgpio->count++;
    pgpio->time = jiffies;
    pgpio->valid_state = 1;

    return pgpio->state;
}

static enum hrtimer_restart gpio_debounce_timer_fun(struct hrtimer *data)
{
    unsigned long flags;    
    volatile int gpio_curr_state;
    struct gpio_data *pgdata = container_of(data, struct gpio_data, timer);

#if (GPIO_DEBUG == 1)
    gpio_set_value(200, 0);
#endif

    spin_lock_irqsave(&pgdata->lock, flags);
    if (pgdata->valid_state == 0) {
        gpio_curr_state = gpio_get_value(pgdata->gpio);
        if (gpio_curr_state == pgdata->state) {
            pgdata->count++;
            pgdata->time = jiffies;
            pgdata->valid_state = 1;
        }
    }
    spin_unlock_irqrestore(&pgdata->lock, flags);

    if (pgdata->valid_state) {
        if (pgdata->notify_task != NULL) {
            if( send_sig_info(CAP_SIG_ID, SEND_SIG_PRIV, pgdata->notify_task) < 0 ) {
                printk(KERN_INFO "send signal fail\n");
            }
        }
    }

	return HRTIMER_NORESTART;
}

static irqreturn_t gpio_capture_irq(int irq, void *dev_id)
{
    struct gpio_data *pgdata = dev_id;

#if (GPIO_DEBUG == 1)
    gpio_set_value(200, 1);
#endif

    if (pgdata->debounce_time) {
        pgdata->state = gpio_get_value(pgdata->gpio);
        pgdata->valid_state = 0;
        pgdata->time = jiffies;
    } else {
        gpio_get_state(pgdata);
    }

    if (hrtimer_is_queued(&pgdata->timer)) {
        hrtimer_restart(&pgdata->timer);
    } else {  
        hrtimer_start(&pgdata->timer, ktime_add_ns(ktime_get(), pgdata->debounce_time), HRTIMER_MODE_ABS);
    }

    return IRQ_HANDLED;
}

/*******************************************************/
static const struct of_device_id gpio_capture_of_match[] = {
    { .compatible = "gpio_cap", },
    { },
};
MODULE_DEVICE_TABLE(of, gpio_capture_of_match);

void capture_free_irq(struct gpio_capture_drv *capture)
{
    int i;

    for (i=0; i<capture->pdrvdata->gpio_nums; i++) {
        if (capture->pdrvdata->pgdata[i].irq) {
            free_irq(capture->pdrvdata->pgdata[i].irq, &capture->pdrvdata->pgdata[i]);
        }
    }
}

void capture_free_gpio(struct gpio_capture_drv *capture)
{
    int i;

    for (i=0; i<capture->pdrvdata->gpio_nums; i++) {
        if (capture->pdrvdata->pgdata[i].gpio) {
            gpio_free(capture->pdrvdata->pgdata[i].gpio);
        }
    }
}

void capture_delay_work_cancel(struct gpio_capture_drv *capture)
{
    int i;

    for (i=0; i<capture->pdrvdata->gpio_nums; i++) {
        if (delayed_work_pending(&capture->pdrvdata->pgdata[i].work))
            cancel_delayed_work(&capture->pdrvdata->pgdata[i].work);
    }
}

void capture_disable_irq_wake(struct gpio_capture_drv *capture)
{
    int i;

    for (i=0; i<capture->pdrvdata->gpio_nums; i++) {
        if (capture->pdrvdata->pgdata[i].irq) {
            disable_irq_wake(capture->pdrvdata->pgdata[i].irq);
        }
    }
}

void capture_enable_irq_wake(struct gpio_capture_drv *capture)
{
    int i;

    for (i=0; i<capture->pdrvdata->gpio_nums; i++) {
        if (capture->pdrvdata->pgdata[i].irq) {
            enable_irq_wake(capture->pdrvdata->pgdata[i].irq);
        }
    }
}

/*******************************************************/

static ssize_t capture_info_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct gpio_capture_drv *capture;
    struct gpio_capture_platform_driver_data *pdrvdata;
    unsigned long flags;
    int i;
    int read_len;
    int show_len;

    if (!dev)
        return -ENODEV;

    capture = dev_get_drvdata(dev);
    if (!capture)
        return -ENODEV;

    pdrvdata = capture->pdrvdata;
    read_len = 0; 
    for (i=0; i<pdrvdata->gpio_nums; i++) {
       
   //     mutex_lock(&pdrvdata->pgdata[i].lock);
        spin_lock_irqsave(&pdrvdata->pgdata[i].lock, flags);
     
        if (pdrvdata->pgdata[i].valid_state == 1) {
            read_buf[read_len++] = (pdrvdata->pgdata[i].state << 16) | pdrvdata->pgdata[i].gpio;
            read_buf[read_len++] = pdrvdata->pgdata[i].count;
            read_buf[read_len++] = pdrvdata->pgdata[i].time;
        }

        spin_unlock_irqrestore(&pdrvdata->pgdata[i].lock, flags);
  //      mutex_unlock(&pdrvdata->pgdata[i].lock);
    }

    show_len = 0;
    show_len += sprintf(&buf[show_len], "state gpio count time\n");
    
    for (i=0; i<read_len; i+=3) {
        show_len += sprintf(&buf[show_len], " %d   %d   %d   %u\n", read_buf[i]>>16, read_buf[i]&0x000000ff, read_buf[i+1], read_buf[i+2]);
    }
    return show_len;
}

static DEVICE_ATTR(capture, S_IRUGO, capture_info_show, NULL);

static struct attribute *gpio_cap_attrs[] = {
    &dev_attr_capture.attr,
    NULL
};

ATTRIBUTE_GROUPS(gpio_cap);

/******************************************************/

static struct gpio_capture_platform_driver_data *gpio_capture_parse_dt(struct device *dev)
{
    struct device_node *np = dev->of_node;
    enum of_gpio_flags flags;
    int err;
    struct gpio_capture_platform_driver_data *pdrvdata;   
    int gpio_nums;
    int gpio_debounce;
    unsigned int ;
    int i;
 
    if (!np)
        return NULL;

    gpio_nums = of_gpio_count(np);
    if (!gpio_nums)           
        return NULL;    
 
    printk("gpio_nums = %d\n", gpio_nums);
    
    pdrvdata = devm_kzalloc(dev, sizeof(struct gpio_capture_platform_driver_data) + (sizeof(struct gpio_data) * gpio_nums), GFP_KERNEL);
    if (!pdrvdata)
        return NULL;
    
    err = of_property_read_u32(dev->of_node, "debounce-us", &gpio_debounce);
    if (err)
        gpio_debounce = 0;
    
    printk("debounce=%d\n", gpio_debounce);
    
    pdrvdata->gpio_nums = gpio_nums;
    for (i=0; i<gpio_nums; i++) {
        pdrvdata->pgdata[i].gpio = of_get_gpio_flags(np, i, &flags);
        pdrvdata->pgdata[i].debounce_time = gpio_debounce * 1000; // us to ns // usecs_to_jiffies(gpio_debounce);
        
        printk("gpio=%d\n", pdrvdata->pgdata[i].gpio);
    }

    err = of_property_read_u32(dev->of_node, "irq-type", &pdrvdata->irq_type);
    if (err)
        pdrvdata->irq_type = 1;
    
    printk("irq_type=%d\n", pdrvdata->irq_type);
    
    return pdrvdata;
}

static struct gpio_capture_platform_driver_data *gpio_capture_parse(struct device *dev)
{
    struct gpio_capture_platform_driver_data *pdrvdata;   
    struct gpio_capture_platform_device_data *pdevdata = dev_get_platdata(dev); 
    int gpio_nums;
    int i;

    gpio_nums = pdevdata->gpio_nums;
    if (!gpio_nums)
        return NULL;    
    
    printk("gpio_nums = %d\n", gpio_nums);
    
    pdrvdata = devm_kzalloc(dev, sizeof(struct gpio_capture_platform_driver_data) + (sizeof(struct gpio_data) * gpio_nums), GFP_KERNEL);
    if (!pdrvdata)
        return NULL;
    
    for (i=0; i<gpio_nums; i++) {
        pdrvdata->pgdata[i].gpio = pdevdata->pgpio[i];
        pdrvdata->pgdata[i].debounce_time = pdevdata->debounce_us * 1000; // us to ns 

        printk("%d:gpio=%d\n", i, pdrvdata->pgdata[i].gpio);
        printk("debounce_time=%d\n", pdrvdata->pgdata[i].debounce_time);
    }
    pdrvdata->gpio_nums = gpio_nums;

    pdrvdata->irq_type = pdevdata->irq_type;
    if (pdrvdata->irq_type)
        pdrvdata->irq_type = 1;
    
    printk("irq_type=%d\n", pdrvdata->irq_type);
    
    return pdrvdata;
}

static int gpio_capture_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct gpio_capture_platform_driver_data *pdrvdata;
    const struct of_device_id *of_id = of_match_device(gpio_capture_of_match, dev);
    struct gpio_capture_drv *capture;
    int err;
    int i;

    capture = kzalloc(sizeof(struct gpio_capture_drv), GFP_KERNEL);
    if (!capture) {
        err = -ENOMEM;
        goto err_exit_free_mem;
    }

    if (of_id) {
        printk("probe dt\n");
        pdrvdata = gpio_capture_parse_dt(dev);
    } else {
        printk("probe plt dev\n");
        pdrvdata = gpio_capture_parse(dev);
    }
    
    if (!pdrvdata) {
        dev_err(dev, "missing platform driver data\n");
        return -EINVAL;
    }

    capture->pdrvdata = pdrvdata;

    for (i=0; i<capture->pdrvdata->gpio_nums; i++) {
        
        err = gpio_request_one(pdrvdata->pgdata[i].gpio, GPIOF_IN, dev_name(dev));
        if (err) {
            dev_err(dev, "unable to request GPIO %d\n", capture->pdrvdata->pgdata[i].gpio);
            goto err_exit_free_mem;
        }

        capture->pdrvdata->pgdata[i].irq = gpio_to_irq(capture->pdrvdata->pgdata[i].gpio);
        err = request_irq(capture->pdrvdata->pgdata[i].irq, &gpio_capture_irq, pdrvdata->irq_type, DRV_NAME, &capture->pdrvdata->pgdata[i]);
        if (err) {
            dev_err(dev, "unable to request IRQ %d\n", capture->pdrvdata->pgdata[i].irq);
            goto err_exit_free_gpio;
        }

        capture->pdrvdata->pgdata[i].time = jiffies;
        capture->pdrvdata->pgdata[i].last_time = capture->pdrvdata->pgdata[i].time;
        capture->pdrvdata->pgdata[i].valid_state = 0;
        capture->pdrvdata->pgdata[i].notify_task = NULL;

   //     INIT_DELAYED_WORK(&capture->pdrvdata->pgdata[i].work, gpio_capture_delayed_work);
  
        /* hi-res timer */ 
        hrtimer_init(&capture->pdrvdata->pgdata[i].timer, CLOCK_REALTIME, HRTIMER_MODE_ABS);
        capture->pdrvdata->pgdata[i].timer.function = gpio_debounce_timer_fun;
 
        spin_lock_init(&capture->pdrvdata->pgdata[i].lock);
   //     mutex_init(&capture->pdrvdata->pgdata[i].lock);
    }
#if (GPIO_DEBUG == 1)
    err = gpio_request_one(200, GPIOF_DIR_OUT, dev_name(dev));
    if (err) {
        dev_err(dev, "unable to request GPIO 200\n");
    }
#endif 
    capture->miscdev.minor  = MISC_DYNAMIC_MINOR;
    capture->miscdev.name   = dev_name(dev);
    capture->miscdev.fops   = &gpio_capture_fops;
    capture->miscdev.parent = dev;

    err = misc_register(&capture->miscdev);
    if (err) {
        dev_err(dev, "register misc device fail\n");
        goto err_exit_free_irq;
    }

    dev_set_drvdata(dev, capture);
    dev_info(dev, "new misc device %s\n", capture->miscdev.name );

    err = sysfs_create_group(&pdev->dev.kobj, &gpio_cap_group);
    if (err) {
        printk(KERN_ERR "device file failed!\n");
        err = -EINVAL;
        goto err_create_sysfs;
    }
    return 0;

err_create_sysfs:
err_exit_free_irq:
    capture_free_irq(capture);
err_exit_free_gpio:
    capture_free_gpio(capture);
err_exit_free_mem:
    kfree(capture);
    return err;
}

static int gpio_capture_remove(struct platform_device *pdev)
{
    struct gpio_capture_drv *capture = platform_get_drvdata(pdev);
    
    device_init_wakeup(&pdev->dev, false);

    capture_delay_work_cancel(capture);

    sysfs_remove_group(&pdev->dev.kobj, &gpio_cap_group);
    
    misc_deregister(&capture->miscdev);
   
    capture_free_irq(capture);
    capture_free_gpio(capture);
    
    kfree(capture);

    platform_set_drvdata(pdev, NULL);

    return 0;
}

static int gpio_capture_suspend(struct device *dev)
{
    struct gpio_capture_drv *capture = dev_get_drvdata(dev);
    
    if (device_may_wakeup(dev))
        capture_disable_irq_wake(capture);
    
    return 0;
}

static int gpio_capture_resume(struct device *dev)
{
    struct gpio_capture_drv *capture = dev_get_drvdata(dev);
    
    if (device_may_wakeup(dev))
        capture_enable_irq_wake(capture);
    
    return 0;
}

static SIMPLE_DEV_PM_OPS(gpio_capture_pm_ops,
         gpio_capture_suspend, gpio_capture_resume);

static struct platform_driver gpio_capture_driver = {
    .probe		= gpio_capture_probe,
    .remove		= gpio_capture_remove,
    .driver		= {
        .name	= DRV_NAME,
        .pm	= &gpio_capture_pm_ops,
        .of_match_table = of_match_ptr(gpio_capture_of_match),
    }
};
module_platform_driver(gpio_capture_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Generic GPIO handle driver");
MODULE_AUTHOR("nuvoton");
