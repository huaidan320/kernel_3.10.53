//#define DEBUG 1

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/regulator/consumer.h>
#include <media/v4l2-chip-ident.h>
#include <media/v4l2-int-device.h>
#include "../../media/platform/mxc/capture/mxc_v4l2_capture.h"

#include "tw9990.h" 

#define BT656_EMBED_EAV_SAV

#define GET_ID(val)  ((val & 0xF8) >> 3)                                                                                                 
#define GET_REV(val) (val & 0x07)
	
#define V4L2_IDENT_TW9990  9990

#define TW9990_VOLTAGE_ANALOG               1800000
#define TW9990_VOLTAGE_DIGITAL_CORE         1800000
#define TW9990_VOLTAGE_DIGITAL_IO           3300000
#define TW9990_VOLTAGE_PLL                  1800000

static struct regulator *dvddio_regulator;
static struct regulator *dvdd_regulator;
static struct regulator *avdd_regulator;
static struct regulator *pvdd_regulator;

/*!
 * Maintains the information on the current state of the sensor.
 */
struct sensor {
	struct sensor_data sen;
	v4l2_std_id std_id;
} tw9990_data;

/*! List of input video formats supported. The video formats is corresponding
 * with v4l2 id in video_fmt_t
 */
typedef enum {
	TW9990_NTSC = 0,	/*!< Locked on (M) NTSC video signal. */
	TW9990_PAL,		    /*!< (B, G, H, I, N)PAL video signal. */
	TW9990_NOT_LOCKED,	/*!< Not locked on a signal. */
} video_fmt_idx;

/*! Number of video standards supported (including 'not locked' signal). */
#define TW9990_STD_MAX		(TW9990_PAL + 1)

/*! Video format structure. */
typedef struct {
	int v4l2_id;		/*!< Video for linux ID. */
	char name[16];		/*!< Name (e.g., "NTSC", "PAL", etc.) */
	u16 raw_width;		/*!< Raw width. */
	u16 raw_height;		/*!< Raw height. */
	u16 active_width;	/*!< Active width. */
	u16 active_height;	/*!< Active height. */
	int frame_rate;		/*!< Frame rate. */
} video_fmt_t;

static video_fmt_t video_fmts[] = {
	{/*! NTSC */
	 .v4l2_id = V4L2_STD_NTSC,
	 .name = "NTSC",
	 .raw_width = 720,	    /* SENS_FRM_WIDTH */
	 .raw_height = 525,	    /* SENS_FRM_HEIGHT */
	 .active_width = 720,	/* ACT_FRM_WIDTH plus 1 */
	 .active_height = 480,	/* ACT_FRM_WIDTH plus 1 */
	 .frame_rate = 30,
	 },
	{/*! (B, G, H, I, N) PAL */
	 .v4l2_id = V4L2_STD_PAL,
	 .name = "PAL",
	 .raw_width = 720,
	 .raw_height = 625,
	 .active_width = 720,
	 .active_height = 576,
	 .frame_rate = 25,
	 },
	{/*! Unlocked standard */
	 .v4l2_id = V4L2_STD_ALL,
	 .name = "Autodetect",
	 .raw_width = 720,
	 .raw_height = 625,
	 .active_width = 720,
	 .active_height = 576,
	 .frame_rate = 0,
	 },
};

static video_fmt_idx video_idx = TW9990_PAL;

static DEFINE_MUTEX(mutex);

//#define IF_NAME                       "tw9990"
#define IF_NAME                       "tw9990"

#define TW9990_IDENT                  0x00	/* IDENT */
#define TW9990_STATUS_1               0x01	/* Status #1 */
#define TW9990_INPUT_CTL              0x02	/* Input Control */
#define TW9990_BRIGHTNESS             0x10	/* Brightness */
#define TW9990_CONTRAST               0x11	/* Contrast */
#define TW9990_SHARPNESS              0x12	/* Sharpness */
#define TW9990_SD_SATURATION_CB       0x13	/* SD Saturation Cb */
#define TW9990_SD_SATURATION_CR       0x14	/* SD Saturation Cr */

