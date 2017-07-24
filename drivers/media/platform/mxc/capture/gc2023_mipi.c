/*
 * Copyright (C) 2011-2014 Freescale Semiconductor, Inc. All Rights Reserved.
 */

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/ctype.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/of_device.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/regulator/consumer.h>
#include <linux/fsl_devices.h>
#include <linux/mipi_csi2.h>
#include <media/v4l2-chip-ident.h>
#include <media/v4l2-int-device.h>
#include "mxc_v4l2_capture.h"

#define GC2023_VOLTAGE_ANALOG               2800000
#define GC2023_VOLTAGE_DIGITAL_CORE    1500000
#define GC2023_VOLTAGE_DIGITAL_IO         1800000

#define MIN_FPS 15
#define MAX_FPS 30
#define DEFAULT_FPS 30

#define GC2023_XCLK_MIN 6000000
#define GC2023_XCLK_MAX 24000000

#define GC2023_CHIP_ID_HIGH_BYTE	0x300A
#define GC2023_CHIP_ID_LOW_BYTE		0x300B

enum gc2023_mode {
	gc2023_mode_MIN = 0,
	gc2023_mode_1080P_1920_1080 = 1,
	gc2023_mode_MAX = 2,
	gc2023_mode_INIT = 0xff, /*only for sensor init*/
};

enum gc2023_frame_rate {
	gc2023_15_fps,
	gc2023_30_fps
};

static int gc2023_framerates[] = {
	[gc2023_15_fps] = 15,
	[gc2023_30_fps] = 30,
};

/* image size under 1280 * 960 are SUBSAMPLING
 * image size upper 1280 * 960 are SCALING
 */
enum gc2023_downsize_mode {
	SUBSAMPLING,
	SCALING,
};

struct reg_value {
	u16 u16RegAddr;
	u8 u8Val;
	u8 u8Mask;
	u32 u32Delay_ms;
};

struct gc2023_mode_info {
	enum gc2023_mode mode;
	enum gc2023_downsize_mode dn_mode;
	u32 width;
	u32 height;
	struct reg_value *init_data_ptr;
	u32 init_data_size;
};

/*!
 * Maintains the information on the current state of the sesor.
 */
static struct sensor_data gc2023_data;
static int pwn_gpio, rst_gpio;

static unsigned char gc2023_register_array[][2] = {
	// --SYS--
	{0xf2 ,0x00}, //sync_pad_io_ebi
	{0xf6 ,0x00}, //up down
	{0xfc ,0x06},
	{0xf7 ,0x01}, //pll enable
	{0xf8 ,0x07}, //Pll mode 2
	{0xf9 ,0x06}, //[0] pll enable
	{0xfa ,0x00}, //div
	{0xfc ,0x0e},

       // ---ANALOG & CISCTL---
	{0xfe, 0x00},
	{0x03, 0x03},
	{0x04, 0xf6},		
	{0x05, 0x02}, // HB
	{0x06, 0xc6}, 
	{0x07, 0x00},// VB
	{0x08, 0x10},		
	{0x09, 0x00},
	{0x0a, 0x00},//row start
	{0x0b, 0x00},
	{0x0c, 0x00},// col start		
	{0x0d, 0x04},
	{0x0e, 0x40},// height 1088
	{0x0f, 0x07},		
	{0x10, 0x88},// width 1928
	{0x17, 0x54},
	{0x18, 0x02},
	{0x19, 0x0d},		
	{0x1a, 0x18},

	{0x20, 0x54},
	{0x23, 0xf0}, //1:vpix
	{0x24, 0xc1},
	{0x25, 0x18},
	{0x26, 0x64},
	
	{0x28, 0xe8},
	{0x29, 0x08},
	{0x2a, 0x08},
	{0x2b, 0x48},	
	{0x2f, 0x40},
	
	{0x30, 0x99},
	{0x34, 0x00},
	{0x38, 0x80},
	{0x3b, 0x12},
	{0x3d, 0xb0},
	{0xcc, 0x8a}, //[3]enbc2nb2clamp
	{0xcd, 0x99},
	{0xcf, 0x70},
	{0xd0, 0x9c},
	{0xd2, 0xc1},
	{0xd8, 0x80},
	{0xda, 0x28},
	{0xdc, 0x24},
	{0xe1, 0x14},
	{0xe3, 0xf0},
	{0xe4, 0xfa},
	{0xe6, 0x1f},
	{0xe8, 0x02},
	{0xe9, 0x02},
	{0xea, 0x03},
	{0xeb, 0x03},
       // ---ISP---
	{0xfe, 0x00},
	{0x80, 0x5c},
	{0x88, 0x73},		
	{0x89, 0x03}, // [4] pregain bypass
	{0x90, 0x01},
	{0x92, 0x02}, // crop win y
	{0x94, 0x00}, // crop wn x
	{0x95, 0x04}, // crop win height
	{0x96, 0x38},
	{0x97, 0x07}, // cropo win width
	{0x98, 0x80},
	
       // ---BLK---
	{0xfe, 0x00},
	{0x40, 0x22},
	{0x43, 0x07},	
	{0x4e, 0x3c},		
	{0x4f, 0x00},
	{0x60, 0x00},
	{0x61, 0x80},	

       // ---GAIN---
	{0xfe, 0x00},
	{0xb0, 0x58},
	{0xb1, 0x01},		
	{0xb2, 0x00},
//	{0xb6, 0x00},

	{0xb6, 0x03},
	
	{0xfe, 0x01},
	{0x01, 0x00},		
	{0x02, 0x01},
	{0x03, 0x02},
	{0x04, 0x03},
	{0x05, 0x04},		
	{0x06, 0x05},
	{0x07, 0x06},		
	{0x08, 0x0e},
	{0x09, 0x16},
	{0x0a, 0x1e},
	{0x0b, 0x36},		
	{0x0c, 0x3e},
	{0x0d, 0x56},
	{0x0e, 0x5e},		
       // ---DNDD---
	{0xfe, 0x02},
	{0x81, 0x05},
       // ---DARK SUN---
	{0xfe, 0x01},
	{0x54, 0x77},
	{0x58, 0x00},
	{0x5a, 0x05},
       // ---MIPI---
	{0xfe, 0x03},
	{0x01, 0x5f},
	{0x02, 0x10},
//   {0x02, 0x17},  //  ok

//	{0x03, 0x9a},
       {0x03, 0xfa},	// ok important!!
	{0x04, 0x80},	
	{0x05, 0x00},		

	{0x10, 0x95},
	{0x11, 0x2a},
	{0x12, 0x80}, // lwc 1920*5/4
	{0x13, 0x07},
//	{0x15, 0x06},  
       {0x15, 0x04},     // can receive lane clock  ok important!!
	{0x36, 0x88},

	{0x21, 0x08},
	{0x22, 0x02},
	{0x23, 0x10},
	{0x24, 0x01},
	{0x25, 0x10},
	{0x26, 0x04},

	{0x29, 0x03},
	{0x2a, 0x04},
	{0x2b, 0x04},
	
	{0xfe, 0x00},
};

