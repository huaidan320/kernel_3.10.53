#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/fsl_devices.h>
#include <linux/clk.h>
#include <sound/pcm_params.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <asm/mach-types.h>
#include "imx-audmux.h"

#include "fsl_esai.h"
#include "fsl_asrc.h"

#define SUPPORT_RATE_NUM    10
#define BT_CODEC_DEBUG

#ifdef BT_CODEC_DEBUG
#define dprintk(x...) printk(x)
#else
#define dprintk(x...)
#endif

#define DAI_NAME_SIZE	32

struct imx_ak7757_data {
	struct snd_soc_dai_link dai[2];
	struct snd_soc_card card;
	char codec_dai_name[DAI_NAME_SIZE];
	char platform_name[DAI_NAME_SIZE];
	struct clk *codec_clk;
	unsigned int clk_frequency;
};

struct imx_priv {
	unsigned int mclk_freq;
	struct platform_device *pdev;
	struct platform_device *asrc_pdev;
};

static struct imx_priv card_priv;

static int imx_ak7757_params(struct snd_pcm_substream *substream,
        struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct imx_priv *priv = &card_priv;
	u32 dai_format = 0;

	dai_format = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
			SND_SOC_DAIFMT_CBM_CFM;

	/* set cpu DAI configuration */
	snd_soc_dai_set_fmt(cpu_dai, dai_format);
	/* set i.MX active slot mask */
	snd_soc_dai_set_tdm_slot(cpu_dai, 0x3, 0x3, 2, 32);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
	{
		dprintk("substream->stream == SNDRV_PCM_STREAM_PLAYBACK,running here\n");
	
		snd_soc_dai_set_sysclk(cpu_dai, ESAI_HCKT_EXTAL,
			       priv->mclk_freq, SND_SOC_CLOCK_IN);
	}
	else
	{
		dprintk("substream->stream != SNDRV_PCM_STREAM_PLAYBACK,running here\n");
		
		snd_soc_dai_set_sysclk(cpu_dai, ESAI_HCKR_EXTAL,
			       priv->mclk_freq, SND_SOC_CLOCK_IN);	
	}

#if 0
	/* set the ratio */
	snd_soc_dai_set_clkdiv(cpu_dai, ESAI_TX_DIV_PSR, 1);

	snd_soc_dai_set_clkdiv(cpu_dai, ESAI_TX_DIV_PM, 2);
	
	snd_soc_dai_set_clkdiv(cpu_dai, ESAI_TX_DIV_FP, 5);
#endif

	snd_soc_dai_set_sysclk(codec_dai, 1, 12000000, SND_SOC_CLOCK_OUT);
	
	/* set codec DAI configuration */
	snd_soc_dai_set_fmt(codec_dai, dai_format);

	dprintk("%s,running here\n", __FUNCTION__);
	
	return 0;
}		

static int imx_cs42888_surround_startup(struct snd_pcm_substream *substream)
{
#if 0
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
		support_rates[3] = 44100;
		support_rates[4] = 8000;
		constraint_rates.list = support_rates;
		constraint_rates.count = 5;

		ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
							&constraint_rates);
		if (ret)
			return ret;
	} else
		dev_warn(dev, "mclk may be not supported %d\n", priv->mclk_freq);
#endif
	dprintk("%s,running here\n", __FUNCTION__);
	
	return 0;
}

static struct snd_soc_ops imx_ak7757_ops = {
	.startup = imx_cs42888_surround_startup,
    .hw_params = imx_ak7757_params,
};

static int imx_ak7757_dai_init(struct snd_soc_pcm_runtime *rtd)
{
	dprintk("%s, reset both codec and btcodec modules\n", __FUNCTION__);
	return 0;
}

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
	//{"esai-Playback",  NULL, "asrc-Playback"},
	//{"Playback",  NULL, "esai-Playback"},/* dai route for be and fe */
	//{"asrc-Capture",  NULL, "esai-Capture"},
	//{"esai-Capture",  NULL, "Capture"},
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

	dprintk("be_hw_params_fixup,running here\n");
	return 0;
}

