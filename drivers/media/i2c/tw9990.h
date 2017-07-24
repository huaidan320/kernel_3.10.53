#ifndef __TW9990_DRIVER_H__
#define __TW9990_DRIVER_H__

#define TW9990_DEBUG

#ifdef TW9990_DEBUG
#define TW9990_INFO(fmt, arg...)  		printk("<<-TW9990-INFO->> "fmt"\n",##arg)
#define TW9990_ERROR(fmt,arg...)		printk("<<-TW9990-ERROR->> "fmt"\n", ##arg)
#else
#define TW9990_INFO(fmt, arg...)
#define TW9990_ERROR(fmt,arg...)
#endif

#define IMX_GPIO_NR(bank, nr)       (((bank) - 1) * 32 + (nr))

#define TW9990_PWN		IMX_GPIO_NR(6, 9) 
#define TW9990_RST		IMX_GPIO_NR(1, 4) 

/*
 * register offset
 */
#define TW9990_ID		0x00 /* Product ID Code Register */
#define TW9990_STATUS1		0x01 /* Chip Status Register I */

#endif