static int gc2023_probe(struct i2c_client *adapter,
				const struct i2c_device_id *device_id);
static int gc2023_remove(struct i2c_client *client);

//static s32 gc2023_read_reg(u16 reg, u8 *val);
//static s32 gc2023_write_reg(u16 reg, u8 val);

static const struct i2c_device_id gc2023_id[] = {
	{"gc2023_mipi", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, gc2023_id);

static struct i2c_driver gc2023_i2c_driver = {
	.driver = {
		  .owner = THIS_MODULE,
		  .name  = "gc2023_mipi",
		  },
	.probe  = gc2023_probe,
	.remove = gc2023_remove,
	.id_table = gc2023_id,
};

static void gc2023_standby(s32 enable)
{
/*
	if (enable)
		gpio_set_value(pwn_gpio, 1);
	else
		gpio_set_value(pwn_gpio, 0);

	msleep(2);
	*/
}

static void gc2023_reset(void)
{
	/* camera reset */
	/*-------power off----------*/

	/* camera power dowmn */
	gpio_set_value(pwn_gpio, 0);
	msleep(5);
	gpio_set_value(pwn_gpio, 1);
	msleep(5);
	gpio_set_value(rst_gpio, 0);
	msleep(5);

	/*-------power on----------*/
	gpio_set_value(pwn_gpio, 0);	
       msleep(5);
	gpio_set_value(rst_gpio, 1);
	msleep(5);
}

static inline int gc2023_read(u8 reg)
{
	int val;

	val = i2c_smbus_read_byte_data(gc2023_data.i2c_client, reg);
	if (val < 0) {
		dev_dbg(&gc2023_data.i2c_client->dev,
			"%s:read reg error: reg=%2x\n", __func__, reg);
		return -1;
	}
	return val;
}

static int gc2023_write_reg(u8 reg, u8 val)
{
	s32 ret;

	ret = i2c_smbus_write_byte_data(gc2023_data.i2c_client, reg, val);
	if (ret < 0) {
		dev_dbg(&gc2023_data.i2c_client->dev, 
			"%s:write reg error:reg=%2x,val=%2x\n", __func__, reg, val);
		return -1;
	}
	return 0;
}

static void gc2023_hard_init(void)
{
	int i = 0;
	int init_reg_array_size = sizeof(gc2023_register_array)/2;

	dev_dbg(&gc2023_data.i2c_client->dev, "In tw9990:tw9990_hard_reset\n");
	
	for (i = 0; i < init_reg_array_size; i++) 
	{
		gc2023_write_reg(gc2023_register_array[i][0], 
			gc2023_register_array[i][1]);

//		printk(" {0x%.2x, 0x%.2x} \n", gc2023_register_array[i][0], gc2023_register_array[i][1]);
	}
}

void GC2023_stream_on(void)
{
//	gc2023_write_reg(0x4202, 0x00);
}

void GC2023_stream_off(void)
{
//	gc2023_write_reg(0x4202, 0x0f);
}


int GC2023_get_sysclk(void)
{
	 /* calculate sysclk */
	return 0;
}

void GC2023_set_night_mode(void)
{
	 /* read HTS from register settings */
}

int GC2023_get_HTS(void)
{
	 /* read HTS from register settings */

	return 0;
}

int GC2023_get_VTS(void)
{
	 /* read VTS from register settings */

	return 0;
}

int GC2023_set_VTS(int VTS)
{
	 /* write VTS to registers */
	 return 0;
}

int GC2023_get_shutter(void)
{
	 /* read shutter, in number of line period */
	int shutter = 0;

	 return shutter;
}

int GC2023_set_shutter(int shutter)
{
	 /* write shutter, in number of line period */

	 return 0;
}

int GC2023_get_gain16(void)
{
	 /* read gain, 16 = 1x */
	return 0;
}

int GC2023_set_gain16(int gain16)
{
	/* write gain, 16 = 1x */
	return 0;
}

int GC2023_get_light_freq(void)
{
	return 0;
}

void GC2023_set_bandingfilter(void)
{

}

int GC2023_set_AE_target(int target)
{
	return 0;
}

void GC2023_turn_on_AE_AG(int enable)
{

}

bool binning_on(void)
{
	return true;
}

static int gc2023_init_mode(enum gc2023_frame_rate frame_rate,
			    enum gc2023_mode mode, enum gc2023_mode orig_mode)
{
	int retval = 0;
	void *mipi_csi2_info;
	u32 mipi_reg, msec_wait4stable = 0;
	
	if ((mode > gc2023_mode_MAX || mode < gc2023_mode_MIN)
		&& (mode != gc2023_mode_INIT)) {
		pr_err("Wrong gc2023 mode detected!\n");
		return -1;
	}

	mipi_csi2_info = mipi_csi2_get_info();

	/* initial mipi dphy */
	if (!mipi_csi2_info) {
		printk(KERN_ERR "%s() in %s: Fail to get mipi_csi2_info!\n",
		       __func__, __FILE__);
		return -1;
	}

	if (!mipi_csi2_get_status(mipi_csi2_info))
		mipi_csi2_enable(mipi_csi2_info);

	if (!mipi_csi2_get_status(mipi_csi2_info)) {
		pr_err("Can not enable mipi csi2 driver!\n");
		return -1;
	}

	mipi_csi2_set_lanes(mipi_csi2_info);

	/*Only reset MIPI CSI2 HW at sensor initialize*/
	if (mode == gc2023_mode_INIT)
		mipi_csi2_reset(mipi_csi2_info);

	if (gc2023_data.pix.pixelformat == V4L2_PIX_FMT_UYVY)
		mipi_csi2_set_datatype(mipi_csi2_info, MIPI_DT_YUV422);
	else if (gc2023_data.pix.pixelformat == V4L2_PIX_FMT_RGB565)
		mipi_csi2_set_datatype(mipi_csi2_info, MIPI_DT_RGB565);
	else if (gc2023_data.pix.pixelformat == V4L2_PIX_FMT_SRGGB8)
		mipi_csi2_set_datatype(mipi_csi2_info, MIPI_DT_RAW8);	
	else
		pr_err("currently this sensor format can not be supported!\n");

       msec_wait4stable = 300;	
	msleep(msec_wait4stable);

	if (mipi_csi2_info) {
		unsigned int i, j;

		i = 0;

		/* wait for mipi sensor ready */
		mipi_reg = mipi_csi2_dphy_status(mipi_csi2_info);
		while ((mipi_reg == 0x200) && (i < 10)) {
			mipi_reg = mipi_csi2_dphy_status(mipi_csi2_info);
			i++;
			msleep(10);
		}

		if (i >= 10) {
			pr_err("mipi csi2 can not receive sensor clk!\n");
			return -1;
		}

		i = 0;

		/* wait for mipi stable */
		mipi_reg = mipi_csi2_get_error1(mipi_csi2_info);
		while ((mipi_reg != 0x0) && (i < 10)) {
			mipi_reg = mipi_csi2_get_error1(mipi_csi2_info);
			i++;
			msleep(10);
		}

		j =0;

		/* wait for mipi stable */
		mipi_reg = mipi_csi2_get_error2(mipi_csi2_info);
		while ((mipi_reg != 0x0) && (j < 10)) {
			mipi_reg = mipi_csi2_get_error2(mipi_csi2_info);
			j++;
			msleep(10);
		}

		
		if (i >= 10) {
			pr_err("mipi csi2 can not reveive data correctly!\n");
			return -1;
		}
	}

	return retval;

}

/* --------------- IOCTL functions from v4l2_int_ioctl_desc --------------- */

static int ioctl_g_ifparm(struct v4l2_int_device *s, struct v4l2_ifparm *p)
{
	if (s == NULL) {
		pr_err("   ERROR!! no slave device set!\n");
		return -1;
	}

	memset(p, 0, sizeof(*p));
	p->u.bt656.clock_curr = gc2023_data.mclk;
	pr_debug(" %s  clock_curr=mclk=%d\n",__FUNCTION__,  gc2023_data.mclk);
	p->if_type = V4L2_IF_TYPE_BT656;
	p->u.bt656.mode = V4L2_IF_TYPE_BT656_MODE_NOBT_8BIT;
	p->u.bt656.clock_min = GC2023_XCLK_MIN;
	p->u.bt656.clock_max = GC2023_XCLK_MAX;
	p->u.bt656.bt_sync_correct = 1;  /* Indicate external vsync */

	return 0;
}

/*!
 * ioctl_s_power - V4L2 sensor interface handler for VIDIOC_S_POWER ioctl
 * @s: pointer to standard V4L2 device structure
 * @on: indicates power mode (on or off)
 *
 * Turns the power on or off, depending on the value of on and returns the
 * appropriate error code.
 */
static int ioctl_s_power(struct v4l2_int_device *s, int on)
{
	return 0;
}

/*!
 * ioctl_g_parm - V4L2 sensor interface handler for VIDIOC_G_PARM ioctl
 * @s: pointer to standard V4L2 device structure
 * @a: pointer to standard V4L2 VIDIOC_G_PARM ioctl structure
 *
 * Returns the sensor's video CAPTURE parameters.
 */
static int ioctl_g_parm(struct v4l2_int_device *s, struct v4l2_streamparm *a)
{
	struct sensor_data *sensor = s->priv;
	struct v4l2_captureparm *cparm = &a->parm.capture;
	int ret = 0;

	switch (a->type) {
	/* This is the only case currently handled. */
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		memset(a, 0, sizeof(*a));
		a->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		cparm->capability = sensor->streamcap.capability;
		cparm->timeperframe = sensor->streamcap.timeperframe;
		cparm->capturemode = sensor->streamcap.capturemode;
		ret = 0;
		break;

	/* These are all the possible cases. */
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
	case V4L2_BUF_TYPE_VIDEO_OVERLAY:
	case V4L2_BUF_TYPE_VBI_CAPTURE:
	case V4L2_BUF_TYPE_VBI_OUTPUT:
	case V4L2_BUF_TYPE_SLICED_VBI_CAPTURE:
	case V4L2_BUF_TYPE_SLICED_VBI_OUTPUT:
		ret = -EINVAL;
		break;

	default:
		pr_debug("   type is unknown - %d\n", a->type);
		ret = -EINVAL;
		break;
	}

	return ret;
}

/*!
 * ioctl_s_parm - V4L2 sensor interface handler for VIDIOC_S_PARM ioctl
 * @s: pointer to standard V4L2 device structure
 * @a: pointer to standard V4L2 VIDIOC_S_PARM ioctl structure
 *
 * Configures the sensor to use the input parameters, if possible.  If
 * not possible, reverts to the old parameters and returns the
 * appropriate error code.
 */
static int ioctl_s_parm(struct v4l2_int_device *s, struct v4l2_streamparm *a)
{
	struct sensor_data *sensor = s->priv;
	struct v4l2_fract *timeperframe = &a->parm.capture.timeperframe;
	u32 tgt_fps;	/* target frames per secound */
	enum gc2023_frame_rate frame_rate;
	enum gc2023_mode orig_mode;
	int ret = 0;

	/* Make sure power on */
	gc2023_standby(0);
	
	switch (a->type) {
	/* This is the only case currently handled. */
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		/* Check that the new frame rate is allowed. */
		if ((timeperframe->numerator == 0) ||
		    (timeperframe->denominator == 0)) {
			timeperframe->denominator = DEFAULT_FPS;
			timeperframe->numerator = 1;
		}

		tgt_fps = timeperframe->denominator /
			  timeperframe->numerator;

		if (tgt_fps > MAX_FPS) {
			timeperframe->denominator = MAX_FPS;
			timeperframe->numerator = 1;
		} else if (tgt_fps < MIN_FPS) {
			timeperframe->denominator = MIN_FPS;
			timeperframe->numerator = 1;
		}

		/* Actual frame rate we use */
		tgt_fps = timeperframe->denominator /
			  timeperframe->numerator;

		if (tgt_fps == 15)
			frame_rate = gc2023_15_fps;
		else if (tgt_fps == 30)
			frame_rate = gc2023_30_fps;
		else {
			pr_err(" The camera frame rate is not supported!\n");
			return -EINVAL;
		}

		orig_mode = sensor->streamcap.capturemode;
		ret = gc2023_init_mode(frame_rate,
				(u32)a->parm.capture.capturemode, orig_mode);
		if (ret < 0)
			return ret;

		sensor->streamcap.timeperframe = *timeperframe;
		sensor->streamcap.capturemode =
				(u32)a->parm.capture.capturemode;

		break;

	/* These are all the possible cases. */
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
	case V4L2_BUF_TYPE_VIDEO_OVERLAY:
	case V4L2_BUF_TYPE_VBI_CAPTURE:
	case V4L2_BUF_TYPE_VBI_OUTPUT:
	case V4L2_BUF_TYPE_SLICED_VBI_CAPTURE:
	case V4L2_BUF_TYPE_SLICED_VBI_OUTPUT:
		pr_debug("   type is not " \
			"V4L2_BUF_TYPE_VIDEO_CAPTURE but %d\n",
			a->type);
		ret = -EINVAL;
		break;

	default:
		pr_debug("   type is unknown - %d\n", a->type);
		ret = -EINVAL;
		break;
	}

	return ret;
}

/*!
 * ioctl_g_fmt_cap - V4L2 sensor interface handler for ioctl_g_fmt_cap
 * @s: pointer to standard V4L2 device structure
 * @f: pointer to standard V4L2 v4l2_format structure
 *
 * Returns the sensor's current pixel format in the v4l2_format
 * parameter.
 */
static int ioctl_g_fmt_cap(struct v4l2_int_device *s, struct v4l2_format *f)
{
	struct sensor_data *sensor = s->priv;
	struct v4l2_pix_format pix;
	pix = sensor->pix;

	pix.height -= 8;
	pix.width -=8;
	
	f->fmt.pix = pix;

	return 0;
}

/*!
 * ioctl_g_ctrl - V4L2 sensor interface handler for VIDIOC_G_CTRL ioctl
 * @s: pointer to standard V4L2 device structure
 * @vc: standard V4L2 VIDIOC_G_CTRL ioctl structure
 *
 * If the requested control is supported, returns the control's current
 * value from the video_control[] array.  Otherwise, returns -EINVAL
 * if the control is not supported.
 */
static int ioctl_g_ctrl(struct v4l2_int_device *s, struct v4l2_control *vc)
{
	int ret = 0;
	switch (vc->id) {
	case V4L2_CID_BRIGHTNESS:
		vc->value = gc2023_data.brightness;
		break;
	case V4L2_CID_HUE:
		vc->value = gc2023_data.hue;
		break;
	case V4L2_CID_CONTRAST:
		vc->value = gc2023_data.contrast;
		break;
	case V4L2_CID_SATURATION:
		vc->value = gc2023_data.saturation;
		break;
	case V4L2_CID_RED_BALANCE:
		vc->value = gc2023_data.red;
		break;
	case V4L2_CID_BLUE_BALANCE:
		vc->value = gc2023_data.blue;
		break;
	case V4L2_CID_EXPOSURE:
		vc->value = gc2023_data.ae_mode;
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

/*!
 * ioctl_s_ctrl - V4L2 sensor interface handler for VIDIOC_S_CTRL ioctl
 * @s: pointer to standard V4L2 device structure
 * @vc: standard V4L2 VIDIOC_S_CTRL ioctl structure
 *
 * If the requested control is supported, sets the control's current
 * value in HW (and updates the video_control[] array).  Otherwise,
 * returns -EINVAL if the control is not supported.
 */
static int ioctl_s_ctrl(struct v4l2_int_device *s, struct v4l2_control *vc)
{
	int retval = 0;

	switch (vc->id) {
	case V4L2_CID_BRIGHTNESS:
		break;
	case V4L2_CID_CONTRAST:
		break;
	case V4L2_CID_SATURATION:
		break;
	case V4L2_CID_HUE:
		break;
	case V4L2_CID_AUTO_WHITE_BALANCE:
		break;
	case V4L2_CID_DO_WHITE_BALANCE:
		break;
	case V4L2_CID_RED_BALANCE:
		break;
	case V4L2_CID_BLUE_BALANCE:
		break;
	case V4L2_CID_GAMMA:
		break;
	case V4L2_CID_EXPOSURE:
		break;
	case V4L2_CID_AUTOGAIN:
		break;
	case V4L2_CID_GAIN:
		break;
	case V4L2_CID_HFLIP:
		break;
	case V4L2_CID_VFLIP:
		break;
	default:
		retval = -EPERM;
		break;
	}

	return retval;
}

/*!
 * ioctl_enum_framesizes - V4L2 sensor interface handler for
 *			   VIDIOC_ENUM_FRAMESIZES ioctl
 * @s: pointer to standard V4L2 device structure
 * @fsize: standard V4L2 VIDIOC_ENUM_FRAMESIZES ioctl structure
 *
 * Return 0 if successful, otherwise -EINVAL.
 */
static int ioctl_enum_framesizes(struct v4l2_int_device *s,
				 struct v4l2_frmsizeenum *fsize)
{
	if (fsize->index > gc2023_mode_MAX)
		return -EINVAL;

	fsize->pixel_format = gc2023_data.pix.pixelformat;
	fsize->discrete.width = 1920;
	fsize->discrete.height = 1080;

	return 0;
}

/*!
 * ioctl_enum_frameintervals - V4L2 sensor interface handler for
 *			       VIDIOC_ENUM_FRAMEINTERVALS ioctl
 * @s: pointer to standard V4L2 device structure
 * @fival: standard V4L2 VIDIOC_ENUM_FRAMEINTERVALS ioctl structure
 *
 * Return 0 if successful, otherwise -EINVAL.
 */
static int ioctl_enum_frameintervals(struct v4l2_int_device *s,
					 struct v4l2_frmivalenum *fival)
{
	fival->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	fival->discrete.numerator = 1;

	fival->discrete.denominator = gc2023_framerates[1];
	return 0;
}

/*!
 * ioctl_g_chip_ident - V4L2 sensor interface handler for
 *			VIDIOC_DBG_G_CHIP_IDENT ioctl
 * @s: pointer to standard V4L2 device structure
 * @id: pointer to int
 *
 * Return 0.
 */
static int ioctl_g_chip_ident(struct v4l2_int_device *s, int *id)
{
	((struct v4l2_dbg_chip_ident *)id)->match.type =
					V4L2_CHIP_MATCH_I2C_DRIVER;
	strcpy(((struct v4l2_dbg_chip_ident *)id)->match.name,
		"gc2023_mipi_camera");
	
	return 0;
}

/*!
 * ioctl_init - V4L2 sensor interface handler for VIDIOC_INT_INIT
 * @s: pointer to standard V4L2 device structure
 */
static int ioctl_init(struct v4l2_int_device *s)
{
	return 0;
}

/*!
 * ioctl_enum_fmt_cap - V4L2 sensor interface handler for VIDIOC_ENUM_FMT
 * @s: pointer to standard V4L2 device structure
 * @fmt: pointer to standard V4L2 fmt description structure
 *
 * Return 0.
 */
static int ioctl_enum_fmt_cap(struct v4l2_int_device *s,
			      struct v4l2_fmtdesc *fmt)
{
	if (fmt->index > gc2023_mode_MAX)
		return -EINVAL;

	fmt->pixelformat = gc2023_data.pix.pixelformat;

	return 0;
}

/*!
 * ioctl_dev_init - V4L2 sensor interface handler for vidioc_int_dev_init_num
 * @s: pointer to standard V4L2 device structure
 *
 * Initialise the device when slave attaches to the master.
 */
static int ioctl_dev_init(struct v4l2_int_device *s)
{
	struct sensor_data *sensor = s->priv;
	u32 tgt_xclk;	/* target xclk */
	u32 tgt_fps;	/* target frames per secound */
	int ret;
	enum gc2023_frame_rate frame_rate;
	void *mipi_csi2_info;

	gc2023_data.on = true;

	/* mclk */
	tgt_xclk = gc2023_data.mclk;
	tgt_xclk = min(tgt_xclk, (u32)GC2023_XCLK_MAX);   // ok
	tgt_xclk = max(tgt_xclk, (u32)GC2023_XCLK_MIN);	 // ok
	gc2023_data.mclk = tgt_xclk;

	/* Default camera frame rate is set in probe */
	tgt_fps = sensor->streamcap.timeperframe.denominator /
		  sensor->streamcap.timeperframe.numerator;

	if (tgt_fps == 15)
		frame_rate = gc2023_15_fps;
	else if (tgt_fps == 30)
		frame_rate = gc2023_30_fps;
	else
		return -EINVAL; /* Only support 15fps or 30fps now. */

	mipi_csi2_info = mipi_csi2_get_info();

	/* enable mipi csi2 */
	if (mipi_csi2_info)
		mipi_csi2_enable(mipi_csi2_info);
	else {
		printk(KERN_ERR "%s() in %s: Fail to get mipi_csi2_info!\n",
		       __func__, __FILE__);
		return -EPERM;
	}

	ret = gc2023_init_mode(frame_rate, gc2023_mode_INIT, gc2023_mode_INIT);

	return ret;
}

/*!
 * ioctl_dev_exit - V4L2 sensor interface handler for vidioc_int_dev_exit_num
 * @s: pointer to standard V4L2 device structure
 *
 * Delinitialise the device when slave detaches to the master.
 */
static int ioctl_dev_exit(struct v4l2_int_device *s)
{
	void *mipi_csi2_info;

	mipi_csi2_info = mipi_csi2_get_info();

	/* disable mipi csi2 */
	if (mipi_csi2_info)
		if (mipi_csi2_get_status(mipi_csi2_info))
			mipi_csi2_disable(mipi_csi2_info);

	return 0;
}

/*!
 * This structure defines all the ioctls for this module and links them to the
 * enumeration.
 */
static struct v4l2_int_ioctl_desc gc2023_ioctl_desc[] = {
	{vidioc_int_dev_init_num, (v4l2_int_ioctl_func *) ioctl_dev_init},
	{vidioc_int_dev_exit_num, ioctl_dev_exit},
	{vidioc_int_s_power_num, (v4l2_int_ioctl_func *) ioctl_s_power},
	{vidioc_int_g_ifparm_num, (v4l2_int_ioctl_func *) ioctl_g_ifparm},
/*	{vidioc_int_g_needs_reset_num,
				(v4l2_int_ioctl_func *)ioctl_g_needs_reset}, */
/*	{vidioc_int_reset_num, (v4l2_int_ioctl_func *)ioctl_reset}, */
	{vidioc_int_init_num, (v4l2_int_ioctl_func *) ioctl_init},
	{vidioc_int_enum_fmt_cap_num,
				(v4l2_int_ioctl_func *) ioctl_enum_fmt_cap},
/*	{vidioc_int_try_fmt_cap_num,
				(v4l2_int_ioctl_func *)ioctl_try_fmt_cap}, */
	{vidioc_int_g_fmt_cap_num, (v4l2_int_ioctl_func *) ioctl_g_fmt_cap},
/*	{vidioc_int_s_fmt_cap_num, (v4l2_int_ioctl_func *) ioctl_s_fmt_cap}, */
	{vidioc_int_g_parm_num, (v4l2_int_ioctl_func *) ioctl_g_parm},
	{vidioc_int_s_parm_num, (v4l2_int_ioctl_func *) ioctl_s_parm},
/*	{vidioc_int_queryctrl_num, (v4l2_int_ioctl_func *)ioctl_queryctrl}, */
	{vidioc_int_g_ctrl_num, (v4l2_int_ioctl_func *) ioctl_g_ctrl},
	{vidioc_int_s_ctrl_num, (v4l2_int_ioctl_func *) ioctl_s_ctrl},
	{vidioc_int_enum_framesizes_num,
				(v4l2_int_ioctl_func *) ioctl_enum_framesizes},
	{vidioc_int_enum_frameintervals_num,
			(v4l2_int_ioctl_func *) ioctl_enum_frameintervals},
	{vidioc_int_g_chip_ident_num,
				(v4l2_int_ioctl_func *) ioctl_g_chip_ident},
};

static struct v4l2_int_slave gc2023_slave = {
	.ioctls = gc2023_ioctl_desc,
	.num_ioctls = ARRAY_SIZE(gc2023_ioctl_desc),
};

static struct v4l2_int_device gc2023_int_device = {
	.module = THIS_MODULE,
	.name = "gc2023",
	.type = v4l2_int_type_slave,
	.u = {
		.slave = &gc2023_slave,
	},
};

/*!
 * gc2023 I2C probe function
 *
 * @param adapter            struct i2c_adapter *
 * @return  Error code indicating success or failure
 */
static int gc2023_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	int retval;
	u8 chip_id_high, chip_id_low;

	/* request power down pin */
	pwn_gpio = of_get_named_gpio(dev->of_node, "pwn-gpios", 0);
	if (!gpio_is_valid(pwn_gpio)) {
		dev_warn(dev, "no sensor pwdn pin available");
		return -EINVAL;
	}
	
	retval = devm_gpio_request_one(dev, pwn_gpio, GPIOF_OUT_INIT_HIGH,
					"gc2023_mipi_pwdn");
	if (retval < 0)
		return retval;

	/* request reset pin */
	rst_gpio = of_get_named_gpio(dev->of_node, "rst-gpios", 0);
	
	if (!gpio_is_valid(rst_gpio)) {
		dev_warn(dev, "no sensor reset pin available");
		return -EINVAL;
	}
	
	retval = devm_gpio_request_one(dev, rst_gpio, GPIOF_OUT_INIT_HIGH,
					"gc2023_mipi_reset");
	if (retval < 0)
		return retval;

	gpio_set_value(pwn_gpio, 0);
	msleep(5);
	gpio_set_value(rst_gpio, 1); 

	/* Set initial values for the sensor struct. */
	memset(&gc2023_data, 0, sizeof(gc2023_data));
	
	gc2023_data.sensor_clk = devm_clk_get(dev, "csi_mclk");
	
	if (IS_ERR(gc2023_data.sensor_clk)) {
		/* assuming clock enabled by default */
		gc2023_data.sensor_clk = NULL;
		dev_err(dev, "clock-frequency missing or invalid\n");
		return PTR_ERR(gc2023_data.sensor_clk);
	}

	retval = of_property_read_u32(dev->of_node, "mclk",
					&(gc2023_data.mclk));
	if (retval) {
		dev_err(dev, "mclk missing or invalid\n");
		return retval;
	}
	
	retval = of_property_read_u32(dev->of_node, "mclk_source",
					(u32 *) &(gc2023_data.mclk_source));
	if (retval) {
		dev_err(dev, "mclk_source missing or invalid\n");
		return retval;
	}

	retval = of_property_read_u32(dev->of_node, "csi_id",
					&(gc2023_data.csi));
	if (retval) {
		dev_err(dev, "csi id missing or invalid\n");
		return retval;
	}
	
	gc2023_reset();

	clk_prepare_enable(gc2023_data.sensor_clk);

	gc2023_data.io_init               = gc2023_reset;
	gc2023_data.i2c_client          = client;
//	gc2023_data.pix.pixelformat  = V4L2_PIX_FMT_UYVY;
	gc2023_data.pix.pixelformat  = V4L2_PIX_FMT_SRGGB8; // V4L2_PIX_FMT_SRGGB8
	gc2023_data.pix.width           = 1928;
	gc2023_data.pix.height          = 1088;
	gc2023_data.streamcap.capability = V4L2_MODE_HIGHQUALITY |V4L2_CAP_TIMEPERFRAME;
	gc2023_data.streamcap.capturemode = 0;
	gc2023_data.streamcap.timeperframe.denominator = DEFAULT_FPS;
	gc2023_data.streamcap.timeperframe.numerator = 1;

	chip_id_high = gc2023_read(0xf0);

	if ( chip_id_high != 0x20) {
		pr_warning("camera gc2023_mipi is not found\n");
		clk_disable_unprepare(gc2023_data.sensor_clk);
		return -ENODEV;
	}
	chip_id_low = gc2023_read(0xf1);

	if (chip_id_low != 0x23) {
		pr_warning("camera gc2023_mipi is not found\n");
		clk_disable_unprepare(gc2023_data.sensor_clk);
		return -ENODEV;
	}

	printk("%s CHIP ID:%.4X \n", __FUNCTION__,  chip_id_high <<16 |chip_id_low);
	gc2023_hard_init();
	
	gc2023_int_device.priv = &gc2023_data;
	retval = v4l2_int_device_register(&gc2023_int_device);

	clk_disable_unprepare(gc2023_data.sensor_clk);

	pr_info("camera gc2023_mipi is found  retval = %d \n", retval);
	return retval;
}

/*!
 * gc2023 I2C detach function
 *
 * @param client            struct i2c_client *
 * @return  Error code indicating success or failure
 */
static int gc2023_remove(struct i2c_client *client)
{
	v4l2_int_device_unregister(&gc2023_int_device);

	return 0;
}

/*!
 * gc2023 init function
 * Called by insmod gc2023_camera.ko.
 *
 * @return  Error code indicating success or failure
 */
static __init int gc2023_init(void)
{
	u8 err;

	err = i2c_add_driver(&gc2023_i2c_driver);
	if (err != 0)
		pr_err("%s:driver registration failed, error=%d\n",
			__func__, err);

	return err;
}

/*!
 * GC2023 cleanup function
 * Called on rmmod gc2023_camera.ko
 *
 * @return  Error code indicating success or failure
 */
static void __exit gc2023_clean(void)
{
	i2c_del_driver(&gc2023_i2c_driver);
}

module_init(gc2023_init);
module_exit(gc2023_clean);

MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("GC2023 MIPI Camera Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
MODULE_ALIAS("CSI");
