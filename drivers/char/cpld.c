/*
 *  cpld.c
 *
 *  Author              pxh
 *  Email               peixiuhui@163.com
 *  Create time         2015-08-14
 *  kernel version		3.10.53
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include <linux/module.h> 
#include <linux/init.h>
#include <linux/delay.h> 
#include <asm/irq.h> 
#include <linux/kernel.h> 
#include <linux/mm.h> 
#include <linux/fs.h> 
#include <linux/types.h> 
#include <linux/delay.h> 
#include <linux/slab.h> 
#include <linux/errno.h> 
#include <linux/ioctl.h> 
#include <linux/cdev.h> 
#include <asm/uaccess.h> 
#include <asm/atomic.h> 
#include <asm/unistd.h> 
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <asm/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>

#define GPIO_IOC_MAGIC					'P'
#define IOCTL_GPIO_SETOUTPUT				_IOW(GPIO_IOC_MAGIC, 0, int)
#define IOCTL_GPIO_SETINPUT				_IOW(GPIO_IOC_MAGIC, 1, int)
#define IOCTL_GPIO_SETVALUE				_IOW(GPIO_IOC_MAGIC, 2, int)
#define IOCTL_GPIO_GETVALUE				_IOR(GPIO_IOC_MAGIC, 3, int)
#define GPIO_IOC_MAXNR					3

typedef struct {
	int pin;
	int data;
	int usepullup;
} user_gpio_arg;
 

struct user_gpio {
	unsigned int *gpios;
	int gpio_num;
	struct cdev gpio_cdev;
};

static int gpio_major = 0;
static int gpio_minor = 0;
static struct class *gpio_class;
static struct device *class_dev;
static void __iomem *cpld_iobase;

static dev_t devt;
static struct cdev cdev;
static struct class *cpld_class;

#define DEV_NAME	"cpld"

static int cpld_open(struct inode *inode, struct file *filp)
{

	return 0;
}

static int cpld_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static ssize_t cpld_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
        u16 temp[2]={0};

        if ((count != 2) && (count != 4))
                return -1;

	temp[0] = readw(cpld_iobase);
	if (count == 4) {
		temp[1] = readw(cpld_iobase + 4);
	}

//        printk("%s() read 0x%x 0x%x form cpld\n", __func__, temp[1], temp[0]);
	if (copy_to_user(buf, temp, count)) {
                return -1;
        }

        return 0;

}

ssize_t cpld_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
	u16 temp[2]={0};
	
	if ((count != 2) && (count != 4)) 
		return -1;

	if (copy_from_user(temp, buf, count)) {
		return -1;
	}

	writew(temp[0], cpld_iobase);
	
	if (count == 4) {
		writew(temp[1], cpld_iobase + 4);
	}

//	printk("%s() write 0x%x 0x%x to cpld\n", __func__, temp[1], temp[0]);
	return 0;
}



static long cpld_ioctl(struct file *filp,  unsigned int cmd,  unsigned long arg)
{
	int ret = 0, i;
/*
	user_gpio_arg * parg = (user_gpio_arg*) arg;  

	struct user_gpio *dev_info = filp->private_data; 


	if (_IOC_TYPE(cmd) != GPIO_IOC_MAGIC) return -ENOTTY;
	if (_IOC_NR(cmd) > GPIO_IOC_MAXNR) return -ENOTTY;

	if (_IOC_DIR(cmd) & _IOC_READ)
		ret = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		ret =  !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));

	if (ret) return -EFAULT;
*/
	return ret;
}

struct file_operations cpld_fops = {
	.owner =    THIS_MODULE,
	.read =     cpld_read,
	.write =    cpld_write,
	.open =     cpld_open,
	.unlocked_ioctl =	cpld_ioctl,
	.release =	cpld_release,
};

static int cpld_probe(struct platform_device *pdev)
{
	int err, i;
	dev_t dev = 0;
	int iosize;

	struct device_node *np = pdev->dev.of_node;

	struct resource	*addr_res;
	printk("===name:%s\n",np->name);
	
	addr_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (addr_res == NULL) {
		err = -EINVAL;
		dev_err(&pdev->dev, "error getting memory resource\n");
		return err;
	}
	
	iosize = resource_size(addr_res);
	printk("res %s [0x%x-0x%x] \n", addr_res->name, addr_res->start, addr_res->end);
	if (!request_mem_region(addr_res->start, iosize, pdev->name)) {
		dev_err(&pdev->dev, "cannot claim address reg area\n");
		return -EIO;
	}

	cpld_iobase = ioremap(addr_res->start, iosize);

	if (cpld_iobase == NULL) {
		dev_err(&pdev->dev, "failed to ioremap address reg\n");
		return -EINVAL;
	}
	printk("cpld map 0x%x\n", cpld_iobase);
        err = alloc_chrdev_region(&devt, 0, 1, DEV_NAME);
        if (err < 0) {
                printk(KERN_ERR"%s: failed to allocate dev region\n", __FILE__);
        	iounmap(cpld_iobase);
		return -EINVAL;
	}

        cpld_class = class_create(THIS_MODULE, DEV_NAME);
        device_create(cpld_class, NULL, devt, 0, DEV_NAME);

        if(IS_ERR(cpld_class)) {
                return PTR_ERR(cpld_class);
        }

        cdev_init(&cdev, &cpld_fops);
        cdev.owner = THIS_MODULE;
        err = cdev_add(&cdev, devt, 1);

	return 0;
}


static void cpld_remove(struct platform_device *pdev)
{
	/// gpio free
	int i;
	struct user_gpio *us_gpio = platform_get_drvdata(pdev);
	for (i = 0; i < us_gpio->gpio_num; i++) {
		gpio_free(us_gpio->gpios[i]);
	}

	if (!us_gpio->gpios)
		kfree(us_gpio->gpios);
	if (!us_gpio)
		kfree(us_gpio);
}

static struct of_device_id cpld_of_match[] = {
	{ .compatible = "imx6,cpld", },
	{ },
};

static struct platform_driver cpld_device_driver = {
	.probe		= cpld_probe,
	.remove		= cpld_remove,
	.driver		= {
		.name	= "imx6,cpld",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(cpld_of_match),
	}
};


static int __init cpld_init(void)
{
	return platform_driver_register(&cpld_device_driver);
}

static void __exit cpld_exit(void)
{
	dev_t dev = 0;

	dev = MKDEV(gpio_major, gpio_minor);

	device_del(class_dev);
	class_destroy(gpio_class);
	unregister_chrdev_region(dev, 0); 
	///cdev_del(&gpio_cdev);
}


module_init(cpld_init);

module_exit(cpld_exit);


MODULE_DESCRIPTION("Driver for CPLD");
MODULE_AUTHOR("peixiuhui@163.com");
MODULE_LICENSE("GPL");

