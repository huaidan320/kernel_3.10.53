/*
 *  user_gpio.c -- Support users to use GPIO
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

#define GPIO_IOC_MAGIC					'G'
#define IOCTL_GPIO_SETOUTPUT			_IOW(GPIO_IOC_MAGIC, 0, int)
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


static int gpio_open(struct inode *inode, struct file *filp)
{
	struct user_gpio *dev_info;		/* device information */

	dev_info = container_of(inode->i_cdev, struct user_gpio, gpio_cdev);
	filp->private_data = dev_info;	/* for other methods */

	return 0;
}

static int gpio_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static ssize_t gpio_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
	return 0;
}

ssize_t gpio_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
	return 0;
}


static loff_t gpio_llseek(struct file *filp, loff_t offset, int origin)
{
	return 0;
}

static long gpio_ioctl(struct file *filp,  unsigned int cmd,  unsigned long arg)
{
	int ret = 0, i;

	user_gpio_arg * parg = (user_gpio_arg*) arg;  

	struct user_gpio *dev_info = filp->private_data; 


	if (_IOC_TYPE(cmd) != GPIO_IOC_MAGIC) return -ENOTTY;
	if (_IOC_NR(cmd) > GPIO_IOC_MAXNR) return -ENOTTY;

	if (_IOC_DIR(cmd) & _IOC_READ)
		ret = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		ret =  !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));

	if (ret) return -EFAULT;

	/// To determine whether the user is GPIO
	for (i = 0; i < dev_info->gpio_num; i++) {
		if (dev_info->gpios[i] == parg->pin) {
			break;
		}
	}

	if (i > dev_info->gpio_num) {
		printk(KERN_WARNING "Unsuported Gpio Num\n");
		return -EFAULT;
	}

	switch (cmd) {

	case IOCTL_GPIO_SETOUTPUT:
			gpio_direction_output(parg->pin, parg->data);
		break;	

	case IOCTL_GPIO_SETINPUT:
			gpio_direction_input(parg->pin);
		break;

	case IOCTL_GPIO_SETVALUE:
			gpio_set_value(parg->pin, parg->data);
		break;

	case IOCTL_GPIO_GETVALUE:
			parg->data = gpio_get_value(parg->pin) ? 1 : 0;
		break;

	default :
		printk(KERN_WARNING "Unsuported Ioctl Cmd: 0x%x\n", cmd);
	}
	return ret;
}

struct file_operations gpio_fops = {
	.owner =    THIS_MODULE,
	.read =     gpio_read,
	.write =    gpio_write,
	.llseek =   gpio_llseek,
	.open =     gpio_open,
	.unlocked_ioctl =	gpio_ioctl,
	.release =	gpio_release,
};

static int user_gpio_probe(struct platform_device *pdev)
{
	int err, i;
	dev_t dev = 0;

	struct device_node *np = pdev->dev.of_node;
	struct user_gpio *us_gpio;

	us_gpio = kzalloc(sizeof(struct user_gpio), GFP_KERNEL);
	if (!us_gpio) {
		dev_err(&pdev->dev, "Could not allocate memory for platform data\n");
		return -ENOMEM;
	}

	us_gpio->gpio_num = of_gpio_named_count(np, "pin_gpios");
	if (us_gpio->gpio_num > 205) {
		err = -EINVAL;
		goto err_free_mem1;
	}
	
	us_gpio->gpios = devm_kzalloc(&pdev->dev, sizeof(unsigned int) * us_gpio->gpio_num, GFP_KERNEL);
	if (!us_gpio->gpios) {
		dev_err(&pdev->dev, "Could not allocate memory for platform data\n");
		err = -ENOMEM;
		goto err_free_mem1;
	}

	printk("GPIO Registered:");
	for (i = 0; i < us_gpio->gpio_num; i++) {
		us_gpio->gpios[i] = of_get_named_gpio(np, "pin_gpios", i);
		printk("%d ", us_gpio->gpios[i]);
	}
	printk("\n");
	/// request for all users GPIO, default gpio set input
	for (i = 0; i < us_gpio->gpio_num; i++) {
		err = gpio_request(us_gpio->gpios[i], "user_gpio");
		if (err) {
			dev_err(&pdev->dev, "Failed to request GPIO%d for num %d\n",
				us_gpio->gpios[i], i);
			goto err_free_pin;
		}

		gpio_direction_input(us_gpio->gpios[i]);
	}

	if (gpio_major) {
		dev = MKDEV(gpio_major, gpio_minor);
		err = register_chrdev_region(dev, 1, "user_gpio");
	} else {
		err = alloc_chrdev_region(&dev, gpio_minor, 1, "user_gpio");
		gpio_major = MAJOR(dev);
	}

	if (err < 0) {
		printk(KERN_WARNING "Can't get major %d\n", gpio_major);
		goto err_free_pin;
	}

	cdev_init(&us_gpio->gpio_cdev, &gpio_fops);
	us_gpio->gpio_cdev.owner = THIS_MODULE;
	us_gpio->gpio_cdev.ops = &gpio_fops;
	err = cdev_add(&us_gpio->gpio_cdev, dev, 1);

	/// derves node file
	gpio_class = class_create(THIS_MODULE, "user_gpio");
	if (IS_ERR(gpio_class)) {
		printk(KERN_WARNING "class_create faild\n");
		cdev_del(&us_gpio->gpio_cdev);
		unregister_chrdev_region(dev, 0);
		goto err_free_pin;
	}

	class_dev = device_create(gpio_class, NULL, dev, 0, "user_gpio");

	platform_set_drvdata(pdev, us_gpio);

	return 0;
err_free_pin:
	for (i--; i >= 0; i--) {
		gpio_free(us_gpio->gpios[i]);
	}

	kfree(us_gpio->gpios);
err_free_mem1:
	kfree(us_gpio);

	return err;
}


static void user_gpio_remove(struct platform_device *pdev)
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

static struct of_device_id user_gpio_of_match[] = {
	{ .compatible = "user_gpio", },
	{ },
};

static struct platform_driver user_gpio_device_driver = {
	.probe		= user_gpio_probe,
	.remove		= user_gpio_remove,
	.driver		= {
		.name	= "user_gpio",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(user_gpio_of_match),
	}
};


static int __init user_gpio_init(void)
{
	return platform_driver_register(&user_gpio_device_driver);
}

static void __exit user_gpio_exit(void)
{
	dev_t dev = 0;

	dev = MKDEV(gpio_major, gpio_minor);

	device_del(class_dev);
	class_destroy(gpio_class);
	unregister_chrdev_region(dev, 0); 
	///cdev_del(&gpio_cdev);
}


module_init(user_gpio_init);

module_exit(user_gpio_exit);


MODULE_DESCRIPTION("Driver for User GPIO");
MODULE_AUTHOR("peixiuhui@163.com");
MODULE_LICENSE("GPL");