static struct v4l2_queryctrl tw9990_qctrl[] = {
	{
	.id = V4L2_CID_BRIGHTNESS,
	.type = V4L2_CTRL_TYPE_INTEGER,
	.name = "Brightness",
	.minimum = 0,		/* check this value */
	.maximum = 255,		/* check this value */
	.step = 1,		/* check this value */
	.default_value = 127,	/* check this value */
	.flags = 0,
	}, {
	.id = V4L2_CID_SATURATION,
	.type = V4L2_CTRL_TYPE_INTEGER,
	.name = "Saturation",
	.minimum = 0,		/* check this value */
	.maximum = 255,		/* check this value */
	.step = 0x1,		/* check this value */
	.default_value = 127,	/* check this value */
	.flags = 0,
	}
};

static int tw9990_regulator_enable(struct device *dev)
{
	int ret = 0;

	dvddio_regulator = devm_regulator_get(dev, "DOVDD");

	if (!IS_ERR(dvddio_regulator)) {
		regulator_set_voltage(dvddio_regulator,
				      TW9990_VOLTAGE_DIGITAL_IO,
				      TW9990_VOLTAGE_DIGITAL_IO);
		ret = regulator_enable(dvddio_regulator);
		if (ret) {
			dev_err(dev, "set io voltage failed\n");
			return ret;
		} else {
			dev_dbg(dev, "set io voltage ok\n");
		}
	} else {
		dev_warn(dev, "cannot get io voltage\n");
	}

	dvdd_regulator = devm_regulator_get(dev, "DVDD");
	if (!IS_ERR(dvdd_regulator)) {
		regulator_set_voltage(dvdd_regulator,
				      TW9990_VOLTAGE_DIGITAL_CORE,
				      TW9990_VOLTAGE_DIGITAL_CORE);
		ret = regulator_enable(dvdd_regulator);
		if (ret) {
			dev_err(dev, "set core voltage failed\n");
			return ret;
		} else {
			dev_dbg(dev, "set core voltage ok\n");
		}
	} else {
		dev_warn(dev, "cannot get core voltage\n");
	}

	avdd_regulator = devm_regulator_get(dev, "AVDD");
	if (!IS_ERR(avdd_regulator)) {
		regulator_set_voltage(avdd_regulator,
				      TW9990_VOLTAGE_ANALOG,
				      TW9990_VOLTAGE_ANALOG);
		ret = regulator_enable(avdd_regulator);
		if (ret) {
			dev_err(dev, "set analog voltage failed\n");
			return ret;
		} else {
			dev_dbg(dev, "set analog voltage ok\n");
		}
	} else {
		dev_warn(dev, "cannot get analog voltage\n");
	}

	pvdd_regulator = devm_regulator_get(dev, "PVDD");
	if (!IS_ERR(pvdd_regulator)) {
		regulator_set_voltage(pvdd_regulator,
				      TW9990_VOLTAGE_PLL,
				      TW9990_VOLTAGE_PLL);
		ret = regulator_enable(pvdd_regulator);
		if (ret) {
			dev_err(dev, "set pll voltage failed\n");
			return ret;
		} else {
			dev_dbg(dev, "set pll voltage ok\n");
		}
	} else {
		dev_warn(dev, "cannot get pll voltage\n");
	}

	return ret;
}

static inline int tw9990_read(u8 reg)
{
	int val;

	val = i2c_smbus_read_byte_data(tw9990_data.sen.i2c_client, reg);
	if (val < 0) {
		dev_dbg(&tw9990_data.sen.i2c_client->dev,
			"%s:read reg error: reg=%2x\n", __func__, reg);
		return -1;
	}
	return val;
}

static int tw9990_write_reg(u8 reg, u8 val)
{
	s32 ret;

	ret = i2c_smbus_write_byte_data(tw9990_data.sen.i2c_client, reg, val);
	if (ret < 0) {
		dev_dbg(&tw9990_data.sen.i2c_client->dev, 
			"%s:write reg error:reg=%2x,val=%2x\n", __func__, reg, val);
		return -1;
	}
	return 0;
}

