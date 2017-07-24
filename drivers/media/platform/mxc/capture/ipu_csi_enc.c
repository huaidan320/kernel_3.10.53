/*
 * Copyright 2009-2014 Freescale Semiconductor, Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

/*!
 * @file ipu_csi_enc.c
 *
 * @brief CSI Use case for video capture
 *
 * @ingroup IPU
 */

//sun modify debug
//#define CAMERA_DBG 1
//#define DEBUG 1

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/ipu.h>
#include <linux/mipi_csi2.h>
#include "mxc_v4l2_capture.h"
#include "ipu_prp_sw.h"

#ifdef CAMERA_DBG
	//#define CAMERA_TRACE(x) (printk)x
	#define CAMERA_TRACE(x) printk(x)
#else
	#define CAMERA_TRACE(x)
#endif

/*
 * Function definitions
 */

//sun modify add
static inline ipu_channel_t _get_ipu_channel(int csi_id)
{
	return (csi_id == 0)?CSI_MEM0:CSI_MEM1;
}
static inline int _get_ipu_irq(int csi_id)
{
	return (csi_id == 0)?IPU_IRQ_CSI0_OUT_EOF:IPU_IRQ_CSI1_OUT_EOF;
}


/*!
 * csi ENC callback function.
 *
 * @param irq       int irq line
 * @param dev_id    void * device id
 *
 * @return status   IRQ_HANDLED for handled
 */
static irqreturn_t csi_enc_callback(int irq, void *dev_id)
{
	cam_data *cam = (cam_data *) dev_id;

	//if you add printk,you can make a mistake 
	//printk("*****csi_enc_callback*******\n"); //add sun modify

	if (cam->enc_callback == NULL)
		return IRQ_HANDLED;

	cam->enc_callback(irq, dev_id);
	return IRQ_HANDLED;
}

/*!
 * CSI ENC enable channel setup function
 *
 * @param cam       struct cam_data * mxc capture instance
 *
 * @return  status
 */
