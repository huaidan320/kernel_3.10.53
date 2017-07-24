#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/jiffies.h>
#include <linux/suspend.h>
#include <linux/notifier.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <sound/soc.h>

#define BT_CODEC_DEBUG
#define AK7757_FORMATS (SNDRV_PCM_FMTBIT_S16_LE)      // setup real physical capacity, only support, S32 no support, S24 not sure, important! zhoujingyu
#define AK7757_PLAYBACK_RATES SNDRV_PCM_RATE_48000    // setup real physical capacity, only support,  cannot support less 48000, important!  zhoujingyu
#define AK7757_CAPTURE_RATES  AK7757_PLAYBACK_RATES

#ifdef BT_CODEC_DEBUG
#define dprintk(x...) printk(x)
#else
#define dprintk(x...)
#endif

static int ak7757_codec_hw_params(struct snd_pcm_substream *substream,
			      struct snd_pcm_hw_params *params,
			      struct snd_soc_dai *dai)
{
	dprintk("%s\n", __func__);
	return 0;
}

static int ak7757_codec_mute(struct snd_soc_dai *dai, int mute)
{
	return 0;
}

static int ak7757_codec_set_dai_fmt(struct snd_soc_dai *codec_dai, unsigned int fmt)
{
	dprintk("%s\n", __func__);
	return 0;
}

static int ak7757_codec_set_dai_sysclk(struct snd_soc_dai *codec_dai,
				   int clk_id, unsigned int freq, int dir)
{
	dprintk("%s\n", __func__);
	return 0;
}

static int ak7757_codec_startup(struct snd_pcm_substream *substream,
			    struct snd_soc_dai *dai)
{
	dprintk("%s\n", __func__);
	return 0;
}

static void ak7757_codec_shutdown(struct snd_pcm_substream *substream,
			      struct snd_soc_dai *dai)
{
	dprintk("%s\n", __func__);
}

struct snd_soc_dai_ops ak7757_codec_ops = {
	.digital_mute = ak7757_codec_mute,
	.set_fmt = ak7757_codec_set_dai_fmt,
	.set_sysclk = ak7757_codec_set_dai_sysclk,
	.hw_params = ak7757_codec_hw_params,
	.startup = ak7757_codec_startup,
	.shutdown = ak7757_codec_shutdown,
};

static struct snd_soc_dai_driver ak7757_codec_dai[] = {
	{
	.name = "ak7757-codec-pcm",
	.playback = {
		.stream_name = "ak7757 Codec HiFi Playback",
		.channels_min = 1, // 4,   //  setup real physical capacity, zhoujingyu
		.channels_max = 8, //4,   //  setup real physical capacity, zhoujingyu
		.rates = AK7757_PLAYBACK_RATES,
		.formats = AK7757_FORMATS,
	},
	.capture = {
		.stream_name = "ak7757 Codec HiFi Capture",
		.channels_min = 1,
		.channels_max = 8,
		.rates = AK7757_CAPTURE_RATES,
		.formats = AK7757_FORMATS,
	},
	.ops = &ak7757_codec_ops,
	},
	
	{
	.name = "ak7757-codec-pcm-1",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 1,
		.channels_max = 8,
		.rates = AK7757_PLAYBACK_RATES,
		.formats = AK7757_FORMATS,
	},
	.ops = &ak7757_codec_ops,
	},
};

static int ak7757_codec_soc_probe(struct snd_soc_codec *codec)
{
	dprintk("%s\n", __func__);

	return 0;
}

static int ak7757_codec_soc_remove(struct snd_soc_codec *codec)
{
	dprintk("%s\n", __func__);

	return 0;
}

#ifdef CONFIG_SUSPEND
static int ak7757_codec_soc_suspend(struct snd_soc_codec *codec)
{
	dprintk("%s\n", __func__);

	return 0;
}

static int ak7757_codec_soc_resume(struct snd_soc_codec *codec)
{
	dprintk("%s\n", __func__);

	return 0;
}
#else
#define ak7757_codec_soc_suspend NULL
#define ak7757_codec_soc_resume	 NULL
#endif /* CONFIG_SUSPEND */

static struct snd_soc_codec_driver soc_codec_ak7757 = {
	.probe = ak7757_codec_soc_probe,
	.remove = ak7757_codec_soc_remove,
	.suspend = ak7757_codec_soc_suspend,
	.resume = ak7757_codec_soc_resume,
};

static int ak7757_codec_probe(struct platform_device *pdev)
{
	int ret = 0;

	ret = snd_soc_register_codec(&pdev->dev,
			&soc_codec_ak7757, ak7757_codec_dai, 2);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register codec: %d\n", ret);
		printk(KERN_INFO "Failed to register codec: %d\n", ret);
		return ret;
	}

	return 0;
}

static int ak7757_codec_remove(struct platform_device *pdev)
{
	dprintk("%s\n", __func__);

	snd_soc_unregister_codec(&pdev->dev);

	return 0;
}

static const struct of_device_id ak7757_codec_dt_ids[] = {
	{ .compatible = "akm,ak7757", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, bt_codec_dt_ids);

static struct platform_driver imx_bt_audio_driver = {
	.driver = {
		.name = "ak7757",
		.owner = THIS_MODULE,
		.of_match_table = ak7757_codec_dt_ids,
	},
	.probe = ak7757_codec_probe,
	.remove = ak7757_codec_remove,
};
module_platform_driver(imx_bt_audio_driver);

MODULE_DESCRIPTION("AK7757 CODEC DRIVER");
MODULE_LICENSE("GPL");