static void tw9990_get_std(v4l2_std_id *std)
{
	int status_1, standard, idx;
	bool locked;

	dev_dbg(&tw9990_data.sen.i2c_client->dev, "In tw9990_get_std\n");
	status_1 = tw9990_read(TW9990_STATUS_1);

	if(status_1 && 0x48) 
		locked = 1;
	else
		locked = 0;

	if(status_1 && 0x01) 
		standard = 0x40;    // ntsc
	else 
		standard = 0;  //pal
		
	mutex_lock(&mutex);
//	*std = V4L2_STD_ALL;
//	idx = TW9990_NOT_LOCKED;
	
	//standard = 0;
//	standard = 0x40;
//	locked = 1;
	
	if (locked) {
		if (standard == 0x40) {
			*std = V4L2_STD_PAL;
			idx = TW9990_PAL;
		} else if (standard == 0) {
			*std = V4L2_STD_NTSC;
			idx = TW9990_NTSC;
		}
	}
	mutex_unlock(&mutex);

	/* This assumes autodetect which this device uses. */
	if (*std != tw9990_data.std_id) {
		video_idx = idx;
		tw9990_data.std_id = *std;
		tw9990_data.sen.pix.width = video_fmts[video_idx].raw_width;   // ?
		tw9990_data.sen.pix.height = video_fmts[video_idx].raw_height;  // ? 
	}


	
}

static int ioctl_g_ifparm(struct v4l2_int_device *s, struct v4l2_ifparm *p)
{
	dev_dbg(&tw9990_data.sen.i2c_client->dev, "tw9990:ioctl_g_ifparm\n");




	if (s == NULL) {
		pr_err("   ERROR!! no slave device set!\n");
		return -1;
	}


	/* Initialize structure to 0s then set any non-0 values. */
	memset(p, 0, sizeof(*p));
	p->if_type = V4L2_IF_TYPE_BT656; /* This is the only possibility. */
	p->u.bt656.mode = V4L2_IF_TYPE_BT656_MODE_NOBT_8BIT;
	p->u.bt656.nobt_hs_inv = 0;
	
	p->u.bt656.latch_clk_inv = 1;   // ok
	
#ifdef BT656_EMBED_EAV_SAV
	p->u.bt656.bt_sync_correct = 0;  //  1: external vsync and hsync will be used for sync, 0: embedded EAV and SAV will be used for sync
#else
    p->u.bt656.bt_sync_correct = 1;
#endif	
	p->u.bt656.clock_curr = 0; // IPUC_CSI_CLK_MODE_CCIR656_INTERLACED
	/**********************/
//	p->u.bt656.frame_start_on_rising_vs = 1; /*not handle by imx*/
//	p->u.bt656.bt_sync_correct = 1; /* indicate external vsync */
//	p->u.bt656.latch_clk_inv = 0; /* 1: clock on falling edge */
//	p->u.bt656.nobt_hs_inv = 0;
//	p->u.bt656.nobt_vs_inv = 0;
//	p->u.bt656.mode         = V4L2_IF_TYPE_BT656_MODE_NOBT_8BIT;
//	p->u.bt656.clock_curr	= ov7740_data.malk;  // external clock
	/****************/

	/* TW9990 has a dedicated clock so no clock settings needed. */

	return 0;
}

static int ioctl_s_power(struct v4l2_int_device *s, int on)
{
	struct sensor *sensor = s->priv;

	dev_dbg(&tw9990_data.sen.i2c_client->dev, "tw9990:ioctl_s_power\n");
	sensor->sen.on = on;

	return 0;
}

static int ioctl_g_parm(struct v4l2_int_device *s, struct v4l2_streamparm *a)
{
	struct sensor *sensor = s->priv;
	struct v4l2_captureparm *cparm = &a->parm.capture;

	dev_dbg(&tw9990_data.sen.i2c_client->dev, "In tw9990:ioctl_g_parm\n");

	switch (a->type) {
	/* These are all the possible cases. */
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		pr_debug("   type is V4L2_BUF_TYPE_VIDEO_CAPTURE\n");
		memset(a, 0, sizeof(*a));
		a->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		cparm->capability = sensor->sen.streamcap.capability;
		cparm->timeperframe = sensor->sen.streamcap.timeperframe;
		cparm->capturemode = sensor->sen.streamcap.capturemode;
		break;

	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
	case V4L2_BUF_TYPE_VIDEO_OVERLAY:
	case V4L2_BUF_TYPE_VBI_CAPTURE:
	case V4L2_BUF_TYPE_VBI_OUTPUT:
	case V4L2_BUF_TYPE_SLICED_VBI_CAPTURE:
	case V4L2_BUF_TYPE_SLICED_VBI_OUTPUT:
		break;

	default:
		pr_debug("ioctl_g_parm:type is unknown %d\n", a->type);
		break;
	}

	return 0;
}