static int csi_enc_setup(cam_data *cam)
{
	ipu_channel_params_t params;
	u32 pixel_fmt;
	int err = 0, sensor_protocol = 0;
	dma_addr_t dummy = cam->dummy_frame.buffer.m.offset;
#ifdef CONFIG_MXC_MIPI_CSI2
	void *mipi_csi2_info;
	int ipu_id;
	int csi_id;
#endif

	CAMERA_TRACE("In csi_enc_setup\n");
	if (!cam) {
		printk(KERN_ERR "cam private is NULL\n");
		return -ENXIO;
	}

	//sun modify add
	pr_debug(KERN_ERR "cam->csi =%d\n", cam->csi);

	memset(&params, 0, sizeof(ipu_channel_params_t));
	params.csi_mem.csi = cam->csi;

	sensor_protocol = ipu_csi_get_sensor_protocol(cam->ipu, cam->csi);
	switch (sensor_protocol) {
	case IPU_CSI_CLK_MODE_GATED_CLK:
	case IPU_CSI_CLK_MODE_NONGATED_CLK:
	case IPU_CSI_CLK_MODE_CCIR656_PROGRESSIVE:
	case IPU_CSI_CLK_MODE_CCIR1120_PROGRESSIVE_DDR:
	case IPU_CSI_CLK_MODE_CCIR1120_PROGRESSIVE_SDR:
		params.csi_mem.interlaced = false;

		pr_debug("csi_mem.interlaced = flase\n"); //add sun modify

		break;
	case IPU_CSI_CLK_MODE_CCIR656_INTERLACED:
	case IPU_CSI_CLK_MODE_CCIR1120_INTERLACED_DDR:
	case IPU_CSI_CLK_MODE_CCIR1120_INTERLACED_SDR:
		params.csi_mem.interlaced = true;

		pr_debug("csi_mem.interlaced = true\n"); //add sun modify

		break;
	default:
		printk(KERN_ERR "sensor protocol unsupported\n");
		return -EINVAL;
	}

	if (cam->v2f.fmt.pix.pixelformat == V4L2_PIX_FMT_YUV420)
	{
		pixel_fmt = IPU_PIX_FMT_YUV420P;
		printk("*******sun IPU_PIX_FMT_YUV420P******\n");
	}
	else if (cam->v2f.fmt.pix.pixelformat == V4L2_PIX_FMT_YVU420)
	{
		pixel_fmt = IPU_PIX_FMT_YVU420P;
		printk("*******sun IPU_PIX_FMT_YVU420P******\n");
	}
	else if (cam->v2f.fmt.pix.pixelformat == V4L2_PIX_FMT_YUV422P)
	{	
		pixel_fmt = IPU_PIX_FMT_YUV422P;
	printk("*******sun IPU_PIX_FMT_YUV422P******\n");
	}
	else if (cam->v2f.fmt.pix.pixelformat == V4L2_PIX_FMT_UYVY)
	{
		pixel_fmt = IPU_PIX_FMT_UYVY;
		printk("*******sun IPU_PIX_FMT_UYVY******\n");
	}
	else if (cam->v2f.fmt.pix.pixelformat == V4L2_PIX_FMT_YUYV)
	{
		pixel_fmt = IPU_PIX_FMT_YUYV;
		printk("*******sun IPU_PIX_FMT_YUYV******\n");
	}
	else if (cam->v2f.fmt.pix.pixelformat == V4L2_PIX_FMT_NV12)
	{
		pixel_fmt = IPU_PIX_FMT_NV12;
		printk("*******sun IPU_PIX_FMT_NV12******\n");
	}
	else if (cam->v2f.fmt.pix.pixelformat == V4L2_PIX_FMT_BGR24)
	{
		pixel_fmt = IPU_PIX_FMT_BGR24;
		printk("*******sun IPU_PIX_FMT_BGR24******\n");
	}
	else if (cam->v2f.fmt.pix.pixelformat == V4L2_PIX_FMT_RGB24)
	{
		pixel_fmt = IPU_PIX_FMT_RGB24;
		printk("*******sun IPU_PIX_FMT_RGB24******\n");
	}
	else if (cam->v2f.fmt.pix.pixelformat == V4L2_PIX_FMT_RGB565)
	{
		pixel_fmt = IPU_PIX_FMT_RGB565;
		printk("*******sun IPU_PIX_FMT_RGB565******\n");
	}
	else if (cam->v2f.fmt.pix.pixelformat == V4L2_PIX_FMT_BGR32)
	{
		pixel_fmt = IPU_PIX_FMT_BGR32;
		printk("*******sun IPU_PIX_FMT_BGR32******\n");
	}
	else if (cam->v2f.fmt.pix.pixelformat == V4L2_PIX_FMT_RGB32)
	{
		pixel_fmt = IPU_PIX_FMT_RGB32;
		printk("*******sun IPU_PIX_FMT_RGB32******\n");
	}
	//add sun
	else if (cam->v2f.fmt.pix.pixelformat == V4L2_PIX_FMT_SRGGB8)
	{
		//raw 8
		pixel_fmt = IPU_PIX_FMT_GENERIC;
		pr_debug("*******sun IPU_PIX_FMT_GENERIC******\n");
	}
	else {
		printk(KERN_ERR "format not supported\n");
		return -EINVAL;
	}

	//raw 10
	//pixel_fmt = IPU_PIX_FMT_GENERIC_16;
	pr_debug("*******sun IPU_PIX_FMT_GENERIC******\n");
	
	//pixel_fmt = IPU_PIX_FMT_GREY;
	//printk("********sun IPU_PIX_FMT_GENRIC*****\n");

#ifdef CONFIG_MXC_MIPI_CSI2
	mipi_csi2_info = mipi_csi2_get_info();

	//sun add debug
	printk("***sun config_mxc_mipi_csi2***\n");

	if (mipi_csi2_info) {
		if (mipi_csi2_get_status(mipi_csi2_info)) {
			ipu_id = mipi_csi2_get_bind_ipu(mipi_csi2_info);
			csi_id = mipi_csi2_get_bind_csi(mipi_csi2_info);

			//sun add debug
			printk("*****sun ipu_id = %d******\n", ipu_id);
			printk("*****sun csi_id = %d******\n", csi_id);

			if (cam->ipu == ipu_get_soc(ipu_id)
				&& cam->csi == csi_id) {
				params.csi_mem.mipi_en = true;
				params.csi_mem.mipi_vc =
				mipi_csi2_get_virtual_channel(mipi_csi2_info);

				//sun add debug
				printk("*****sun v_channel = %d****\n*", params.csi_mem.mipi_vc);

				params.csi_mem.mipi_id =
				mipi_csi2_get_datatype(mipi_csi2_info);

				//sun add debug
				printk("*****sun mipi_id = %d*****\n", params.csi_mem.mipi_id);

				mipi_csi2_pixelclk_enable(mipi_csi2_info);
			} else {
				params.csi_mem.mipi_en = false;
				params.csi_mem.mipi_vc = 0;
				params.csi_mem.mipi_id = 0;
			}
		} else {
			params.csi_mem.mipi_en = false;
			params.csi_mem.mipi_vc = 0;
			params.csi_mem.mipi_id = 0;
		}
	}
#endif

	//sun modify add
	//err = ipu_init_channel(cam->ipu, CSI_MEM, &params);
	err = ipu_init_channel(cam->ipu, _get_ipu_channel(cam->csi), &params);
	if (err != 0) {
		printk(KERN_ERR "ipu_init_channel %d\n", err);
		return err;
	}

	//sun modify add
	//err = ipu_init_channel_buffer(cam->ipu, CSI_MEM, IPU_OUTPUT_BUFFER,
	err = ipu_init_channel_buffer(cam->ipu, _get_ipu_channel(cam->csi), IPU_OUTPUT_BUFFER,
				      pixel_fmt, cam->v2f.fmt.pix.width,
				      cam->v2f.fmt.pix.height,
				      cam->v2f.fmt.pix.bytesperline,
				      IPU_ROTATE_NONE,
				      dummy, dummy, 0,
				      cam->offset.u_offset,
				      cam->offset.v_offset);
	if (err != 0) {
		printk(KERN_ERR "CSI_MEM output buffer\n");
		return err;
	}

	//sun modify add
	//err = ipu_enable_channel(cam->ipu, CSI_MEM);
	err = ipu_enable_channel(cam->ipu, _get_ipu_channel(cam->csi));
	if (err < 0) {
		printk(KERN_ERR "ipu_enable_channel CSI_MEM\n");
		return err;
	}

	return err;
}

