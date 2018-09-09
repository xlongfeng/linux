/*
 * LM48100Q ALSA SoC Audio driver
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
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/clk.h>
#include <linux/log2.h>
#include <linux/regmap.h>
#include <linux/of_device.h>
#include <sound/core.h>
#include <sound/tlv.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>

#include "lm48100q.h"

/* default value of lm48100q registers */
static const struct reg_default lm48100q_reg_defaults[] = {
	{ LM48100Q_MODE_CONTROL, 0x00},
	{ LM48100Q_DIAGNOSTIC_CONTROL, 0x00},
	{ LM48100Q_FAULT_DETECTION_CONTROL, 0x00},
	{ LM48100Q_VOLUME_1_CONTROL, 0x00},
	{ LM48100Q_VOLUME_2_CONTROL, 0x00},
};

/* lm48100q private structure in codec */
struct lm48100q_priv {
	int fmt;	/* i2s data format */
	struct regmap *regmap;
};

static int power_on_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec, LM48100Q_MODE_CONTROL,
			LM48100Q_POWER_ON_MASK, LM48100Q_POWER_ON);
		break;

	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec, LM48100Q_MODE_CONTROL,
			LM48100Q_POWER_ON_MASK, 0);
		break;
	default:
		break;
	}

	return 0;
}

static const struct snd_soc_dapm_widget lm48100q_dapm_widgets[] = {
	SND_SOC_DAPM_PRE("POWER_PRE", power_on_event),
	SND_SOC_DAPM_POST("POWER_POST", power_on_event),
	SND_SOC_DAPM_INPUT("IN1"),
	SND_SOC_DAPM_INPUT("IN2"),
	SND_SOC_DAPM_OUTPUT("OUT"),
};

/* routes for lm48100q */
static const struct snd_soc_dapm_route lm48100q_dapm_routes[] = {
	{"OUT", NULL, "IN1"},
	{"OUT", NULL, "IN2"},
};

/* tlv for hp volume, -80db to 18db, step 1.5db */
static const DECLARE_TLV_DB_SCALE(headphone_volume, -2850, 150, 0);

static const unsigned int volume_gain_tlv[] = {
	TLV_DB_RANGE_HEAD(6),
	0, 0, TLV_DB_SCALE_ITEM(-8000, 0, 0),
	1, 1, TLV_DB_SCALE_ITEM(-5400, 0, 0),
	2, 2, TLV_DB_SCALE_ITEM(-4050, 0, 0),
	3, 3, TLV_DB_SCALE_ITEM(-3450, 0, 0),
	4, 9, TLV_DB_SCALE_ITEM(-3000, 300, 0),
	10, 31, TLV_DB_SCALE_ITEM(-1350, 150, 0),
};

static const struct snd_kcontrol_new lm48100q_snd_controls[] = {
	SOC_DOUBLE_R_TLV("Master Playback Volume", LM48100Q_VOLUME_1_CONTROL,
			LM48100Q_VOLUME_2_CONTROL, 0, 31, 0, volume_gain_tlv),
	SOC_DOUBLE("Master Playback Switch", LM48100Q_MODE_CONTROL, 2, 3, 1, 0),
};

static struct snd_soc_dai_driver lm48100q_dai = {
	.name = "lm48100q",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 2,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_8000_192000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
	},
};

static int lm48100q_probe(struct snd_soc_codec *codec)
{
	/* struct lm48100q_priv *lm48100q = snd_soc_codec_get_drvdata(codec); */

	return 0;
}

static int lm48100q_remove(struct snd_soc_codec *codec)
{
	/* struct lm48100q_priv *lm48100q = snd_soc_codec_get_drvdata(codec); */

	return 0;
}