static int ioctl_s_parm(struct v4l2_int_device *s, struct v4l2_streamparm *a)
{
	dev_dbg(&tw9990_data.sen.i2c_client->dev, "In tw9990:ioctl_s_parm\n");

	switch (a->type) {
	/* These are all the possible cases. */
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
	case V4L2_BUF_TYPE_VIDEO_OVERLAY:
	case V4L2_BUF_TYPE_VBI_CAPTURE:
	case V4L2_BUF_TYPE_VBI_OUTPUT:
	case V4L2_BUF_TYPE_SLICED_VBI_CAPTURE:
	case V4L2_BUF_TYPE_SLICED_VBI_OUTPUT:
		break;

	default:
		pr_debug("   type is unknown - %d\n", a->type);
		break;
	}

	return 0;
}

static int ioctl_g_fmt_cap(struct v4l2_int_device *s, struct v4l2_format *f)
{
	struct sensor *sensor = s->priv;

	dev_dbg(&tw9990_data.sen.i2c_client->dev, "tw9990:ioctl_g_fmt_cap\n");

	switch (f->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		pr_debug("   Returning size of %dx%d\n",
			 sensor->sen.pix.width, sensor->sen.pix.height);
		f->fmt.pix = sensor->sen.pix;

		break;

	case V4L2_BUF_TYPE_PRIVATE: {
		v4l2_std_id std;
		tw9990_get_std(&std);
		f->fmt.pix.pixelformat = (u32)std;
		}
		break;

	default:
		f->fmt.pix = sensor->sen.pix;
		break;
	}

	return 0;
}

static int ioctl_queryctrl(struct v4l2_int_device *s,
			   struct v4l2_queryctrl *qc)
{
	int i;

	dev_dbg(&tw9990_data.sen.i2c_client->dev, "tw9990:ioctl_queryctrl\n");

	for (i = 0; i < ARRAY_SIZE(tw9990_qctrl); i++)
		if (qc->id && qc->id == tw9990_qctrl[i].id) {
			memcpy(qc, &(tw9990_qctrl[i]),
				sizeof(*qc));
			return 0;
		}

	return -EINVAL;
}

static int ioctl_g_ctrl(struct v4l2_int_device *s, struct v4l2_control *vc)
{
	int ret = 0;
	int sat = 0;

	dev_dbg(&tw9990_data.sen.i2c_client->dev, "In tw9990:ioctl_g_ctrl\n");

	switch (vc->id) {
	case V4L2_CID_BRIGHTNESS:
		dev_dbg(&tw9990_data.sen.i2c_client->dev, "   V4L2_CID_BRIGHTNESS\n");
		tw9990_data.sen.brightness = tw9990_read(TW9990_BRIGHTNESS);
		vc->value = tw9990_data.sen.brightness;
		break;
	case V4L2_CID_CONTRAST:
		dev_dbg(&tw9990_data.sen.i2c_client->dev, "   V4L2_CID_CONTRAST\n");
		vc->value = tw9990_data.sen.contrast;
		break;
	case V4L2_CID_SATURATION:
		dev_dbg(&tw9990_data.sen.i2c_client->dev, "   V4L2_CID_SATURATION\n");
		sat = tw9990_read(TW9990_SD_SATURATION_CB);
		tw9990_data.sen.saturation = sat;
		vc->value = tw9990_data.sen.saturation;
		break;
	case V4L2_CID_HUE:
		dev_dbg(&tw9990_data.sen.i2c_client->dev, "   V4L2_CID_HUE\n");
		vc->value = tw9990_data.sen.hue;
		break;
	case V4L2_CID_AUTO_WHITE_BALANCE:
		dev_dbg(&tw9990_data.sen.i2c_client->dev, "   V4L2_CID_AUTO_WHITE_BALANCE\n");
		break;
	case V4L2_CID_DO_WHITE_BALANCE:
		dev_dbg(&tw9990_data.sen.i2c_client->dev, "   V4L2_CID_DO_WHITE_BALANCE\n");
		break;
	case V4L2_CID_RED_BALANCE:
		dev_dbg(&tw9990_data.sen.i2c_client->dev, "   V4L2_CID_RED_BALANCE\n");
		vc->value = tw9990_data.sen.red;
		break;
	case V4L2_CID_BLUE_BALANCE:
		dev_dbg(&tw9990_data.sen.i2c_client->dev, "   V4L2_CID_BLUE_BALANCE\n");
		vc->value = tw9990_data.sen.blue;
		break;
	case V4L2_CID_GAMMA:
		dev_dbg(&tw9990_data.sen.i2c_client->dev, "   V4L2_CID_GAMMA\n");
		break;
	case V4L2_CID_EXPOSURE:
		dev_dbg(&tw9990_data.sen.i2c_client->dev, "   V4L2_CID_EXPOSURE\n");
		vc->value = tw9990_data.sen.ae_mode;
		break;
	case V4L2_CID_AUTOGAIN:
		dev_dbg(&tw9990_data.sen.i2c_client->dev, "   V4L2_CID_AUTOGAIN\n");
		break;
	case V4L2_CID_GAIN:
		dev_dbg(&tw9990_data.sen.i2c_client->dev, "   V4L2_CID_GAIN\n");
		break;
	case V4L2_CID_HFLIP:
		dev_dbg(&tw9990_data.sen.i2c_client->dev, "   V4L2_CID_HFLIP\n");
		break;
	case V4L2_CID_VFLIP:
		dev_dbg(&tw9990_data.sen.i2c_client->dev, "   V4L2_CID_VFLIP\n");
		break;
	default:
		dev_dbg(&tw9990_data.sen.i2c_client->dev, "   Default case\n");
		vc->value = 0;
		ret = -EPERM;
		break;
	}

	return ret;
}

