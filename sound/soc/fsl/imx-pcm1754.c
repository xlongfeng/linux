/*
 * PCM1754 LM48100Q ALSA SoC Audio driver
 *
 * Copyright (c) 2017, longfeng.xiao <xlongfeng@126.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/clk.h>
#include <sound/soc.h>

#include "imx-audmux.h"
#include "imx-ssi.h"

#define DAI_NAME_SIZE	32

struct imx_pcm1754_data {
	struct snd_soc_dai_link dai;
	struct snd_soc_card card;
	char codec_dai_name[DAI_NAME_SIZE];
	char platform_name[DAI_NAME_SIZE];
	struct clk *codec_clk;
	struct clk *codec_pll_clk;
	unsigned int pll_clk_streams;
	struct gpio_desc *dac_mute;
	int dac_mute_value;
	int hp_disabled;
	bool limit_16bit_samples;
};

static unsigned int sck_factors[] = {
	128, 192, 256, 384, 512, 768, 1152
};

static int do_mute(struct gpio_desc *gd, int mute)
{
	if (!gd)
		return 0;

	gpiod_set_value(gd, mute);
	return 0;
}

static int event_hp(struct snd_soc_dapm_widget *w,
		    struct snd_kcontrol *k, int event)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct snd_soc_card *card = dapm->card;
	struct imx_pcm1754_data *data = container_of(card,
			struct imx_pcm1754_data, card);
	int mute = SND_SOC_DAPM_EVENT_ON(event) ? 0 : 1;

	data->hp_disabled = data->dac_mute_value = mute;
	return do_mute(data->dac_mute, mute);
}

static const struct snd_soc_dapm_widget imx_pcm1754_dapm_widgets[] = {
	SND_SOC_DAPM_HP("Ext Spk", event_hp),
};

static int imx_pcm1475_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct imx_pcm1754_data *data = snd_soc_card_get_drvdata(rtd->card);

	if (data->limit_16bit_samples) {
		snd_pcm_hw_constraint_minmax(substream->runtime,
			SNDRV_PCM_HW_PARAM_SAMPLE_BITS, 16, 16);
	}

	return 0;
}

static int imx_pcm1475_hw_params(struct snd_pcm_substream *substream,
				     struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct device *dev = card->dev;
	struct imx_pcm1754_data *data = snd_soc_card_get_drvdata(rtd->card);
	unsigned int rate = params_rate(params);
	int factors_cnt = ARRAY_SIZE(sck_factors);
	unsigned long freq;
	int ret;
	int i;

	for (i = 0; i < factors_cnt; i++) {
		freq = rate * sck_factors[i];
		if (freq > 8000000 && freq < 16000000) {
			break;
		}
	}

	if (i == factors_cnt) {
		dev_err(dev, "failed to handle the required sck\n");
		return -EINVAL;
	}

	/* codec_pll_clk = codec_clk * 8 (fixed) */
	freq *= 8;

	ret = clk_set_rate(data->codec_pll_clk, clk_round_rate(data->codec_pll_clk, freq));
	if (ret) {
		dev_err(dev, "failed to set pll rate\n");
		return -EINVAL;
	}

	ret = clk_prepare_enable(data->codec_clk);
	if (ret) {
		dev_err(dev, "clk_prepare_enable failed: %d\n", ret);
		return ret;
	}

	data->pll_clk_streams |= BIT(substream->stream);

	return 0;
}

static int imx_pcm1475_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct imx_pcm1754_data *data = snd_soc_card_get_drvdata(rtd->card);

	if (data->pll_clk_streams & BIT(substream->stream)) {
		clk_disable_unprepare(data->codec_clk);
		data->pll_clk_streams &= ~BIT(substream->stream);
	}

	return 0;
}

static struct snd_soc_ops imx_pcm1475_ops = {
	.startup	= imx_pcm1475_startup,
	.hw_params	= imx_pcm1475_hw_params,
	.hw_free	= imx_pcm1475_hw_free,
};

static int dac_mute_set(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card =  snd_kcontrol_chip(kcontrol);
	struct imx_pcm1754_data *data = container_of(card,
			struct imx_pcm1754_data, card);
	int value = ucontrol->value.integer.value[0];

	if (value > 1)
		return -EINVAL;
	value = (value ^ 1 ) | data->hp_disabled;
	data->dac_mute_value = value;
	if (data->dac_mute)
		gpiod_set_value(data->dac_mute, value);
	return 0;
}

static int dac_mute_get(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card =  snd_kcontrol_chip(kcontrol);
	struct imx_pcm1754_data *data = container_of(card,
			struct imx_pcm1754_data, card);

	ucontrol->value.integer.value[0] = data->dac_mute_value ^ 1;
	return 0;
}

static const struct snd_kcontrol_new more_controls[] = {
	SOC_SINGLE_EXT("DAC Mute Switch", 0, 0, 1, 0, dac_mute_get, dac_mute_set),
};