static int imx_ak7757_probe(struct platform_device *pdev)
{
	struct device_node *esai_np, *codec_np;
	struct device_node *asrc_np;
	struct platform_device *esai_pdev;
	struct platform_device *asrc_pdev = NULL;
	struct platform_device *codec_dev;
	struct imx_priv *priv = &card_priv;
	struct clk *codec_clk = NULL;
	struct imx_ak7757_data *data;
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

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data) {
		ret = -ENOMEM;
		goto fail;
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
	
	data->dai[0].name = "HiFi";
	data->dai[0].stream_name = "HiFi";
	data->dai[0].codec_dai_name = "ak7757-codec-pcm";
	data->dai[0].platform_of_node = esai_np;
	data->dai[0].codec_of_node = codec_np;
	data->dai[0].cpu_dai_name = dev_name(&esai_pdev->dev);
	//data->dai[0].ignore_pmdown_time = 1;
	data->dai[0].init = &imx_ak7757_dai_init;
	data->dai[0].dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
			    SND_SOC_DAIFMT_CBM_CFM;
	data->dai[0].ops  = &imx_ak7757_ops,

	data->dai[1].name = "HiFi-1";
	data->dai[1].stream_name = "HiFi-1";
	data->dai[1].codec_dai_name = "ak7757-codec-pcm-1";
	data->dai[1].platform_of_node = esai_np;
	//data->dai[1].platform_name = "snd-soc-dummy",
	data->dai[1].codec_of_node = codec_np;
	data->dai[1].cpu_dai_name = dev_name(&esai_pdev->dev);
	//data->dai[1].ignore_pmdown_time = 1;
	data->dai[1].init = &imx_ak7757_dai_init;
	data->dai[1].dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
			    SND_SOC_DAIFMT_CBM_CFM;
	data->dai[1].be_hw_params_fixup = be_hw_params_fixup;
	data->dai[1].ops  = &imx_ak7757_ops;
	
	data->card.dev = &pdev->dev;
	ret = snd_soc_of_parse_card_name(&data->card, "model");
		if (ret)
			goto fail;
	
	data->card.num_links = 2;
	data->card.owner = THIS_MODULE;
	data->card.dai_link = data->dai;
	data->card.dapm_widgets = imx_cs42888_dapm_widgets;
	data->card.num_dapm_widgets = ARRAY_SIZE(imx_cs42888_dapm_widgets);
	//data->card.dapm_routes = audio_map,
	//data->card.num_dapm_routes = ARRAY_SIZE(audio_map),

#if 1
	codec_clk = devm_clk_get(&codec_dev->dev, NULL);
	if (IS_ERR(codec_clk)) {
		ret = PTR_ERR(codec_clk);
		dev_err(&codec_dev->dev, "failed to get codec clk: %d\n", ret);
		goto fail;
	}
	priv->mclk_freq = clk_get_rate(codec_clk);
#endif

	ret = snd_soc_register_card(&data->card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n", ret);
		goto fail;
	}

	platform_set_drvdata(pdev, data);

fail:
	if (esai_np)
		of_node_put(esai_np);
	if (codec_np)
		of_node_put(codec_np);

	return ret;
}

static int imx_ak7757_remove(struct platform_device *pdev)
{
	struct imx_ak7757_data *data = platform_get_drvdata(pdev);

	snd_soc_unregister_card(&data->card);

	return 0;
}

static const struct of_device_id imx_ak7757_dt_ids[] = {
	{ .compatible = "fsl,imx-audio-ak7757-codec", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, imx_ak7757_dt_ids);

static struct platform_driver imx_ak7757_audio_driver = {
	.driver = {
		.name = "imx-ak7757",
		.owner = THIS_MODULE,
		.of_match_table = imx_ak7757_dt_ids,
	},
	.probe = imx_ak7757_probe,
	.remove = imx_ak7757_remove,
};
module_platform_driver(imx_ak7757_audio_driver);

/* Module information */
MODULE_AUTHOR("Ning Chen");
MODULE_DESCRIPTION("IMX BT CODEC Audio Driver");
MODULE_LICENSE("GPL");