static int ioctl_s_ctrl(struct v4l2_int_device *s, struct v4l2_control *vc)
{
	int retval = 0;
	u8 tmp;

	dev_dbg(&tw9990_data.sen.i2c_client->dev, "In tw9990:ioctl_s_ctrl\n");

	switch (vc->id) {
	case V4L2_CID_BRIGHTNESS:
		dev_dbg(&tw9990_data.sen.i2c_client->dev, "   V4L2_CID_BRIGHTNESS\n");
		tmp = vc->value;
		tw9990_write_reg(TW9990_BRIGHTNESS, tmp);
		tw9990_data.sen.brightness = vc->value;
		break;
	case V4L2_CID_CONTRAST:
		dev_dbg(&tw9990_data.sen.i2c_client->dev, "   V4L2_CID_CONTRAST\n");
		break;
	case V4L2_CID_SATURATION:
		dev_dbg(&tw9990_data.sen.i2c_client->dev, "   V4L2_CID_SATURATION\n");
		tmp = vc->value;
		tw9990_write_reg(TW9990_SD_SATURATION_CB, tmp);
		tw9990_write_reg(TW9990_SD_SATURATION_CR, tmp);
		tw9990_data.sen.saturation = vc->value;
		break;
	case V4L2_CID_HUE:
		dev_dbg(&tw9990_data.sen.i2c_client->dev, "   V4L2_CID_HUE\n");
		break;
	case V4L2_CID_AUTO_WHITE_BALANCE:
		dev_dbg(&tw9990_data.sen.i2c_client->dev, "   V4L2_CID_AUTO_WHITE_BALANCE\n");
		break;
	case V4L2_CID_DO_WHITE_BALANCE:
		dev_dbg(&tw9990_data.sen.i2c_client->dev, "   V4L2_CID_DO_WHITE_BALANCE\n");
		break;
	case V4L2_CID_RED_BALANCE:
		dev_dbg(&tw9990_data.sen.i2c_client->dev, "   V4L2_CID_RED_BALANCE\n");
		break;
	case V4L2_CID_BLUE_BALANCE:
		dev_dbg(&tw9990_data.sen.i2c_client->dev, "   V4L2_CID_BLUE_BALANCE\n");
		break;
	case V4L2_CID_GAMMA:
		dev_dbg(&tw9990_data.sen.i2c_client->dev, "   V4L2_CID_GAMMA\n");
		break;
	case V4L2_CID_EXPOSURE:
		dev_dbg(&tw9990_data.sen.i2c_client->dev, "   V4L2_CID_EXPOSURE\n");
		break;
	case V4L2_CID_AUTOGAIN:
		dev_dbg(&tw9990_data.sen.i2c_client->dev, "   V4L2_CID_AUTOGAIN\n");
		break;
	case V4L2_CID_GAIN:
		dev_dbg(&tw9990_data.sen.i2c_client->dev, "   V4L2_CID_GAIN\n");
		break;
	case V4L2_CID_HFLIP:
		dev_dbg(&tw9990_data.sen.i2c_client->dev, "   V4L2_CID_HFLIP\n");
		break;
	case V4L2_CID_VFLIP:
		dev_dbg(&tw9990_data.sen.i2c_client->dev, "   V4L2_CID_VFLIP\n");
		break;
	default:
		dev_dbg(&tw9990_data.sen.i2c_client->dev, "   Default case\n");
		retval = -EPERM;
		break;
	}

	return retval;
}