static struct snd_soc_codec_driver lm48100q_driver = {
	.probe = lm48100q_probe,
	.remove = lm48100q_remove,
	.component_driver = {
		.controls = lm48100q_snd_controls,
		.num_controls = ARRAY_SIZE(lm48100q_snd_controls),
		.dapm_widgets = lm48100q_dapm_widgets,
		.num_dapm_widgets = ARRAY_SIZE(lm48100q_dapm_widgets),
		.dapm_routes = lm48100q_dapm_routes,
		.num_dapm_routes = ARRAY_SIZE(lm48100q_dapm_routes),
	},
};

static const struct regmap_config lm48100q_regmap = {
	.reg_bits = 3,
	.reg_stride = 1,
	.val_bits = 5,

	.max_register = LM48100Q_MAX_REG_OFFSET,

	.cache_type = REGCACHE_RBTREE,
	.reg_defaults = lm48100q_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(lm48100q_reg_defaults),
};

/*
 * Write all the default values from lm48100q_reg_defaults[] array into the
 * lm48100q registers, to make sure we always start with the sane registers
 * values as stated in the datasheet.
 *
 * Since lm48100q does not have a reset line, nor a reset command in software,
 * we follow this approach to guarantee we always start from the default values
 * and avoid problems like, not being able to probe after an audio playback
 * followed by a system reset or a 'reboot' command in Linux
 */
static int lm48100q_fill_defaults(struct lm48100q_priv *lm48100q)
{
	int i, ret, val, index;

	for (i = 0; i < ARRAY_SIZE(lm48100q_reg_defaults); i++) {
		val = lm48100q_reg_defaults[i].def;
		index = lm48100q_reg_defaults[i].reg;
		ret = regmap_write(lm48100q->regmap, index, val);
		if (ret)
			return ret;
	}

	return 0;
}

static int lm48100q_i2c_probe(struct i2c_client *client,
			      const struct i2c_device_id *id)
{
	struct lm48100q_priv *lm48100q;
	int ret;

	lm48100q = devm_kzalloc(&client->dev, sizeof(*lm48100q), GFP_KERNEL);
	if (!lm48100q)
		return -ENOMEM;

	lm48100q->regmap = devm_regmap_init_i2c(client, &lm48100q_regmap);
	if (IS_ERR(lm48100q->regmap)) {
		ret = PTR_ERR(lm48100q->regmap);
		dev_err(&client->dev, "Failed to allocate regmap: %d\n", ret);
		return ret;
	}

	/* Need 8 clocks before I2C accesses */
	udelay(1);

	i2c_set_clientdata(client, lm48100q);

	/* Ensure lm48100q will start with sane register values */
	ret = lm48100q_fill_defaults(lm48100q);
	if (ret)
		return ret;

	ret = snd_soc_register_codec(&client->dev,
			&lm48100q_driver, &lm48100q_dai, 1);
	if (ret)
		return ret;

	dev_info(&client->dev, "lm48100q registered\n");

	return 0;
}

static int lm48100q_i2c_remove(struct i2c_client *client)
{
	/* struct lm48100q_priv *lm48100q = i2c_get_clientdata(client); */

	snd_soc_unregister_codec(&client->dev);
	return 0;
}

static const struct i2c_device_id lm48100q_id[] = {
	{"lm48100q", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, lm48100q_id);

static const struct of_device_id lm48100q_dt_ids[] = {
	{ .compatible = "fsl,lm48100q", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, lm48100q_dt_ids);

static struct i2c_driver lm48100q_i2c_driver = {
	.driver = {
		   .name = "lm48100q",
		   .owner = THIS_MODULE,
		   .of_match_table = lm48100q_dt_ids,
		   },
	.probe = lm48100q_i2c_probe,
	.remove = lm48100q_i2c_remove,
	.id_table = lm48100q_id,
};

module_i2c_driver(lm48100q_i2c_driver);

MODULE_DESCRIPTION("Freescale i.MX LM4100Q ALSA SoC Codec Driver");
MODULE_AUTHOR("xlongfeng <xlongfeng@126.com>");
MODULE_LICENSE("GPL");