static int imx_pcm1754_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct device_node *ssi_np, *codec_np;
	struct platform_device *ssi_pdev;
	struct i2c_client *codec_dev;
	struct imx_pcm1754_data *data = NULL;
	struct gpio_desc *gd = NULL;
	int int_port, ext_port;
	const struct snd_kcontrol_new *kcontrols = more_controls;
	int kcontrols_cnt = ARRAY_SIZE(more_controls);
	int ret;

	ret = of_property_read_u32(np, "mux-int-port", &int_port);
	if (ret) {
		dev_err(&pdev->dev, "mux-int-port missing or invalid\n");
		return ret;
	}
	ret = of_property_read_u32(np, "mux-ext-port", &ext_port);
	if (ret) {
		dev_err(&pdev->dev, "mux-ext-port missing or invalid\n");
		return ret;
	}

	/*
	 * The port numbering in the hardware manual starts at 1, while
	 * the audmux API expects it starts at 0.
	 */
	int_port--;
	ext_port--;
	ret = imx_audmux_v2_configure_port(int_port,
			IMX_AUDMUX_V2_PTCR_SYN,
			IMX_AUDMUX_V2_PDCR_RXDSEL(ext_port));
	if (ret) {
		dev_err(&pdev->dev, "audmux internal port setup failed\n");
		return ret;
	}
	ret = imx_audmux_v2_configure_port(ext_port,
			IMX_AUDMUX_V2_PTCR_SYN |
			IMX_AUDMUX_V2_PTCR_TFSEL(int_port) |
			IMX_AUDMUX_V2_PTCR_TCSEL(int_port) |
			IMX_AUDMUX_V2_PTCR_TFSDIR |
			IMX_AUDMUX_V2_PTCR_TCLKDIR,
			IMX_AUDMUX_V2_PDCR_RXDSEL(int_port));
	if (ret) {
		dev_err(&pdev->dev, "audmux external port setup failed\n");
		return ret;
	}

	ssi_np = of_parse_phandle(pdev->dev.of_node, "ssi-controller", 0);
	codec_np = of_parse_phandle(pdev->dev.of_node, "audio-codec", 0);
	if (!ssi_np || !codec_np) {
		dev_err(&pdev->dev, "phandle missing or invalid\n");
		ret = -EINVAL;
		goto fail;
	}

	ssi_pdev = of_find_device_by_node(ssi_np);
	if (!ssi_pdev) {
		dev_err(&pdev->dev, "failed to find SSI platform device\n");
		ret = -EPROBE_DEFER;
		goto fail;
	}
	codec_dev = of_find_i2c_device_by_node(codec_np);
	if (!codec_dev) {
		dev_err(&pdev->dev, "failed to find codec platform device\n");
		return -EPROBE_DEFER;
	}

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data) {
		ret = -ENOMEM;
		goto fail;
	}

	data->codec_clk = clk_get(&codec_dev->dev, NULL);
	if (IS_ERR(data->codec_clk)) {
		ret = PTR_ERR(data->codec_clk);
		goto fail;
	}

	data->codec_pll_clk = clk_get(&codec_dev->dev, "pll");
	if (IS_ERR(data->codec_pll_clk)) {
		ret = PTR_ERR(data->codec_pll_clk);
		goto fail;
	}

	data->dai.name = "HiFi";
	data->dai.stream_name = "HiFi";
	data->dai.codec_dai_name = "lm48100q";
	data->dai.codec_of_node = codec_np;
	data->dai.cpu_of_node = ssi_np;
	data->dai.platform_of_node = ssi_np;
	data->dai.ops = &imx_pcm1475_ops;
	data->dai.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
			    SND_SOC_DAIFMT_CBS_CFS;

	data->limit_16bit_samples = of_property_read_bool(pdev->dev.of_node, "limit-to-16-bit-samples");
	gd = devm_gpiod_get_index(&pdev->dev, "dac-mute", 0, GPIOD_OUT_HIGH);
	if (!IS_ERR(gd)) {
		data->dac_mute = gd;
	}

	data->card.dev = &pdev->dev;
	ret = snd_soc_of_parse_card_name(&data->card, "model");
	if (ret)
		goto fail;
	ret = snd_soc_of_parse_audio_routing(&data->card, "audio-routing");
	if (ret)
		goto fail;
	data->card.num_links = 1;
	data->card.owner = THIS_MODULE;
	if (!data->dac_mute)
		kcontrols_cnt--;
	if (kcontrols_cnt) {
		data->card.controls	 = kcontrols;
		data->card.num_controls  = kcontrols_cnt;
	}
	data->card.dai_link = &data->dai;
	data->card.dapm_widgets = imx_pcm1754_dapm_widgets;
	data->card.num_dapm_widgets = ARRAY_SIZE(imx_pcm1754_dapm_widgets);

	platform_set_drvdata(pdev, &data->card);
	snd_soc_card_set_drvdata(&data->card, data);

	ret = devm_snd_soc_register_card(&pdev->dev, &data->card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n", ret);
		goto fail;
	}

	of_node_put(ssi_np);
	of_node_put(codec_np);

	return 0;

fail:
	if (data && !IS_ERR(data->codec_pll_clk))
		clk_put(data->codec_pll_clk);

	if (data && !IS_ERR(data->codec_clk))
		clk_put(data->codec_clk);
	of_node_put(ssi_np);
	of_node_put(codec_np);

	return ret;
}

static int imx_pcm1754_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct imx_pcm1754_data *data = snd_soc_card_get_drvdata(card);

	if (data->dac_mute)
		gpiod_set_value(data->dac_mute, 1);
	clk_put(data->codec_pll_clk);
	clk_put(data->codec_clk);

	return 0;
}

static const struct of_device_id imx_pcm1754_dt_ids[] = {
	{ .compatible = "fsl,imx-audio-pcm1754", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, imx_pcm1754_dt_ids);

static struct platform_driver imx_pcm1754_driver = {
	.driver = {
		.name = "imx-pcm1754",
		.pm = &snd_soc_pm_ops,
		.of_match_table = imx_pcm1754_dt_ids,
	},
	.probe = imx_pcm1754_probe,
	.remove = imx_pcm1754_remove,
};
module_platform_driver(imx_pcm1754_driver);

MODULE_AUTHOR("xlongfeng <xlongfeng@126.com>");
MODULE_DESCRIPTION("Freescale i.MX PCM1754 ASoC machine driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:imx-pcm1754");
