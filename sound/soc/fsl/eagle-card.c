#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_i2c.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/initval.h>
#include <sound/pcm_params.h>

#include "fsl_esai.h"
#include "fsl_asrc.h"

#define CODEC_CLK_EXTER_OSC   1
#define CODEC_CLK_ESAI_HCKT   2
#define SUPPORT_RATE_NUM    10

struct imx_priv {
	unsigned int mclk_freq;
	struct platform_device *pdev;
	struct platform_device *asrc_pdev;
};

static struct imx_priv card_priv;

static int imx_cs42888_surround_hw_params(struct snd_pcm_substream *substream,
					 struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct imx_priv *priv = &card_priv;
	u32 dai_format = 0;

	dai_format = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
			SND_SOC_DAIFMT_CBM_CFM;
	
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		snd_soc_dai_set_sysclk(cpu_dai, ESAI_HCKT_EXTAL,
			       priv->mclk_freq, SND_SOC_CLOCK_IN);
	else
		snd_soc_dai_set_sysclk(cpu_dai, ESAI_HCKR_EXTAL,
			       priv->mclk_freq, SND_SOC_CLOCK_IN);
	
	snd_soc_dai_set_sysclk(codec_dai, 1, 12000000, SND_SOC_CLOCK_OUT);

	/* set cpu DAI configuration */
	snd_soc_dai_set_fmt(cpu_dai, dai_format);
	/* set i.MX active slot mask */
	snd_soc_dai_set_tdm_slot(cpu_dai, 0x3, 0x3, 2, 32);

	/* set codec DAI configuration */
	snd_soc_dai_set_fmt(codec_dai, dai_format);
	
	return 0;
}

static int imx_cs42888_surround_startup(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	static struct snd_pcm_hw_constraint_list constraint_rates;
	struct imx_priv *priv = &card_priv;
	struct device *dev = &priv->pdev->dev;
	static u32 support_rates[SUPPORT_RATE_NUM];
	int ret;
	
	if (priv->mclk_freq == 24576000) {
		support_rates[0] = 48000;
		support_rates[1] = 96000;
		support_rates[2] = 192000;
		constraint_rates.list = support_rates;
		constraint_rates.count = 3;

		ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
							&constraint_rates);
		if (ret)
			return ret;
	} else
		dev_warn(dev, "mclk may be not supported %d\n", priv->mclk_freq);

	return 0;
}

static struct snd_soc_ops imx_cs42888_surround_ops = {
	.startup = imx_cs42888_surround_startup,
	.hw_params = imx_cs42888_surround_hw_params,
};

/**
 * imx_cs42888_surround_startup() is to set constrain for hw parameter, but
 * backend use same runtime as frontend, for p2p backend need to use different
 * parameter, so backend can't use the startup.
 */
static struct snd_soc_ops imx_cs42888_surround_ops_be = {
	.hw_params = imx_cs42888_surround_hw_params,
};


static const struct snd_soc_dapm_widget imx_cs42888_dapm_widgets[] = {
	SND_SOC_DAPM_LINE("Line Out Jack", NULL),
	SND_SOC_DAPM_LINE("Line In Jack", NULL),
};

static const struct snd_soc_dapm_route audio_map[] = {
	/* Line out jack */
	{"Line Out Jack", NULL, "AOUT1L"},
	{"Line Out Jack", NULL, "AOUT1R"},
	{"Line Out Jack", NULL, "AOUT2L"},
	{"Line Out Jack", NULL, "AOUT2R"},
	{"Line Out Jack", NULL, "AOUT3L"},
	{"Line Out Jack", NULL, "AOUT3R"},
	{"Line Out Jack", NULL, "AOUT4L"},
	{"Line Out Jack", NULL, "AOUT4R"},
	{"AIN1L", NULL, "Line In Jack"},
	{"AIN1R", NULL, "Line In Jack"},
	{"AIN2L", NULL, "Line In Jack"},
	{"AIN2R", NULL, "Line In Jack"},
	{"esai-Playback",  NULL, "asrc-Playback"},
	{"Playback",  NULL, "esai-Playback"},/* dai route for be and fe */
	{"asrc-Capture",  NULL, "esai-Capture"},
	{"esai-Capture",  NULL, "Capture"},
};

static int be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
				struct snd_pcm_hw_params *params) {

	struct imx_priv *priv = &card_priv;
	struct fsl_asrc_p2p *asrc_p2p;

	if (!priv->asrc_pdev)
		return -EINVAL;

	asrc_p2p = platform_get_drvdata(priv->asrc_pdev);

	hw_param_interval(params, SNDRV_PCM_HW_PARAM_RATE)->min = asrc_p2p->p2p_rate;
	hw_param_interval(params, SNDRV_PCM_HW_PARAM_RATE)->max = asrc_p2p->p2p_rate;
	snd_mask_none(hw_param_mask(params, SNDRV_PCM_HW_PARAM_FORMAT));
	if (asrc_p2p->p2p_width == 16)
		snd_mask_set(hw_param_mask(params, SNDRV_PCM_HW_PARAM_FORMAT),
							SNDRV_PCM_FORMAT_S16_LE);
	else
		snd_mask_set(hw_param_mask(params, SNDRV_PCM_HW_PARAM_FORMAT),
							SNDRV_PCM_FORMAT_S24_LE);
	return 0;
}