/*!
 * function to update physical buffer address for encorder IDMA channel
 *
 * @param eba         physical buffer address for encorder IDMA channel
 * @param buffer_num  int buffer 0 or buffer 1
 *
 * @return  status
 */
//sun modify add
//static int csi_enc_eba_update(struct ipu_soc *ipu, dma_addr_t eba, int *buffer_num)
static int csi_enc_eba_update1(struct ipu_soc *ipu, dma_addr_t eba, int *buffer_num)
{
	int err = 0;

	//pr_debug("eba %x\n", eba);
	//printk("****sun eba = %x*****\n", eba);
	//printk("****sun buffer_num = %d****\n", *buffer_num);

	//sun modify add
	//err = ipu_update_channel_buffer(ipu, CSI_MEM, IPU_OUTPUT_BUFFER,
	err = ipu_update_channel_buffer(ipu, CSI_MEM1, IPU_OUTPUT_BUFFER,
					*buffer_num, eba);
	if (err != 0) {
	//sun modify add
	//	ipu_clear_buffer_ready(ipu, CSI_MEM, IPU_OUTPUT_BUFFER,
	ipu_clear_buffer_ready(ipu, CSI_MEM1, IPU_OUTPUT_BUFFER,
				       *buffer_num);

	//sun modify add
	//	err = ipu_update_channel_buffer(ipu, CSI_MEM, IPU_OUTPUT_BUFFER,
	err = ipu_update_channel_buffer(ipu, CSI_MEM1, IPU_OUTPUT_BUFFER,
						*buffer_num, eba);
		if (err != 0) {
			pr_err("ERROR: v4l2 capture: fail to update "
			       "buf%d\n", *buffer_num);
			return err;
		}
	}

	//sun modify add
	//ipu_select_buffer(ipu, CSI_MEM, IPU_OUTPUT_BUFFER, *buffer_num);
	ipu_select_buffer(ipu, CSI_MEM1, IPU_OUTPUT_BUFFER, *buffer_num);

	*buffer_num = (*buffer_num == 0) ? 1 : 0;

	return 0;
}