static int ioctl_enum_framesizes(struct v4l2_int_device *s,
				 struct v4l2_frmsizeenum *fsize)
{
	if (fsize->index >= 1)
		return -EINVAL;

	fsize->discrete.width = video_fmts[video_idx].active_width;
	fsize->discrete.height  = video_fmts[video_idx].active_height;

	return 0;
}

static int ioctl_enum_frameintervals(struct v4l2_int_device *s, struct v4l2_frmivalenum *fival)
{
	video_fmt_t fmt;
	int i;

	if (fival->index != 0)
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(video_fmts) - 1; i++) {
		fmt = video_fmts[i];
		if (fival->width  == fmt.active_width &&
		    fival->height == fmt.active_height) {
			fival->type = V4L2_FRMIVAL_TYPE_DISCRETE;
			fival->discrete.numerator = 1;
			fival->discrete.denominator = fmt.frame_rate;
			return 0;
		}
	}

	return -EINVAL;
}

static int ioctl_g_chip_ident(struct v4l2_int_device *s, int *id)
{
	((struct v4l2_dbg_chip_ident *)id)->match.type = V4L2_CHIP_MATCH_I2C_DRIVER;
	
	strcpy(((struct v4l2_dbg_chip_ident *)id)->match.name, "tw9990_decoder");
	
	((struct v4l2_dbg_chip_ident *)id)->ident = V4L2_IDENT_TW9990;   // ?
	return 0;
}

static int ioctl_init(struct v4l2_int_device *s)
{
	dev_dbg(&tw9990_data.sen.i2c_client->dev, "In tw9990:ioctl_init\n");
	return 0;
}

static int ioctl_dev_init(struct v4l2_int_device *s)
{
	dev_dbg(&tw9990_data.sen.i2c_client->dev, "tw9990:ioctl_dev_init\n");
	return 0;
}

static struct v4l2_int_ioctl_desc tw9990_ioctl_desc[] = {
	{vidioc_int_dev_init_num, (v4l2_int_ioctl_func*)ioctl_dev_init},
	{vidioc_int_s_power_num, (v4l2_int_ioctl_func*)ioctl_s_power},
	{vidioc_int_g_ifparm_num, (v4l2_int_ioctl_func*)ioctl_g_ifparm},
	{vidioc_int_init_num, (v4l2_int_ioctl_func*)ioctl_init},
	{vidioc_int_g_fmt_cap_num, (v4l2_int_ioctl_func*)ioctl_g_fmt_cap},
	{vidioc_int_g_parm_num, (v4l2_int_ioctl_func*)ioctl_g_parm},
	{vidioc_int_s_parm_num, (v4l2_int_ioctl_func*)ioctl_s_parm},
	{vidioc_int_queryctrl_num, (v4l2_int_ioctl_func*)ioctl_queryctrl},
	{vidioc_int_g_ctrl_num, (v4l2_int_ioctl_func*)ioctl_g_ctrl},
	{vidioc_int_s_ctrl_num, (v4l2_int_ioctl_func*)ioctl_s_ctrl},
	{vidioc_int_enum_framesizes_num, (v4l2_int_ioctl_func *)ioctl_enum_framesizes},
	{vidioc_int_enum_frameintervals_num, (v4l2_int_ioctl_func *)ioctl_enum_frameintervals},
	{vidioc_int_g_chip_ident_num, (v4l2_int_ioctl_func *)ioctl_g_chip_ident},
};

static struct v4l2_int_slave tw9990_slave = {
	.ioctls = tw9990_ioctl_desc,
	.num_ioctls = ARRAY_SIZE(tw9990_ioctl_desc),
};

static struct v4l2_int_device tw9990_int_device = {
	.module = THIS_MODULE,
//	.name = "tw9990",
       .name = "tw9990",
	.type = v4l2_int_type_slave,
	.u = {
		.slave = &tw9990_slave,
	},
};

