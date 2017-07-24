#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <asm/irq.h>
//#include <mach/hardware.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/ioctl.h>
#include <linux/cdev.h>
#include <linux/string.h>
#include <asm/uaccess.h>
#include <asm/atomic.h>
#include <asm/unistd.h>
#include <linux/gpio.h>

#define IMX_GPIO_NR(bank, nr)       (((bank) - 1) * 32 + (nr))

#define DEVICE_NAME "ak7757"

#define     AK7757_IOC_MAGIC                'K'

#define     IOCTL_AK7757_RESET_HIGH       _IO(AK7757_IOC_MAGIC, 1)
#define		IOCTL_AK7757_RESET_LOW		  _IO(AK7757_IOC_MAGIC, 2)
#define		IOCTL_AK7757_SPICS_HIGH		  _IO(AK7757_IOC_MAGIC, 3)
#define		IOCTL_AK7757_SPICS_LOW		  _IO(AK7757_IOC_MAGIC, 4)


static  int ak7757_open(struct inode *dev, struct file *file)
{
    //printk("%s\n", __func__);
    if(file->f_mode & FMODE_WRITE){
        //printk()
    }
    if(file->f_mode & FMODE_READ){
        //printk()
    }

    if(file->f_flags & O_NONBLOCK)
        pr_debug("open: non-blocking\n");
    else
        pr_debug("open: blocking\n");
 
//	gpio_direction_output(IMX_GPIO_NR(4, 25), 1);
	gpio_direction_output(IMX_GPIO_NR(4, 24), 1);

	return 0;
}



static long ak7757_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int ret = 0;

    //printk("%s\n", __func__);

    switch(cmd)
    {
        case IOCTL_AK7757_RESET_HIGH:
             gpio_set_value(IMX_GPIO_NR(4, 25), 1);
             printk("AK7757 reset high\n");
             break;
        case IOCTL_AK7757_RESET_LOW:
             gpio_set_value(IMX_GPIO_NR(4, 25), 0);
             printk("AK7757 reset low\n");
             break;
        case IOCTL_AK7757_SPICS_HIGH:
             gpio_set_value(IMX_GPIO_NR(4, 24), 1);
             printk("AK7757 spi_cs high\n");
             break;
        case IOCTL_AK7757_SPICS_LOW:
             gpio_set_value(IMX_GPIO_NR(4, 24), 0);
             printk("AK7757 spi_cs low\n");
             break;
        default:
             printk("Err: unknow cmd:0x%X,[0x%X, 0x%X, 0x%X, 0x%X]\n", cmd, IOCTL_AK7757_RESET_HIGH, IOCTL_AK7757_RESET_LOW, IOCTL_AK7757_SPICS_HIGH, IOCTL_AK7757_SPICS_LOW);
            return -EINVAL;
    }
    return ret;
}


static ssize_t  ak7757_write(struct file *file, const char __user *buf,
                 size_t count, loff_t *f_pos)
{
    char  cmd[1];

    if(1==count){
        if (copy_from_user(cmd, (void __user *)buf, sizeof(cmd))){
            printk("Err: invalid param\n");
            return -EINVAL;
        }
        if(cmd[0] == 1){
                 gpio_set_value(IMX_GPIO_NR(4, 25), 1);
                 printk("AK7757 reset high\n");
        }else if(2 == cmd[0]){
                 gpio_set_value(IMX_GPIO_NR(4, 25), 0);
                 printk("AK7757 reset low\n");
        }else if(3 == cmd[0]){
                 gpio_set_value(IMX_GPIO_NR(4, 24), 1);
                 printk("AK7757 spi_cs high\n");
        }else if(4 == cmd[0]){
                 gpio_set_value(IMX_GPIO_NR(4, 24), 0);
                 printk("AK7757 spi_cs low\n");
        }else{
            printk("Err: invalid param\n");
            return -EINVAL;
        }
    }else if (4==count){
        //reading.

    }else if(8==count){
        //writing.
    }else{
        //error
        printk("Err: invalid param len.\n");
    }
    return 0;
}

static struct file_operations dev_fops = {
    .owner  =   THIS_MODULE,
    .open  = ak7757_open,
    .write = ak7757_write,
    .unlocked_ioctl  =   ak7757_ioctl,
};

static struct miscdevice misc = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = DEVICE_NAME,
    .fops  = &dev_fops,
};

static int __init dev_init(void)
{
    int ret;

    ret = misc_register(&misc);
    printk ("%s initialized    ---===>>>\n",DEVICE_NAME);

//	gpio_direction_output(IMX_GPIO_NR(4, 25), 1);
//	gpio_direction_output(IMX_GPIO_NR(4, 24), 1);

    return ret;
}

static void __exit dev_exit(void)
{
    misc_deregister(&misc);
    printk ("%s UNloaded  ---===>>>\n",DEVICE_NAME);

}

module_init(dev_init);
module_exit(dev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("AK7757");
MODULE_DESCRIPTION("AK7757 Reset");