//sun modify add
static int csi_enc_eba_update0(struct ipu_soc *ipu, dma_addr_t eba, int *buffer_num)
{
	int err = 0;

	pr_debug("eba %x\n", eba);
	err = ipu_update_channel_buffer(ipu, CSI_MEM0, IPU_OUTPUT_BUFFER,
			*buffer_num, eba);
	if (err != 0) {
		ipu_clear_buffer_ready(ipu, CSI_MEM0, IPU_OUTPUT_BUFFER,
				*buffer_num);

		err = ipu_update_channel_buffer(ipu, CSI_MEM0, IPU_OUTPUT_BUFFER,
				*buffer_num, eba);
		if (err != 0) {
			pr_err("ERROR: v4l2 capture: fail to update "
					"buf%d\n", *buffer_num);
			return err;
		}
	}

	ipu_select_buffer(ipu, CSI_MEM0, IPU_OUTPUT_BUFFER, *buffer_num);

	*buffer_num = (*buffer_num == 0) ? 1 : 0;

	return 0;
}

/*!
 * Enable encoder task
 * @param private       struct cam_data * mxc capture instance
 *
 * @return  status
 */
static int csi_enc_enabling_tasks(void *private)
{
	cam_data *cam = (cam_data *) private;
	int err = 0;
	CAMERA_TRACE("IPU:In csi_enc_enabling_tasks\n");

	cam->dummy_frame.vaddress = dma_alloc_coherent(0,
			       PAGE_ALIGN(cam->v2f.fmt.pix.sizeimage),
			       &cam->dummy_frame.paddress,
			       GFP_DMA | GFP_KERNEL);
	if (cam->dummy_frame.vaddress == 0) {
		pr_err("ERROR: v4l2 capture: Allocate dummy frame "
		       "failed.\n");
		return -ENOBUFS;
	}
	cam->dummy_frame.buffer.type = V4L2_BUF_TYPE_PRIVATE;
	cam->dummy_frame.buffer.length =
	    PAGE_ALIGN(cam->v2f.fmt.pix.sizeimage);
	cam->dummy_frame.buffer.m.offset = cam->dummy_frame.paddress;

	//sun modify add
	//ipu_clear_irq(cam->ipu, IPU_IRQ_CSI0_OUT_EOF);
	//err = ipu_request_irq(cam->ipu, IPU_IRQ_CSI0_OUT_EOF,

	ipu_clear_irq(cam->ipu, _get_ipu_irq(cam->csi));
	err = ipu_request_irq(cam->ipu, _get_ipu_irq(cam->csi),
			      csi_enc_callback, 0, "Mxc Camera", cam);
	
	if (err != 0) {
		printk(KERN_ERR "Error registering rot irq\n");
		return err;
	}

	err = csi_enc_setup(cam);
	if (err != 0) {
		printk(KERN_ERR "csi_enc_setup %d\n", err);
		return err;
	}

	return err;
}

/*!
 * Disable encoder task
 * @param private       struct cam_data * mxc capture instance
 *
 * @return  int
 */