static unsigned char tw9990_register_array[][2] = {
	{0xFF, 0x00},
	{0x00, 0x00},
	{0x01, 0x68},
	{0x02, 0xC0},
	
//	{0x03, 0xA1},

#ifdef BT656_EMBED_EAV_SAV
	{0x03, 0xA0},
#else
	{0x03, 0x20},
#endif
	{0x04, 0x00},
	{0x05, 0x00},
	{0x06, 0x00},
	{0x07, 0x12},
	{0x08, 0x12},
	{0x09, 0x20},
	{0x0A, 0x10},
	{0x0B, 0xD0},
	{0x0C, 0xCC},
	{0x0D, 0x74},
	{0x0E, 0x11},
	{0x0F, 0x31},
	{0x10, 0x00},
	{0x11, 0x64},
	{0x12, 0x21},
	{0x13, 0x80},
	{0x14, 0x80},
	{0x15, 0x00},
	{0x16, 0x00},
	{0x17, 0x30},
	{0x18, 0x44},
	{0x19, 0x58},
	{0x1A, 0x0A},
	{0x1B, 0x00},
	{0x1C, 0x07},
	{0x1D, 0x7F},
	{0x1E, 0x08},
	{0x1F, 0x00},
	{0x20, 0xA0},
	{0x21, 0x42},
	{0x22, 0xF0},
	{0x23, 0xD8},
	{0x24, 0xBC},
	{0x25, 0xB8},
	{0x26, 0x44},
	{0x27, 0x2A},
	{0x28, 0x00},
	{0x29, 0x00},
	{0x2A, 0x78},
	{0x2B, 0x44},
	{0x2C, 0x40},
	{0x2D, 0x14},
	{0x2E, 0xA5},
	{0x2F, 0xE0},
	//{0x2F, 0xEe},   // blue color
	{0x30, 0x00},
	{0x31, 0x10},
	{0x32, 0xFF},
	{0x33, 0x05},
	{0x34, 0x1A},
	{0x35, 0xA0},
	{0x4C, 0x05},
	{0x4D, 0x40},
	{0x4E, 0x00},
	{0x4F, 0x00},
	{0x50, 0xA0},
	{0x51, 0x22},
	{0x52, 0x31},
	{0x53, 0x80},
	{0x54, 0x00},
	{0x55, 0x10},
	{0x6B, 0x06},
	{0x6C, 0x24},
	{0x6D, 0x00},
	{0x6E, 0x20},
	{0x6F, 0x13},
};

static void tw9990_hard_reset(void)
{
	int i = 0;
	int init_reg_array_size = sizeof(tw9990_register_array)/2;

	dev_dbg(&tw9990_data.sen.i2c_client->dev, "In tw9990:tw9990_hard_reset\n");
	
	for (i = 0; i < init_reg_array_size; i++) 
	{
		tw9990_write_reg(tw9990_register_array[i][0], 
			tw9990_register_array[i][1]);
	}
}

static void tw9990_read_reg(void)
{
	int i = 0;
	int ral;
	int init_reg_array_size = sizeof(tw9990_register_array)/2;

	for (i = 0; i < init_reg_array_size; i++) 
	{
		ral = tw9990_read(tw9990_register_array[i][0]);
//		printk("0x%X,0x%X\n",tw9990_register_array[i][0], ral);
	}

	return;
}

static inline void tw9990_reset(void)
{
	gpio_direction_output(TW9990_PWN, 0);
	gpio_direction_output(TW9990_RST, 1);

	msleep(10);
	gpio_set_value(TW9990_RST, 0);
	msleep(100);
	gpio_set_value(TW9990_RST, 1);
	msleep(10);
}

