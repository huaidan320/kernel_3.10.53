/*
 * linux/sound/soc/codec/bt_codec.c
 *
 * Dummy codec for Bluetooth audio path
 *
 * Copyright (c) 2011 Cambridge Silicon Radio Limited, a CSR plc group company.
 *
 * Licensed under GPLv2 or later.
 */

#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/jiffies.h>
#include <linux/suspend.h>
#include <linux/notifier.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <sound/soc.h>

/* #define BT_CODEC_DEBUG */

#ifdef BT_CODEC_DEBUG
#define dprintk(x...) printk(x)
#else
#define dprintk(x...)
#endif


static int bt_codec_hw_params(struct snd_pcm_substream *substream,
			      struct snd_pcm_hw_params *params,
			      struct snd_soc_dai *dai)
{
	dprintk("%s\n", __func__);
	return 0;
}

static int bt_codec_mute(struct snd_soc_dai *dai, int mute)
{
	return 0;
}

static int bt_codec_set_dai_fmt(struct snd_soc_dai *codec_dai, unsigned int fmt)
{
	dprintk("%s\n", __func__);
	return 0;
}

static int bt_codec_set_dai_sysclk(struct snd_soc_dai *codec_dai,
				   int clk_id, unsigned int freq, int dir)
{
	dprintk("%s\n", __func__);
	return 0;
}

static int bt_codec_startup(struct snd_pcm_substream *substream,
			    struct snd_soc_dai *dai)
{
	dprintk("%s\n", __func__);
	return 0;
}

static void bt_codec_shutdown(struct snd_pcm_substream *substream,
			      struct snd_soc_dai *dai)
{
	dprintk("%s\n", __func__);
}

struct snd_soc_dai_ops bt_codec_ops = {
	.digital_mute = bt_codec_mute,
	.set_fmt = bt_codec_set_dai_fmt,
	.set_sysclk = bt_codec_set_dai_sysclk,
	.hw_params = bt_codec_hw_params,
	.startup = bt_codec_startup,
	.shutdown = bt_codec_shutdown,
};

static struct snd_soc_dai_driver bt_codec_dai = {
	.name = "bt-codec-pcm",
	.playback = {
		.stream_name = "BT Codec HiFi Playback",
		.channels_min = 1,
		.channels_max = 1,
		/* bt_codec supports all the rates from 8K to 96K */
		.rates = SNDRV_PCM_RATE_8000_96000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
	},
	.capture = {
		.stream_name = "BT Codec HiFi Capture",
		.channels_min = 1,
		.channels_max = 1,
		.rates = SNDRV_PCM_RATE_8000_96000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
	},
	.ops = &bt_codec_ops,
};

static int bt_codec_soc_probe(struct snd_soc_codec *codec)
{
	dprintk("%s\n", __func__);

	printk("sun*************************************bt_codec_soc_probe\n");
	return 0;
}

static int bt_codec_soc_remove(struct snd_soc_codec *codec)
{
	dprintk("%s\n", __func__);

	return 0;
}

#ifdef CONFIG_SUSPEND
static int bt_codec_soc_suspend(struct snd_soc_codec *codec)
{
	dprintk("%s\n", __func__);

	return 0;
}

static int bt_codec_soc_resume(struct snd_soc_codec *codec)
{
	dprintk("%s\n", __func__);

	return 0;
}
#else
#define bt_codec_soc_suspend NULL
#define bt_codec_soc_resume	 NULL
#endif /* CONFIG_SUSPEND */

static struct snd_soc_codec_driver soc_codec_bt = {
	.probe = bt_codec_soc_probe,
	.remove = bt_codec_soc_remove,
	.suspend = bt_codec_soc_suspend,
	.resume = bt_codec_soc_resume,
};

static int bt_codec_probe(struct platform_device *pdev)
{
	int ret = 0;

	ret = snd_soc_register_codec(&pdev->dev,
			&soc_codec_bt, &bt_codec_dai, 1);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register codec: %d\n", ret);
		printk(KERN_INFO "Failed to register codec: %d\n", ret);
		return ret;
	}

	return 0;
}

static int bt_codec_remove(struct platform_device *pdev)
{
	dprintk("%s\n", __func__);

	snd_soc_unregister_codec(&pdev->dev);

	return 0;
}

static const struct of_device_id bt_codec_dt_ids[] = {
	{ .compatible = "fsl,bt-codec", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, bt_codec_dt_ids);

static struct platform_driver imx_bt_audio_driver = {
	.driver = {
		.name = "bt-codec",
		.owner = THIS_MODULE,
		.of_match_table = bt_codec_dt_ids,
	},
	.probe = bt_codec_probe,
	.remove = bt_codec_remove,
};
module_platform_driver(imx_bt_audio_driver);

MODULE_DESCRIPTION("BT CODEC DRIVER");
MODULE_LICENSE("GPL");