static struct snd_soc_dai_link imx_cs42888_dai[] = {
	{
		.name = "HiFi",
		.stream_name = "HiFi",
		.codec_dai_name = "ak7757-codec-pcm",
		.ops = &imx_cs42888_surround_ops,
	},
	{
		.name = "HiFi-ASRC-FE",
		.stream_name = "HiFi-ASRC-FE",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dynamic = 1,
	},
	{
		.name = "HiFi-ASRC-BE",
		.stream_name = "HiFi-ASRC-BE",
		.codec_dai_name = "ak7757-codec-pcm",
		.platform_name = "snd-soc-dummy",
		.no_pcm = 1,
		.ops = &imx_cs42888_surround_ops_be,
		.be_hw_params_fixup = be_hw_params_fixup,
	},
};

static struct snd_soc_card snd_soc_card_imx_cs42888 = {
	.name = "imx-ak7757",
	.dai_link = imx_cs42888_dai,
	.dapm_widgets = imx_cs42888_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(imx_cs42888_dapm_widgets),
	//.dapm_routes = audio_map,
	//.num_dapm_routes = ARRAY_SIZE(audio_map),
};

/*
 * This function will register the snd_soc_pcm_link drivers.
 */
static int imx_cs42888_probe(struct platform_device *pdev)
{
	struct device_node *esai_np, *codec_np;
	struct device_node *asrc_np;
	struct platform_device *esai_pdev;
	struct platform_device *asrc_pdev = NULL;
	struct platform_device *codec_dev;
	struct imx_priv *priv = &card_priv;
	struct clk *codec_clk = NULL;
	int ret;

	priv->pdev = pdev;
	priv->asrc_pdev = NULL;

	esai_np = of_parse_phandle(pdev->dev.of_node, "esai-controller", 0);
	codec_np = of_parse_phandle(pdev->dev.of_node, "audio-codec", 0);
	if (!esai_np || !codec_np) {
		dev_err(&pdev->dev, "phandle missing or invalid\n");
		ret = -EINVAL;
		goto fail;
	}

	asrc_np = of_parse_phandle(pdev->dev.of_node, "asrc-controller", 0);
	if (asrc_np) {
		asrc_pdev = of_find_device_by_node(asrc_np);
		if (asrc_pdev) {
			struct fsl_asrc_p2p *asrc_p2p;
			priv->asrc_pdev = asrc_pdev;
			asrc_p2p = platform_get_drvdata(asrc_pdev);
			if (!asrc_p2p) {
				dev_err(&pdev->dev, "failed to get p2p params\n");
				ret = -EINVAL;
				goto fail;
			}
			asrc_p2p->per_dev = ESAI;
		}
	}

	esai_pdev = of_find_device_by_node(esai_np);
	if (!esai_pdev) {
		dev_err(&pdev->dev, "failed to find ESAI platform device\n");
		ret = -EINVAL;
		goto fail;
	}
	codec_dev = of_find_device_by_node(codec_np);
	if (!codec_dev) {
		dev_err(&pdev->dev, "failed to find codec platform device\n");
		ret = -EINVAL;
		goto fail;
	}

	/*if there is no asrc controller, we only enable one device*/
	if (!asrc_pdev) {
		imx_cs42888_dai[0].codec_of_node   = codec_np;
		imx_cs42888_dai[0].cpu_dai_name    = dev_name(&esai_pdev->dev);
		imx_cs42888_dai[0].platform_of_node = esai_np;
		snd_soc_card_imx_cs42888.num_links = 1;

		printk("*****************!asrc_pdev\n");
	} else {

		imx_cs42888_dai[0].codec_of_node   = codec_np;
		imx_cs42888_dai[0].cpu_dai_name    = dev_name(&esai_pdev->dev);
		imx_cs42888_dai[0].platform_of_node = esai_np;
		imx_cs42888_dai[0].ignore_pmdown_time = 1;

		imx_cs42888_dai[1].cpu_dai_name    = dev_name(&asrc_pdev->dev);
		imx_cs42888_dai[1].platform_name   = "imx-pcm-asrc";
		imx_cs42888_dai[1].ignore_pmdown_time = 1;

		imx_cs42888_dai[2].codec_of_node   = codec_np;
		imx_cs42888_dai[2].cpu_dai_name    = dev_name(&esai_pdev->dev);
		imx_cs42888_dai[2].ignore_pmdown_time = 1;
		snd_soc_card_imx_cs42888.num_links = 3;

		printk("*****************asrc_pdev\n");
	}

	codec_clk = devm_clk_get(&codec_dev->dev, NULL);
	if (IS_ERR(codec_clk)) {
		ret = PTR_ERR(codec_clk);
		dev_err(&codec_dev->dev, "failed to get codec clk: %d\n", ret);
		goto fail;
	}
	priv->mclk_freq = clk_get_rate(codec_clk);

	snd_soc_card_imx_cs42888.dev = &pdev->dev;

	platform_set_drvdata(pdev, &snd_soc_card_imx_cs42888);

	ret = snd_soc_register_card(&snd_soc_card_imx_cs42888);
	if (ret)
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n", ret);
fail:
	if (esai_np)
		of_node_put(esai_np);
	if (codec_np)
		of_node_put(codec_np);
	return ret;
}

static int imx_cs42888_remove(struct platform_device *pdev)
{
	snd_soc_unregister_card(&snd_soc_card_imx_cs42888);
	return 0;
}

static const struct of_device_id imx_cs42888_dt_ids[] = {
	{ .compatible = "fsl,imx-audio-ak7757-codec", },
	{ /* sentinel */ }
};

static struct platform_driver imx_cs42888_driver = {
	.probe = imx_cs42888_probe,
	.remove = imx_cs42888_remove,
	.driver = {
		.name = "imx-ak7757",
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = imx_cs42888_dt_ids,
	},
};
module_platform_driver(imx_cs42888_driver);

MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("ALSA SoC cs42888 Machine Layer Driver");
MODULE_ALIAS("platform:imx-cs42888");
MODULE_LICENSE("GPL");