static int tw9990_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	int rev_id;
	int ret = 0;
	struct pinctrl *pinctrl;
	struct device *dev = &client->dev;

	printk(KERN_ERR"TW9990 sensor data is at %p\n", &tw9990_data);

	/* ov5640 pinctrl */
	pinctrl = devm_pinctrl_get_select_default(dev);
	if (IS_ERR(pinctrl)) {
		dev_err(dev, "setup pinctrl failed\n");
		return PTR_ERR(pinctrl);
	}

	tw9990_regulator_enable(dev);

	tw9990_reset();
	msleep(1);

	/* Set initial values for the sensor struct. */
	memset(&tw9990_data, 0, sizeof(tw9990_data));
	tw9990_data.sen.i2c_client = client;
	//tw9990_data.sen.streamcap.timeperframe.denominator = 30;
	tw9990_data.sen.streamcap.timeperframe.denominator = 25;
	tw9990_data.sen.streamcap.timeperframe.numerator = 1;
	tw9990_data.std_id = V4L2_STD_ALL;
	//tw9990_data.std_id = V4L2_STD_PAL;
	//video_idx = TW9990_NOT_LOCKED;
	video_idx = TW9990_PAL;
	tw9990_data.sen.pix.width = video_fmts[video_idx].raw_width;
	tw9990_data.sen.pix.height = video_fmts[video_idx].raw_height;
	tw9990_data.sen.pix.pixelformat =      V4L2_PIX_FMT_UYVY ; // V4L2_PIX_FMT_YUYV;  /* YUV422 */
	tw9990_data.sen.pix.priv = 1;  /* 1 is used to indicate TV in */
	tw9990_data.sen.on = true;

	tw9990_data.sen.sensor_clk = devm_clk_get(dev, "csi_mclk");
	if (IS_ERR(tw9990_data.sen.sensor_clk)) {
		dev_err(dev, "get mclk failed\n");
		return PTR_ERR(tw9990_data.sen.sensor_clk);
	}

	ret = of_property_read_u32(dev->of_node, "mclk", &tw9990_data.sen.mclk);
	if (ret) {
		dev_err(dev, "mclk frequency is invalid\n");
		return ret;
	}

	ret = of_property_read_u32(dev->of_node, "mclk_source", (u32 *) &(tw9990_data.sen.mclk_source));
	if (ret) {
		dev_err(dev, "mclk_source invalid\n");
		return ret;
	}

	ret = of_property_read_u32(dev->of_node, "csi_id",
					&(tw9990_data.sen.csi));

	if (ret) {
		dev_err(dev, "csi_id invalid\n");
		return ret;
	}

	clk_prepare_enable(tw9990_data.sen.sensor_clk);

	dev_dbg(&tw9990_data.sen.i2c_client->dev, "%s:tw9990 probe i2c address is 0x%02X\n",
		__func__, tw9990_data.sen.i2c_client->addr);
	
	/*! Read the revision ID of the tvin chip */
	rev_id = tw9990_read(TW9990_IDENT);
	dev_dbg(dev, "%s:Analog Device TW99%d detected!\n", __func__, GET_ID(rev_id));

	/*! TW9990 initialization. */
	tw9990_hard_reset();

	msleep(1000);
	
	tw9990_read_reg();

	pr_debug("   type is %d (expect %d)\n", tw9990_int_device.type, v4l2_int_type_slave);
	pr_debug("   num ioctls is %d\n", tw9990_int_device.u.slave->num_ioctls);

	/* This function attaches this structure to the /dev/video0 device.
	 * The pointer in priv points to the tw9990_data structure here.*/
	tw9990_int_device.priv = &tw9990_data;
	
	ret = v4l2_int_device_register(&tw9990_int_device);

	clk_disable_unprepare(tw9990_data.sen.sensor_clk);

	return ret;
}

static int tw9990_detach(struct i2c_client *client)
{
	dev_dbg(&tw9990_data.sen.i2c_client->dev, "%s:Removing %s video decoder @ 0x%02X from adapter %s\n",
		__func__, IF_NAME, client->addr << 1, client->adapter->name);

	if (dvddio_regulator)
		regulator_disable(dvddio_regulator);

	if (dvdd_regulator)
		regulator_disable(dvdd_regulator);

	if (avdd_regulator)
		regulator_disable(avdd_regulator);

	if (pvdd_regulator)
		regulator_disable(pvdd_regulator);

	v4l2_int_device_unregister(&tw9990_int_device);

	return 0;
}

static const struct i2c_device_id tw9990_id[] = {
	{"tw9990", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, tw9990_id);

static struct i2c_driver tw9990_i2c_driver = {
	.driver = {
		   .owner = THIS_MODULE,
		   .name = "tw9990",
		},
	.probe = tw9990_probe,
	.remove = tw9990_detach,
	.id_table = tw9990_id,
};

static __init int tw9990_init(void)
{
	u8 err = 0;

	pr_debug("In tw9990_init \n");

	/* Tells the i2c driver what functions to call for this driver. */
	err = i2c_add_driver(&tw9990_i2c_driver);
	if (err != 0)
		pr_err("%s:driver registration failed, error=%d\n", __func__, err);

	return err;
}

static void __exit tw9990_clean(void)
{
	dev_dbg(&tw9990_data.sen.i2c_client->dev, "In tw9990_clean\n");
	i2c_del_driver(&tw9990_i2c_driver);
}

MODULE_LICENSE("GPL");
module_init(tw9990_init);
module_exit(tw9990_clean);