static int csi_enc_disabling_tasks(void *private)
{
	cam_data *cam = (cam_data *) private;
	int err = 0;
#ifdef CONFIG_MXC_MIPI_CSI2
	void *mipi_csi2_info;
	int ipu_id;
	int csi_id;
#endif

	//sun modify add
	//err = ipu_disable_channel(cam->ipu, CSI_MEM, true);
	err = ipu_disable_channel(cam->ipu, _get_ipu_channel(cam->csi), true);

	//sun modify add
	//ipu_uninit_channel(cam->ipu, CSI_MEM);
	ipu_uninit_channel(cam->ipu, _get_ipu_channel(cam->csi));

	if (cam->dummy_frame.vaddress != 0) {
		dma_free_coherent(0, cam->dummy_frame.buffer.length,
				  cam->dummy_frame.vaddress,
				  cam->dummy_frame.paddress);
		cam->dummy_frame.vaddress = 0;
	}

#ifdef CONFIG_MXC_MIPI_CSI2
	mipi_csi2_info = mipi_csi2_get_info();

	if (mipi_csi2_info) {
		if (mipi_csi2_get_status(mipi_csi2_info)) {
			ipu_id = mipi_csi2_get_bind_ipu(mipi_csi2_info);
			csi_id = mipi_csi2_get_bind_csi(mipi_csi2_info);

			if (cam->ipu == ipu_get_soc(ipu_id)
				&& cam->csi == csi_id)
				mipi_csi2_pixelclk_disable(mipi_csi2_info);
		}
	}
#endif

	return err;
}

/*!
 * Enable csi
 * @param private       struct cam_data * mxc capture instance
 *
 * @return  status
 */
static int csi_enc_enable_csi(void *private)
{
	cam_data *cam = (cam_data *) private;

	return ipu_enable_csi(cam->ipu, cam->csi);
}

/*!
 * Disable csi
 * @param private       struct cam_data * mxc capture instance
 *
 * @return  status
 */
static int csi_enc_disable_csi(void *private)
{
	cam_data *cam = (cam_data *) private;

	/* free csi eof irq firstly.
	 * when disable csi, wait for idmac eof.
	 * it requests eof irq again */

	//sun modify add
	//ipu_free_irq(cam->ipu, IPU_IRQ_CSI0_OUT_EOF, cam);
	ipu_free_irq(cam->ipu, _get_ipu_irq(cam->csi), cam);

	printk("\nMakis is invoking csi_enc_disable_csi(), csi=%d \n", cam->csi);

	return ipu_disable_csi(cam->ipu, cam->csi);
}

/*!
 * function to select CSI ENC as the working path
 *
 * @param private       struct cam_data * mxc capture instance
 *
 * @return  int
 */
int csi_enc_select(void *private)
{
	cam_data *cam = (cam_data *) private;
	int err = 0;

	if (cam) {

		//sun modify add
		//cam->enc_update_eba = csi_enc_eba_update;
		if (cam->csi == 0) cam->enc_update_eba = csi_enc_eba_update0;
		if (cam->csi == 1) cam->enc_update_eba = csi_enc_eba_update1;

		cam->enc_enable = csi_enc_enabling_tasks;
		cam->enc_disable = csi_enc_disabling_tasks;
		cam->enc_enable_csi = csi_enc_enable_csi;
		cam->enc_disable_csi = csi_enc_disable_csi;
	} else {
		err = -EIO;
	}

	return err;
}
EXPORT_SYMBOL(csi_enc_select);

/*!
 * function to de-select CSI ENC as the working path
 *
 * @param private       struct cam_data * mxc capture instance
 *
 * @return  int
 */
int csi_enc_deselect(void *private)
{
	cam_data *cam = (cam_data *) private;
	int err = 0;

	if (cam) {
		cam->enc_update_eba = NULL;
		cam->enc_enable = NULL;
		cam->enc_disable = NULL;
		cam->enc_enable_csi = NULL;
		cam->enc_disable_csi = NULL;
	}

	return err;
}
EXPORT_SYMBOL(csi_enc_deselect);

/*!
 * Init the Encorder channels
 *
 * @return  Error code indicating success or failure
 */
__init int csi_enc_init(void)
{
	return 0;
}

/*!
 * Deinit the Encorder channels
 *
 */
void __exit csi_enc_exit(void)
{
}

module_init(csi_enc_init);
module_exit(csi_enc_exit);

MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("CSI ENC Driver");
MODULE_LICENSE("GPL");
