// SPDX-License-Identifier: GPL-2.0-only
/*
 * Driver for AR0330 CMOS Image Sensor from Aptina
 *
 * Copyright (C) 2011, Laurent Pinchart <laurent.pinchart@ideasonboard.com>
 * Copyright (C) 2011, Javier Martin <javier.martin@vista-silicon.com>
 * Copyright (C) 2011, Guennadi Liakhovetski <g.liakhovetski@gmx.de>
 *
 * Based on the MT9V032 driver and Bastian Hecht's code.
 */

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/log2.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/videodev2.h>

/* for smiapp-pll */
#include <linux/gcd.h>
#include <linux/lcm.h>

#include <media/v4l2-async.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>

#include "smiapp-pll.h"

#define AR0330_PIXEL_ARRAY_WIDTH		2304U
#define AR0330_PIXEL_ARRAY_HEIGHT		1536U

#define AR0330_WINDOW_WIDTH_MIN			32U
#define AR0330_WINDOW_WIDTH_DEF			2304U
#define AR0330_WINDOW_WIDTH_MAX			AR0330_PIXEL_ARRAY_WIDTH
#define AR0330_WINDOW_HEIGHT_MIN		32U
#define AR0330_WINDOW_HEIGHT_DEF		1296U
#define AR0330_WINDOW_HEIGHT_MAX		AR0330_PIXEL_ARRAY_HEIGHT

#define AR0330_CHIP_VERSION				0x3000
#define		AR0330_CHIP_VERSION_VALUE		0x2604
#define AR0330_Y_ADDR_START				0x3002
#define AR0330_X_ADDR_START				0x3004
#define AR0330_Y_ADDR_END				0x3006
#define AR0330_X_ADDR_END				0x3008
#define AR0330_FRAME_LENGTH_LINES			0x300a
#define AR0330_LINE_LENGTH_PCK				0x300c
#define AR0330_CHIP_REVISION				0x300e
#define		AR0330_CHIP_REVISION_1x			0x10
#define		AR0330_CHIP_REVISION_2x			0x20
#define AR0330_LOCK_CONTROL				0x3010
#define		AR0330_LOCK_CONTROL_UNLOCK		0xbeef
#define AR0330_COARSE_INTEGRATION_TIME			0x3012
#define		AR0330_COARSE_INTEGRATION_TIME_MIN	1U
#define		AR0330_COARSE_INTEGRATION_TIME_DEF	16U
#define		AR0330_COARSE_INTEGRATION_TIME_MAX	65535U
#define AR0330_RESET					0x301a
#define		AR0330_RESET_SMIA_DIS			(1 << 12)
#define		AR0330_RESET_FORCE_PLL_ON		(1 << 11)
#define		AR0330_RESET_RESTART_BAD		(1 << 10)
#define		AR0330_RESET_MASK_BAD			(1 << 9)
#define		AR0330_RESET_GPI_EN			(1 << 8)
#define		AR0330_RESET_PARALLEL_EN		(1 << 7)
#define		AR0330_RESET_DRIVE_PINS			(1 << 6)
#define		AR0330_RESET_LOCK_REG			(1 << 3)
#define		AR0330_RESET_STREAM			(1 << 2)
#define		AR0330_RESET_RESTART			(1 << 1)
#define		AR0330_RESET_RESET			(1 << 0)
/* AR03303_MODE_SELECT is an alias for AR0330_RESET_STREAM */
#define AR0330_MODE_SELECT				0x301c
#define		AR0330_MODE_SELECT_STREAM		(1 << 0)
#define AR0330_VT_PIX_CLK_DIV				0x302a
#define AR0330_VT_SYS_CLK_DIV				0x302c
#define AR0330_PRE_PLL_CLK_DIV				0x302e
#define AR0330_PLL_MULTIPLIER				0x3030
#define AR0330_OP_PIX_CLK_DIV				0x3036
#define AR0330_OP_SYS_CLK_DIV				0x3038
#define AR0330_FRAME_COUNT				0x303a
#define AR0330_READ_MODE				0x3040
#define		AR0330_READ_MODE_VERT_FLIP		(1 << 15)
#define		AR0330_READ_MODE_HORIZ_MIRROR		(1 << 14)
#define		AR0330_READ_MODE_COL_BIN		(1 << 13)
#define		AR0330_READ_MODE_ROW_BIN		(1 << 12)
#define		AR0330_READ_MODE_COL_SF_BIN		(1 << 9)
#define		AR0330_READ_MODE_COL_SUM		(1 << 5)
#define AR0330_GREEN1_GAIN				0x3056
#define AR0330_BLUE_GAIN				0x3058
#define AR0330_RED_GAIN					0x305a
#define AR0330_GREEN2_GAIN				0x305c
#define AR0330_GLOBAL_GAIN				0x305e
#define		AR0330_GLOBAL_GAIN_MIN			128U
#define		AR0330_GLOBAL_GAIN_DEF			128U
#define		AR0330_GLOBAL_GAIN_MAX			2047U
#define AR0330_ANALOG_GAIN				0x3060
#define AR0330_DATAPATH_SELECT				0x306e
#define		AR0330_DATAPATH_SLEW_DOUT_MASK		(7 << 13)
#define		AR0330_DATAPATH_SLEW_DOUT_SHIFT		13
#define		AR0330_DATAPATH_SLEW_PCLK_MASK		(7 << 10)
#define		AR0330_DATAPATH_SLEW_PCLK_SHIFT		10
#define		AR0330_DATAPATH_HIGH_VCM		(1 << 9)
#define AR0330_TEST_PATTERN_MODE			0x3070
#define AR0330_TEST_DATA_RED				0x3072
#define AR0330_TEST_DATA_GREENR				0x3074
#define AR0330_TEST_DATA_BLUE				0x3076
#define AR0330_TEST_DATA_GREENB				0x3078
#define AR0330_SEQ_DATA_PORT				0x3086
#define AR0330_SEQ_CTRL_PORT				0x3088
#define AR0330_X_ODD_INC				0x30a2
#define AR0330_Y_ODD_INC				0x30a6
#define AR0330_X_ODD_INC				0x30a2
#define AR0330_DATA_FORMAT_BITS				0x31ac
#define AR0330_SERIAL_FORMAT				0x31ae
#define AR0330_FRAME_PREAMBLE				0x31b0
#define AR0330_LINE_PREAMBLE				0x31b2
#define AR0330_MIPI_TIMING_0				0x31b4
#define AR0330_MIPI_TIMING_1				0x31b6
#define AR0330_MIPI_TIMING_2				0x31b8
#define AR0330_MIPI_TIMING_3				0x31ba
#define AR0330_MIPI_TIMING_4				0x31bc
#define AR0330_MIPI_CONFIG_STATUS			0x31be
#define AR0330_COMPRESSION				0x31d0
#define		AR0330_COMPRESSION_ENABLE		(1 << 0)
#define AR0330_POLY_SC					0x3780
#define		AR0330_POLY_SC_ENABLE			(1 << 15)

struct ar0330 {
	struct i2c_client *client;
	struct device *dev;

	struct smiapp_pll pll;
	unsigned int version;

	struct gpio_desc *reset;
	struct regulator_bulk_data supplies[3];

	struct v4l2_subdev subdev;
	struct media_pad pad;

	struct v4l2_rect crop;  /* Sensor window */
	struct v4l2_mbus_framefmt format;

	struct v4l2_ctrl_handler ctrls;
	struct v4l2_ctrl *flip[2];
	struct v4l2_ctrl *pixel_rate;

	bool streaming;

	/* Registers cache */
	u16 read_mode;
};

static struct ar0330 *to_ar0330(struct v4l2_subdev *sd)
{
	return container_of(sd, struct ar0330, subdev);
}

static const char * const ar0330_supply_names[3] = {
	"vddpll",
	"vaa",
	"vddio",
};

/* -----------------------------------------------------------------------------
 * Register access
 */

static int __ar0330_read(struct ar0330 *ar0330, u16 reg, size_t size)
{
	struct i2c_client *client = ar0330->client;
	u8 data[2];
	struct i2c_msg msg[2] = {
		{ client->addr, 0, 2, data },
		{ client->addr, I2C_M_RD, size, data },
	};
	int ret;

	data[0] = reg >> 8;
	data[1] = reg & 0xff;

	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret < 0) {
		dev_err(ar0330->dev, "%s(0x%04x) failed (%d)\n", __func__,
			reg, ret);
		return ret;
	}

	if (size == 2)
		return (data[0] << 8) | data[1];
	else
		return data[0];
}

static int __ar0330_write(struct ar0330 *ar0330, u16 reg, u16 value,
			  size_t size, int *err)
{
	struct i2c_client *client = ar0330->client;
	u8 data[4];
	struct i2c_msg msg = { client->addr, 0, 2 + size, data };
	int ret;

	if (err && *err)
		return *err;

	dev_dbg(ar0330->dev, "writing 0x%04x to 0x%04x\n", value, reg);

	data[0] = reg >> 8;
	data[1] = reg & 0xff;
	if (size == 2) {
		data[2] = value >> 8;
		data[3] = value & 0xff;
	} else {
		data[2] = value & 0xff;
	}

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret < 0) {
		dev_err(ar0330->dev, "%s(0x%04x) failed (%d)\n", __func__,
			reg, ret);
		if (err)
			*err = ret;
		return ret;
	}

	return 0;
}

static inline int ar0330_read8(struct ar0330 *ar0330, u16 reg)
{
	return __ar0330_read(ar0330, reg, 1);
}

static inline int ar0330_write8(struct ar0330 *ar0330, u16 reg, u8 value)
{
	return __ar0330_write(ar0330, reg, value, 1, NULL);
}

static inline int ar0330_read16(struct ar0330 *ar0330, u16 reg)
{
	return __ar0330_read(ar0330, reg, 2);
}

static inline int ar0330_write16(struct ar0330 *ar0330, u16 reg, u16 value,
				 int *err)
{
	return __ar0330_write(ar0330, reg, value, 2, err);
}

static int ar0330_set_read_mode(struct ar0330 *ar0330, u16 clear, u16 set)
{
	u16 value = (ar0330->read_mode & ~clear) | set;
	int ret;

	ret = ar0330_write16(ar0330, AR0330_READ_MODE, value, NULL);
	if (ret < 0)
		return ret;

	ar0330->read_mode = value;
	return 0;
}

/* -----------------------------------------------------------------------------
 * PLL configuration
 */

static int ar0330_pll_configure(struct ar0330 *ar0330)
{
	int ret = 0;

	ar0330_write16(ar0330, AR0330_VT_PIX_CLK_DIV,
		       ar0330->pll.vt.pix_clk_div, &ret);
	ar0330_write16(ar0330, AR0330_VT_SYS_CLK_DIV,
		       ar0330->pll.vt.sys_clk_div, &ret);
	ar0330_write16(ar0330, AR0330_PRE_PLL_CLK_DIV,
		       ar0330->pll.pre_pll_clk_div, &ret);
	ar0330_write16(ar0330, AR0330_PLL_MULTIPLIER,
		       ar0330->pll.pll_multiplier, &ret);
	ar0330_write16(ar0330, AR0330_OP_PIX_CLK_DIV,
		       ar0330->pll.op.pix_clk_div, &ret);
	ar0330_write16(ar0330, AR0330_OP_SYS_CLK_DIV,
			ar0330->pll.op.sys_clk_div, &ret);
	if (ret < 0)
		return ret;

	usleep_range(1000, 2000);

	return 0;
}

/* Return an even number or one. */
static inline uint32_t clk_div_even(uint32_t a)
{
	return max_t(uint32_t, 1, a & ~1);
}

/* Return an even number or one. */
static inline uint32_t clk_div_even_up(uint32_t a)
{
	if (a == 1)
		return 1;
	return (a + 1) & ~1;
}

static inline uint32_t is_one_or_even(uint32_t a)
{
	if (a == 1)
		return 1;
	if (a & 1)
		return 0;

	return 1;
}

static int bounds_check(struct device *dev, uint32_t val,
			uint32_t min, uint32_t max, char *str)
{
	if (val >= min && val <= max)
		return 0;

	dev_dbg(dev, "%s out of bounds: %d (%d--%d)\n", str, val, min, max);

	return -EINVAL;
}

static void print_pll(struct device *dev, struct smiapp_pll *pll)
{
	dev_dbg(dev, "pre_pll_clk_div\t%u\n",  pll->pre_pll_clk_div);
	dev_dbg(dev, "pll_multiplier \t%u\n",  pll->pll_multiplier);
	if (!(pll->flags & SMIAPP_PLL_FLAG_NO_OP_CLOCKS)) {
		dev_dbg(dev, "op_sys_clk_div \t%u\n", pll->op.sys_clk_div);
		dev_dbg(dev, "op_pix_clk_div \t%u\n", pll->op.pix_clk_div);
	}
	dev_dbg(dev, "vt_sys_clk_div \t%u\n",  pll->vt.sys_clk_div);
	dev_dbg(dev, "vt_pix_clk_div \t%u\n",  pll->vt.pix_clk_div);

	dev_dbg(dev, "ext_clk_freq_hz \t%u\n", pll->ext_clk_freq_hz);
	dev_dbg(dev, "pll_ip_clk_freq_hz \t%u\n", pll->pll_ip_clk_freq_hz);
	dev_dbg(dev, "pll_op_clk_freq_hz \t%u\n", pll->pll_op_clk_freq_hz);
	if (!(pll->flags & SMIAPP_PLL_FLAG_NO_OP_CLOCKS)) {
		dev_dbg(dev, "op_sys_clk_freq_hz \t%u\n",
			pll->op.sys_clk_freq_hz);
		dev_dbg(dev, "op_pix_clk_freq_hz \t%u\n",
			pll->op.pix_clk_freq_hz);
	}
	dev_dbg(dev, "vt_sys_clk_freq_hz \t%u\n", pll->vt.sys_clk_freq_hz);
	dev_dbg(dev, "vt_pix_clk_freq_hz \t%u\n", pll->vt.pix_clk_freq_hz);
}

static int check_all_bounds(struct device *dev,
			    const struct smiapp_pll_limits *limits,
			    const struct smiapp_pll_branch_limits *op_limits,
			    struct smiapp_pll *pll,
			    struct smiapp_pll_branch *op_pll)
{
	int rval;

	rval = bounds_check(dev, pll->pll_ip_clk_freq_hz,
			    limits->min_pll_ip_freq_hz,
			    limits->max_pll_ip_freq_hz,
			    "pll_ip_clk_freq_hz");
	if (!rval)
		rval = bounds_check(
			dev, pll->pll_multiplier,
			limits->min_pll_multiplier, limits->max_pll_multiplier,
			"pll_multiplier");
	if (!rval)
		rval = bounds_check(
			dev, pll->pll_op_clk_freq_hz,
			limits->min_pll_op_freq_hz, limits->max_pll_op_freq_hz,
			"pll_op_clk_freq_hz");
	if (!rval)
		rval = bounds_check(
			dev, op_pll->sys_clk_div,
			op_limits->min_sys_clk_div, op_limits->max_sys_clk_div,
			"op_sys_clk_div");
	if (!rval)
		rval = bounds_check(
			dev, op_pll->sys_clk_freq_hz,
			op_limits->min_sys_clk_freq_hz,
			op_limits->max_sys_clk_freq_hz,
			"op_sys_clk_freq_hz");
	if (!rval)
		rval = bounds_check(
			dev, op_pll->pix_clk_freq_hz,
			op_limits->min_pix_clk_freq_hz,
			op_limits->max_pix_clk_freq_hz,
			"op_pix_clk_freq_hz");

	/*
	 * If there are no OP clocks, the VT clocks are contained in
	 * the OP clock struct.
	 */
	if (pll->flags & SMIAPP_PLL_FLAG_NO_OP_CLOCKS)
		return rval;

	if (!rval)
		rval = bounds_check(
			dev, pll->vt.sys_clk_freq_hz,
			limits->vt.min_sys_clk_freq_hz,
			limits->vt.max_sys_clk_freq_hz,
			"vt_sys_clk_freq_hz");
	if (!rval)
		rval = bounds_check(
			dev, pll->vt.pix_clk_freq_hz,
			limits->vt.min_pix_clk_freq_hz,
			limits->vt.max_pix_clk_freq_hz,
			"vt_pix_clk_freq_hz");

	return rval;
}

/*
 * Heuristically guess the PLL tree for a given common multiplier and
 * divisor. Begin with the operational timing and continue to video
 * timing once operational timing has been verified.
 *
 * @mul is the PLL multiplier and @div is the common divisor
 * (pre_pll_clk_div and op_sys_clk_div combined). The final PLL
 * multiplier will be a multiple of @mul.
 *
 * @return Zero on success, error code on error.
 */
static int __smiapp_pll_calculate(
	struct device *dev, const struct smiapp_pll_limits *limits,
	const struct smiapp_pll_branch_limits *op_limits,
	struct smiapp_pll *pll, struct smiapp_pll_branch *op_pll, uint32_t mul,
	uint32_t div, uint32_t lane_op_clock_ratio)
{
	unsigned int num_readout_paths;
	uint32_t sys_div;
	uint32_t best_pix_div = INT_MAX >> 1;
	uint32_t vt_op_binning_div;
	/*
	 * Higher multipliers (and divisors) are often required than
	 * necessitated by the external clock and the output clocks.
	 * There are limits for all values in the clock tree. These
	 * are the minimum and maximum multiplier for mul.
	 */
	uint32_t more_mul_min, more_mul_max;
	uint32_t more_mul_factor;
	uint32_t min_vt_div, max_vt_div, vt_div;
	uint32_t min_sys_div, max_sys_div;
	unsigned int i;

	/*
	 * Get pre_pll_clk_div so that our pll_op_clk_freq_hz won't be
	 * too high.
	 */
	dev_dbg(dev, "pre_pll_clk_div %u\n", pll->pre_pll_clk_div);

	/* Don't go above max pll multiplier. */
	more_mul_max = limits->max_pll_multiplier / mul;
	dev_dbg(dev, "more_mul_max: max_pll_multiplier check: %u\n",
		more_mul_max);
	/* Don't go above max pll op frequency. */
	more_mul_max =
		min_t(uint32_t,
		      more_mul_max,
		      limits->max_pll_op_freq_hz
		      / (pll->ext_clk_freq_hz / pll->pre_pll_clk_div * mul));
	dev_dbg(dev, "more_mul_max: max_pll_op_freq_hz check: %u\n",
		more_mul_max);
	/* Don't go above the division capability of op sys clock divider. */
	more_mul_max = min(more_mul_max,
			   limits->op.max_sys_clk_div * pll->pre_pll_clk_div
			   / div);
	dev_dbg(dev, "more_mul_max: max_op_sys_clk_div check: %u\n",
		more_mul_max);
	/* Ensure we won't go above min_pll_multiplier. */
	more_mul_max = min(more_mul_max,
			   DIV_ROUND_UP(limits->max_pll_multiplier, mul));
	dev_dbg(dev, "more_mul_max: min_pll_multiplier check: %u\n",
		more_mul_max);

	/* Ensure we won't go below min_pll_op_freq_hz. */
	more_mul_min = DIV_ROUND_UP(limits->min_pll_op_freq_hz,
				    pll->ext_clk_freq_hz / pll->pre_pll_clk_div
				    * mul);
	dev_dbg(dev, "more_mul_min: min_pll_op_freq_hz check: %u\n",
		more_mul_min);
	/* Ensure we won't go below min_pll_multiplier. */
	more_mul_min = max(more_mul_min,
			   DIV_ROUND_UP(limits->min_pll_multiplier, mul));
	dev_dbg(dev, "more_mul_min: min_pll_multiplier check: %u\n",
		more_mul_min);

	if (more_mul_min > more_mul_max) {
		dev_dbg(dev,
			"unable to compute more_mul_min and more_mul_max\n");
		return -EINVAL;
	}

	more_mul_factor = lcm(div, pll->pre_pll_clk_div) / div;
	dev_dbg(dev, "more_mul_factor: %u\n", more_mul_factor);
	more_mul_factor = lcm(more_mul_factor, op_limits->min_sys_clk_div);
	dev_dbg(dev, "more_mul_factor: min_op_sys_clk_div: %d\n",
		more_mul_factor);
	i = roundup(more_mul_min, more_mul_factor);
	if (!is_one_or_even(i))
		i <<= 1;

	dev_dbg(dev, "final more_mul: %u\n", i);
	if (i > more_mul_max) {
		dev_dbg(dev, "final more_mul is bad, max %u\n", more_mul_max);
		return -EINVAL;
	}

	pll->pll_multiplier = mul * i;
	op_pll->sys_clk_div = div * i / pll->pre_pll_clk_div;
	dev_dbg(dev, "op_sys_clk_div: %u\n", op_pll->sys_clk_div);

	pll->pll_ip_clk_freq_hz = pll->ext_clk_freq_hz
		/ pll->pre_pll_clk_div;

	pll->pll_op_clk_freq_hz = pll->pll_ip_clk_freq_hz
		* pll->pll_multiplier;

	/* Derive pll_op_clk_freq_hz. */
	op_pll->sys_clk_freq_hz =
		pll->pll_op_clk_freq_hz / op_pll->sys_clk_div;

	op_pll->pix_clk_div = pll->bits_per_pixel;
	dev_dbg(dev, "op_pix_clk_div: %u\n", op_pll->pix_clk_div);

	op_pll->pix_clk_freq_hz =
		op_pll->sys_clk_freq_hz / op_pll->pix_clk_div;

	if (pll->flags & SMIAPP_PLL_FLAG_NO_OP_CLOCKS) {
		/* No OP clocks --- VT clocks are used instead. */
		goto out_skip_vt_calc;
	}

	/*
	 * Some sensors perform analogue binning and some do this
	 * digitally. The ones doing this digitally can be roughly be
	 * found out using this formula. The ones doing this digitally
	 * should run at higher clock rate, so smaller divisor is used
	 * on video timing side.
	 */
	if (limits->min_line_length_pck_bin > limits->min_line_length_pck
	    / pll->binning_horizontal)
		vt_op_binning_div = pll->binning_horizontal;
	else
		vt_op_binning_div = 1;
	dev_dbg(dev, "vt_op_binning_div: %u\n", vt_op_binning_div);

	/*
	 * Profile 2 supports vt_pix_clk_div E [4, 10]
	 *
	 * Horizontal binning can be used as a base for difference in
	 * divisors. One must make sure that horizontal blanking is
	 * enough to accommodate the CSI-2 sync codes.
	 *
	 * Take scaling factor into account as well.
	 *
	 * Find absolute limits for the factor of vt divider.
	 */
	dev_dbg(dev, "scale_m: %u\n", pll->scale_m);
	if (pll->flags & SMIAPP_PLL_FLAG_DUAL_READOUT)
		num_readout_paths = 2;
	else
		num_readout_paths = 1;
	min_vt_div = DIV_ROUND_UP(op_pll->pix_clk_div * op_pll->sys_clk_div
				  * pll->scale_n * num_readout_paths,
				  lane_op_clock_ratio * vt_op_binning_div
				  * pll->scale_m);

	/* Find smallest and biggest allowed vt divisor. */
	dev_dbg(dev, "min_vt_div: %u\n", min_vt_div);
	min_vt_div = max(min_vt_div,
			 DIV_ROUND_UP(pll->pll_op_clk_freq_hz,
				      limits->vt.max_pix_clk_freq_hz));
	dev_dbg(dev, "min_vt_div: max_vt_pix_clk_freq_hz: %u\n",
		min_vt_div);
	min_vt_div = max_t(uint32_t, min_vt_div,
			   limits->vt.min_pix_clk_div
			   * limits->vt.min_sys_clk_div);
	dev_dbg(dev, "min_vt_div: min_vt_clk_div: %u\n", min_vt_div);

	max_vt_div = limits->vt.max_sys_clk_div * limits->vt.max_pix_clk_div;
	dev_dbg(dev, "max_vt_div: %u\n", max_vt_div);
	max_vt_div = min(max_vt_div,
			 DIV_ROUND_UP(pll->pll_op_clk_freq_hz,
				      limits->vt.min_pix_clk_freq_hz));
	dev_dbg(dev, "max_vt_div: min_vt_pix_clk_freq_hz: %u\n",
		max_vt_div);

	/*
	 * Find limitsits for sys_clk_div. Not all values are possible
	 * with all values of pix_clk_div.
	 */
	min_sys_div = limits->vt.min_sys_clk_div;
	dev_dbg(dev, "min_sys_div: %u\n", min_sys_div);
	min_sys_div = max(min_sys_div,
			  DIV_ROUND_UP(min_vt_div,
				       limits->vt.max_pix_clk_div));
	dev_dbg(dev, "min_sys_div: max_vt_pix_clk_div: %u\n", min_sys_div);
	min_sys_div = max(min_sys_div,
			  pll->pll_op_clk_freq_hz
			  / limits->vt.max_sys_clk_freq_hz);
	dev_dbg(dev, "min_sys_div: max_pll_op_clk_freq_hz: %u\n", min_sys_div);
	min_sys_div = clk_div_even_up(min_sys_div);
	dev_dbg(dev, "min_sys_div: one or even: %u\n", min_sys_div);

	max_sys_div = limits->vt.max_sys_clk_div;
	dev_dbg(dev, "max_sys_div: %u\n", max_sys_div);
	max_sys_div = min(max_sys_div,
			  DIV_ROUND_UP(max_vt_div,
				       limits->vt.min_pix_clk_div));
	dev_dbg(dev, "max_sys_div: min_vt_pix_clk_div: %u\n", max_sys_div);
	max_sys_div = min(max_sys_div,
			  DIV_ROUND_UP(pll->pll_op_clk_freq_hz,
				       limits->vt.min_pix_clk_freq_hz));
	dev_dbg(dev, "max_sys_div: min_vt_pix_clk_freq_hz: %u\n", max_sys_div);

	/*
	 * Find pix_div such that a legal pix_div * sys_div results
	 * into a value which is not smaller than div, the desired
	 * divisor.
	 */
	for (vt_div = min_vt_div; vt_div <= max_vt_div;
	     vt_div += 2 - (vt_div & 1)) {
		for (sys_div = min_sys_div;
		     sys_div <= max_sys_div;
		     sys_div += 2 - (sys_div & 1)) {
			uint16_t pix_div = DIV_ROUND_UP(vt_div, sys_div);

			if (pix_div < limits->vt.min_pix_clk_div
			    || pix_div > limits->vt.max_pix_clk_div) {
				dev_dbg(dev,
					"pix_div %u too small or too big (%u--%u)\n",
					pix_div,
					limits->vt.min_pix_clk_div,
					limits->vt.max_pix_clk_div);
				continue;
			}

			/* Check if this one is better. */
			if (pix_div * sys_div
			    <= roundup(min_vt_div, best_pix_div))
				best_pix_div = pix_div;
		}
		if (best_pix_div < INT_MAX >> 1)
			break;
	}

	pll->vt.sys_clk_div = DIV_ROUND_UP(min_vt_div, best_pix_div);
	pll->vt.pix_clk_div = best_pix_div;

	pll->vt.sys_clk_freq_hz =
		pll->pll_op_clk_freq_hz / pll->vt.sys_clk_div;
	pll->vt.pix_clk_freq_hz =
		pll->vt.sys_clk_freq_hz / pll->vt.pix_clk_div;

out_skip_vt_calc:
	pll->pixel_rate_csi =
		op_pll->pix_clk_freq_hz * lane_op_clock_ratio;
	pll->pixel_rate_pixel_array = pll->vt.pix_clk_freq_hz;

	return check_all_bounds(dev, limits, op_limits, pll, op_pll);
}

int smiapp_pll_calculate(struct device *dev,
			 const struct smiapp_pll_limits *limits,
			 struct smiapp_pll *pll)
{
	const struct smiapp_pll_branch_limits *op_limits = &limits->op;
	struct smiapp_pll_branch *op_pll = &pll->op;
	uint16_t min_pre_pll_clk_div;
	uint16_t max_pre_pll_clk_div;
	uint32_t lane_op_clock_ratio;
	uint32_t mul, div;
	unsigned int i;
	int rval = -EINVAL;

	if (pll->flags & SMIAPP_PLL_FLAG_NO_OP_CLOCKS) {
		/*
		 * If there's no OP PLL at all, use the VT values
		 * instead. The OP values are ignored for the rest of
		 * the PLL calculation.
		 */
		op_limits = &limits->vt;
		op_pll = &pll->vt;
	}

	if (pll->flags & SMIAPP_PLL_FLAG_OP_PIX_CLOCK_PER_LANE)
		lane_op_clock_ratio = pll->csi2.lanes;
	else
		lane_op_clock_ratio = 1;
	dev_dbg(dev, "lane_op_clock_ratio: %u\n", lane_op_clock_ratio);

	dev_dbg(dev, "binning: %ux%u\n", pll->binning_horizontal,
		pll->binning_vertical);

	switch (pll->bus_type) {
	case SMIAPP_PLL_BUS_TYPE_CSI2:
		/* CSI transfers 2 bits per clock per lane; thus times 2 */
		pll->pll_op_clk_freq_hz = pll->link_freq * 2
			* (pll->csi2.lanes / lane_op_clock_ratio);
		break;
	case SMIAPP_PLL_BUS_TYPE_PARALLEL:
		pll->pll_op_clk_freq_hz = pll->link_freq * pll->bits_per_pixel
			/ DIV_ROUND_UP(pll->bits_per_pixel,
				       pll->parallel.bus_width);
		break;
	default:
		return -EINVAL;
	}

	/* Figure out limits for pre-pll divider based on extclk */
	dev_dbg(dev, "min / max pre_pll_clk_div: %u / %u\n",
		limits->min_pre_pll_clk_div, limits->max_pre_pll_clk_div);
	max_pre_pll_clk_div =
		min_t(uint16_t, limits->max_pre_pll_clk_div,
		      clk_div_even(pll->ext_clk_freq_hz /
				   limits->min_pll_ip_freq_hz));
	min_pre_pll_clk_div =
		max_t(uint16_t, limits->min_pre_pll_clk_div,
		      clk_div_even_up(
			      DIV_ROUND_UP(pll->ext_clk_freq_hz,
					   limits->max_pll_ip_freq_hz)));
	dev_dbg(dev, "pre-pll check: min / max pre_pll_clk_div: %u / %u\n",
		min_pre_pll_clk_div, max_pre_pll_clk_div);

	i = gcd(pll->pll_op_clk_freq_hz, pll->ext_clk_freq_hz);
	mul = div_u64(pll->pll_op_clk_freq_hz, i);
	div = pll->ext_clk_freq_hz / i;
	dev_dbg(dev, "mul %u / div %u\n", mul, div);

	min_pre_pll_clk_div =
		max_t(uint16_t, min_pre_pll_clk_div,
		      clk_div_even_up(
			      DIV_ROUND_UP(mul * pll->ext_clk_freq_hz,
					   limits->max_pll_op_freq_hz)));
	dev_dbg(dev, "pll_op check: min / max pre_pll_clk_div: %u / %u\n",
		min_pre_pll_clk_div, max_pre_pll_clk_div);

	for (pll->pre_pll_clk_div = min_pre_pll_clk_div;
	     pll->pre_pll_clk_div <= max_pre_pll_clk_div;
	     pll->pre_pll_clk_div += 2 - (pll->pre_pll_clk_div & 1)) {
		rval = __smiapp_pll_calculate(dev, limits, op_limits, pll,
					      op_pll, mul, div,
					      lane_op_clock_ratio);
		if (rval)
			continue;

		print_pll(dev, pll);
		return 0;
	}

	dev_dbg(dev, "unable to compute pre_pll divisor\n");

	return rval;
}

static int ar0330_pll_init(struct ar0330 *ar0330, unsigned long rate)
{
	struct smiapp_pll_limits limits;
	int ret;

	limits.min_ext_clk_freq_hz = 6000000;
	limits.max_ext_clk_freq_hz = 64000000;
	/*
	 * HACK: The real minimum pre_pll_clk_div value is 1. The SMIA++ PLL
	 * computation code currently hardcodes pre_pll_clk_div to its minimum
	 * value, so set it to 8 as that's what the AR0330 requires with the
	 * panda board.
	 */
	limits.min_pre_pll_clk_div = 8;
	limits.max_pre_pll_clk_div = 64;
	limits.min_pll_ip_freq_hz = 1000000;
	limits.max_pll_ip_freq_hz = 24000000;
	limits.min_pll_multiplier = 32;
	limits.max_pll_multiplier = 384;
	limits.min_pll_op_freq_hz = 384000000;
	limits.max_pll_op_freq_hz = 768000000;

	limits.vt.min_sys_clk_div = 1;
	limits.vt.max_sys_clk_div = 16;
	limits.vt.min_sys_clk_freq_hz = 24000000;
	limits.vt.max_sys_clk_freq_hz = 768000000;
	limits.vt.min_pix_clk_div = 4;
	limits.vt.max_pix_clk_div = 16;
	limits.vt.min_pix_clk_freq_hz = 1;
	limits.vt.max_pix_clk_freq_hz = 98000000;

	limits.op.min_sys_clk_div = 1;
	limits.op.max_sys_clk_div = 1;
	limits.op.min_sys_clk_freq_hz = 384000000;
	limits.op.max_sys_clk_freq_hz = 768000000;
	limits.op.min_pix_clk_div = 8;
	limits.op.max_pix_clk_div = 12;
	limits.op.min_pix_clk_freq_hz = 1;
	limits.op.max_pix_clk_freq_hz = 98000000;

	ar0330->pll.csi2.lanes = 2;
	ar0330->pll.binning_horizontal = 1;
	ar0330->pll.binning_vertical = 1;
	ar0330->pll.scale_m = 16;
	ar0330->pll.scale_n = 16;
	ar0330->pll.bits_per_pixel = 12;
	ar0330->pll.flags = SMIAPP_PLL_FLAG_OP_PIX_CLOCK_PER_LANE
			  | SMIAPP_PLL_FLAG_DUAL_READOUT;
	ar0330->pll.ext_clk_freq_hz = rate;
	ar0330->pll.link_freq = 294000000;

	ret = smiapp_pll_calculate(ar0330->dev, &limits, &ar0330->pll);
	if (ret < 0)
		return ret;

	dev_dbg(ar0330->dev, "ext_clk_freq_hz %u\n", ar0330->pll.ext_clk_freq_hz);
	dev_dbg(ar0330->dev, "pre_pll_clk_div %u\n", ar0330->pll.pre_pll_clk_div);
	dev_dbg(ar0330->dev, "pll_multiplier %u\n", ar0330->pll.pll_multiplier);
	dev_dbg(ar0330->dev, "vt_pix_clk_div %u\n", ar0330->pll.vt.pix_clk_div);
	dev_dbg(ar0330->dev, "vt_sys_clk_div %u\n", ar0330->pll.vt.sys_clk_div);
	dev_dbg(ar0330->dev, "vt_pix_clk_freq_hz %u\n", ar0330->pll.vt.pix_clk_freq_hz);
	dev_dbg(ar0330->dev, "op_pix_clk_div %u\n", ar0330->pll.op.pix_clk_div);
	dev_dbg(ar0330->dev, "op_sys_clk_div %u\n", ar0330->pll.op.sys_clk_div);

	/*
	 * The sensor has dual pixel readout paths, the pixel rate is equal to
	 * twice the VT pixel clock frequency.
	 */
	__v4l2_ctrl_s_ctrl_int64(ar0330->pixel_rate,
				 ar0330->pll.vt.pix_clk_freq_hz * 2);

	return 0;
}

/* -----------------------------------------------------------------------------
 * Hardware configuration
 */

struct ar0330_register {
	u16 addr;
	u16 value;
	bool wide;
}
;
struct ar0330_patch {
	const struct ar0330_register *reg_data;
	unsigned int reg_size;

	u16 seq_addr;
	const u16 *seq_data;
	unsigned int seq_size;
};

static const u16 ar0330_sequencer_v1[] = {
	0x4540, 0x6134, 0x4a31, 0x4342, 0x4560, 0x2714, 0x3dff, 0x3dff,
	0x3dea, 0x2704, 0x3d10, 0x2705, 0x3d10, 0x2715, 0x3527, 0x053d,
	0x1045, 0x4027, 0x4027, 0x143d, 0xff3d, 0xff3d, 0xea62, 0x2728,
	0x3627, 0x083d, 0x6444, 0x2c2c, 0x2c2c, 0x4b01, 0x432d, 0x4643,
	0x1647, 0x435f, 0x4f50, 0x2604, 0x2684, 0x2027, 0xfc53, 0x0d5c,
	0x0d60, 0x5754, 0x1709, 0x5556, 0x4917, 0x145c, 0x0945, 0x0045,
	0x8026, 0xa627, 0xf917, 0x0227, 0xfa5c, 0x0b5f, 0x5307, 0x5302,
	0x4d28, 0x6c4c, 0x0928, 0x2c28, 0x294e, 0x1718, 0x26a2, 0x5c03,
	0x1744, 0x2809, 0x27f2, 0x1714, 0x2808, 0x164d, 0x1a26, 0x8317,
	0x0145, 0xa017, 0x0727, 0xf317, 0x2945, 0x8017, 0x0827, 0xf217,
	0x285d, 0x27fa, 0x170e, 0x2681, 0x5300, 0x17e6, 0x5302, 0x1710,
	0x2683, 0x2682, 0x4827, 0xf24d, 0x4e28, 0x094c, 0x0b17, 0x6d28,
	0x0817, 0x014d, 0x1a17, 0x0126, 0x035c, 0x0045, 0x4027, 0x9017,
	0x2a4a, 0x0a43, 0x160b, 0x4327, 0x9445, 0x6017, 0x0727, 0x9517,
	0x2545, 0x4017, 0x0827, 0x905d, 0x2808, 0x530d, 0x2645, 0x5c01,
	0x2798, 0x4b12, 0x4452, 0x5117, 0x0260, 0x184a, 0x0343, 0x1604,
	0x4316, 0x5843, 0x1659, 0x4316, 0x5a43, 0x165b, 0x4327, 0x9c45,
	0x6017, 0x0727, 0x9d17, 0x2545, 0x4017, 0x1027, 0x9817, 0x2022,
	0x4b12, 0x442c, 0x2c2c, 0x2c00,
};

static const struct ar0330_register ar0330_register_v1[] = {
	{ 0x30ba, 0x2c, 0 },
	{ 0x30fe, 0x0080, 1 },
	{ 0x31e0, 0x0000, 1 },
	{ 0x3ece, 0xff, 0 },
	{ 0x3ed0, 0xe4f6, 1 },
	{ 0x3ed2, 0x0106, 1 },
	{ 0x3ed4, 0x8f6c, 1 },
	{ 0x3ed6, 0x6666, 1 },
	{ 0x3ed8, 0x6642, 1 },
	{ 0x3eda, 0x8822, 1 },
	{ 0x3edc, 0x2222, 1 },
	{ 0x3ede, 0x22c0, 1 },
	{ 0x3ee0, 0x1500, 1 },
	{ 0x3ee6, 0x0080, 1 },
	{ 0x3ee8, 0x2027, 1 },
	{ 0x3eea, 0x001d, 1 },
	{ 0x3f06, 0x046a, 1 },
};

static const u16 ar0330_sequencer_v2[] = {
	0x4a03, 0x4316, 0x0443, 0x1645, 0x4045, 0x6017, 0x2045, 0x404b,
	0x1244, 0x6134, 0x4a31, 0x4342, 0x4560, 0x2714, 0x3dff, 0x3dff,
	0x3dea, 0x2704, 0x3d10, 0x2705, 0x3d10, 0x2715, 0x3527, 0x053d,
	0x1045, 0x4027, 0x0427, 0x143d, 0xff3d, 0xff3d, 0xea62, 0x2728,
	0x3627, 0x083d, 0x6444, 0x2c2c, 0x2c2c, 0x4b01, 0x432d, 0x4643,
	0x1647, 0x435f, 0x4f50, 0x2604, 0x2684, 0x2027, 0xfc53, 0x0d5c,
	0x0d57, 0x5417, 0x0955, 0x5649, 0x5307, 0x5302, 0x4d28, 0x6c4c,
	0x0928, 0x2c28, 0x294e, 0x5c09, 0x6045, 0x0045, 0x8026, 0xa627,
	0xf817, 0x0227, 0xfa5c, 0x0b17, 0x1826, 0xa25c, 0x0317, 0x4427,
	0xf25f, 0x2809, 0x1714, 0x2808, 0x1701, 0x4d1a, 0x2683, 0x1701,
	0x27fa, 0x45a0, 0x1707, 0x27fb, 0x1729, 0x4580, 0x1708, 0x27fa,
	0x1728, 0x5d17, 0x0e26, 0x8153, 0x0117, 0xe653, 0x0217, 0x1026,
	0x8326, 0x8248, 0x4d4e, 0x2809, 0x4c0b, 0x6017, 0x2027, 0xf217,
	0x535f, 0x2808, 0x164d, 0x1a17, 0x0127, 0xfa26, 0x035c, 0x0145,
	0x4027, 0x9817, 0x2a4a, 0x0a43, 0x160b, 0x4327, 0x9c45, 0x6017,
	0x0727, 0x9d17, 0x2545, 0x4017, 0x0827, 0x985d, 0x2645, 0x4b17,
	0x0a28, 0x0853, 0x0d52, 0x5112, 0x4460, 0x184a, 0x0343, 0x1604,
	0x4316, 0x5843, 0x1659, 0x4316, 0x5a43, 0x165b, 0x4327, 0x9c45,
	0x6017, 0x0727, 0x9d17, 0x2545, 0x4017, 0x1027, 0x9817, 0x2022,
	0x4b12, 0x442c, 0x2c2c, 0x2c00,
};

static const struct ar0330_register ar0330_register_v2[] = {
	{ 0x30ba, 0x2c, 0 },
	{ 0x30fe, 0x0080, 1 },
	{ 0x31e0, 0x0000, 1 },
	{ 0x3ece, 0xff, 0 },
	{ 0x3ed0, 0xe4f6, 1 },
	{ 0x3ed2, 0x0146, 1 },
	{ 0x3ed4, 0x8f6c, 1 },
	{ 0x3ed6, 0x6666, 1 },
	{ 0x3ed8, 0x8642, 1 },
	{ 0x3eda, 0x889b, 1 },
	{ 0x3edc, 0x8863, 1 },
	{ 0x3ede, 0xaa04, 1 },
	{ 0x3ee0, 0x15f0, 1 },
	{ 0x3ee6, 0x008c, 1 },
	{ 0x3ee8, 0x2024, 1 },
	{ 0x3eea, 0xff1f, 1 },
	{ 0x3f06, 0x046a, 1 },
};

static const u16 ar0330_sequencer_v3[] = {
	0x2045,
};

static const struct ar0330_register ar0330_register_v3[] = {
	{ 0x3ed4, 0x8f6c, 1 },
	{ 0x3ed6, 0x6666, 1 }
};

struct ar0330_patch ar0330_patches[] = {
	[0] = {
		.reg_data = ar0330_register_v1,
		.reg_size = ARRAY_SIZE(ar0330_register_v1),
		.seq_addr = 0x8000,
		.seq_data = ar0330_sequencer_v1,
		.seq_size = ARRAY_SIZE(ar0330_sequencer_v1),
	},
	[1] = {
		.reg_data = ar0330_register_v2,
		.reg_size = ARRAY_SIZE(ar0330_register_v2),
		.seq_addr = 0x8000,
		.seq_data = ar0330_sequencer_v2,
		.seq_size = ARRAY_SIZE(ar0330_sequencer_v2),
	},
	[2] = {
		.reg_data = ar0330_register_v3,
		.reg_size = ARRAY_SIZE(ar0330_register_v3),
		.seq_addr = 0x800c,
		.seq_data = ar0330_sequencer_v3,
		.seq_size = ARRAY_SIZE(ar0330_sequencer_v3),
	},
	[3] = {
		.reg_size = 0,
		.seq_size = 0,
	},
};

static int ar0330_otpm_patch(struct ar0330 *ar0330)
{
	const struct ar0330_patch *patch;
	unsigned int i;
	int ret;

	if (ar0330->version == 0)
		return 0;

	patch = &ar0330_patches[ar0330->version - 1];

	for (i = 0; i < patch->reg_size; ++i) {
		if (!patch->reg_data[i].wide)
			ret = ar0330_write8(ar0330, patch->reg_data[i].addr,
					    patch->reg_data[i].value);
		else
			ret = ar0330_write16(ar0330, patch->reg_data[i].addr,
					     patch->reg_data[i].value, NULL);

		if (ret < 0)
			return ret;
	}

	ret = ar0330_write16(ar0330, AR0330_SEQ_CTRL_PORT, patch->seq_addr,
			     NULL);
	if (ret < 0)
		return ret;

	for (i = 0; i < patch->seq_size; ++i) {
		ret = ar0330_write16(ar0330, AR0330_SEQ_DATA_PORT,
				     patch->seq_data[i], NULL);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int ar0330_set_params(struct ar0330 *ar0330)
{
	struct v4l2_mbus_framefmt *format = &ar0330->format;
	const struct v4l2_rect *crop = &ar0330->crop;
	unsigned int xskip;
	unsigned int yskip;
	int ret = 0;

	/* Serial interface. */
	ar0330_write16(ar0330, AR0330_RESET, 0, &ret);
	ar0330_write16(ar0330, AR0330_DATA_FORMAT_BITS, 0x0c0c, &ret);
	ar0330_write16(ar0330, AR0330_SERIAL_FORMAT, 0x0202, &ret);
	ar0330_write16(ar0330, AR0330_FRAME_PREAMBLE, 36, &ret);
	ar0330_write16(ar0330, AR0330_LINE_PREAMBLE, 12, &ret);
	ar0330_write16(ar0330, AR0330_MIPI_TIMING_0, 0x2643, &ret);
	ar0330_write16(ar0330, AR0330_MIPI_TIMING_1, 0x114e, &ret);
	ar0330_write16(ar0330, AR0330_MIPI_TIMING_2, 0x2048, &ret);
	ar0330_write16(ar0330, AR0330_MIPI_TIMING_3, 0x0186, &ret);
	ar0330_write16(ar0330, AR0330_MIPI_TIMING_4, 0x8005, &ret);
	ar0330_write16(ar0330, AR0330_MIPI_CONFIG_STATUS, 0x2003, &ret);

	/*
	 * Windows position and size.
	 *
	 * TODO: Make sure the start coordinates and window size match the
	 * skipping and mirroring requirements.
	 */
	ar0330_write16(ar0330, AR0330_X_ADDR_START, crop->left, &ret);
	ar0330_write16(ar0330, AR0330_Y_ADDR_START, crop->top, &ret);
	ar0330_write16(ar0330, AR0330_X_ADDR_END, crop->left + crop->width,
		       &ret);
	ar0330_write16(ar0330, AR0330_Y_ADDR_END, crop->top + crop->height,
		       &ret);

	/* Row and column binning. */
	xskip = DIV_ROUND_CLOSEST(crop->width, format->width) * 2 - 1;
	yskip = DIV_ROUND_CLOSEST(crop->height, format->height) * 2 - 1;

	ar0330_write16(ar0330, AR0330_X_ODD_INC, xskip, &ret);
	ar0330_write16(ar0330, AR0330_Y_ODD_INC, yskip, &ret);

	/* Blanking - use default values. */
	ar0330_write16(ar0330, AR0330_LINE_LENGTH_PCK, (crop->width + 192) / 2,
		       &ret);
	ar0330_write16(ar0330, AR0330_FRAME_LENGTH_LINES, crop->height + 60,
		       &ret);

	return ret;
}

/* -----------------------------------------------------------------------------
 * V4L2 subdev video operations
 */

static int ar0330_s_stream(struct v4l2_subdev *subdev, int enable)
{
	struct ar0330 *ar0330 = to_ar0330(subdev);
	int ret;

	dev_dbg(ar0330->dev, "%s: frame count is %d\n", __func__,
		ar0330_read16(ar0330, AR0330_FRAME_COUNT));

	if (!enable) {
		ret = ar0330_write8(ar0330, AR0330_MODE_SELECT, 0);
		pm_runtime_mark_last_busy(ar0330->dev);
		pm_runtime_put_autosuspend(ar0330->dev);

		mutex_lock(ar0330->ctrls.lock);
		ar0330->streaming = false;
		mutex_unlock(ar0330->ctrls.lock);

		return ret;
	}

	mutex_lock(ar0330->ctrls.lock);

	/*
	 * Set streaming to true early, protected by the controls lock, to
	 * ensure __v4l2_ctrl_handler_setup() will set the controls. The flag
	 * is reset to false further down if an error occurs.
	 */
	ar0330->streaming = true;

	ret = pm_runtime_get_sync(ar0330->dev);
	if (ret < 0)
		goto done;

	ret = ar0330_pll_configure(ar0330);
	if (ret < 0)
		goto done;

	ret = __v4l2_ctrl_handler_setup(&ar0330->ctrls);
	if (ret < 0)
		goto done;

	ret = ar0330_set_params(ar0330);
	if (ret < 0)
		goto done;

	ret = ar0330_write8(ar0330, AR0330_MODE_SELECT,
			    AR0330_MODE_SELECT_STREAM);

done:
	if (ret < 0) {
		/*
		 * In case of error, turn the power off synchronously as the
		 * device likely has no other chance to recover.
		 */
		pm_runtime_put_sync(ar0330->dev);
		ar0330->streaming = false;
	}

	mutex_unlock(ar0330->ctrls.lock);

	return ret;
}

/* -----------------------------------------------------------------------------
 * V4L2 subdev pad operations
 */

static struct v4l2_mbus_framefmt *
__ar0330_get_pad_format(struct ar0330 *ar0330,
			struct v4l2_subdev_state *sd_state,
			unsigned int pad, u32 which)
{
	switch (which) {
	case V4L2_SUBDEV_FORMAT_TRY:
		return v4l2_subdev_get_try_format(&ar0330->subdev, sd_state,
						  pad);
	case V4L2_SUBDEV_FORMAT_ACTIVE:
		return &ar0330->format;
	default:
		return NULL;
	}
}

static struct v4l2_rect *
__ar0330_get_pad_crop(struct ar0330 *ar0330,
		      struct v4l2_subdev_state *sd_state,
		      unsigned int pad, u32 which)
{
	switch (which) {
	case V4L2_SUBDEV_FORMAT_TRY:
		return v4l2_subdev_get_try_crop(&ar0330->subdev, sd_state,
						pad);
	case V4L2_SUBDEV_FORMAT_ACTIVE:
		return &ar0330->crop;
	default:
		return NULL;
	}
}

static int ar0330_init_cfg(struct v4l2_subdev *subdev,
			   struct v4l2_subdev_state *sd_state)
{
	u32 which = sd_state ? V4L2_SUBDEV_FORMAT_TRY : V4L2_SUBDEV_FORMAT_ACTIVE;
	struct ar0330 *ar0330 = to_ar0330(subdev);
	struct v4l2_mbus_framefmt *format;
	struct v4l2_rect *crop;

	crop = __ar0330_get_pad_crop(ar0330, sd_state, 0, which);
	crop->left = (AR0330_WINDOW_WIDTH_MAX - AR0330_WINDOW_WIDTH_DEF) / 2;
	crop->top = (AR0330_WINDOW_HEIGHT_MAX - AR0330_WINDOW_HEIGHT_DEF) / 2;
	crop->width = AR0330_WINDOW_WIDTH_DEF;
	crop->height = AR0330_WINDOW_HEIGHT_DEF;

	format = __ar0330_get_pad_format(ar0330, sd_state, 0, which);
	format->code = MEDIA_BUS_FMT_SGRBG12_1X12;
	format->width = AR0330_WINDOW_WIDTH_DEF;
	format->height = AR0330_WINDOW_HEIGHT_DEF;
	format->field = V4L2_FIELD_NONE;
	format->colorspace = V4L2_COLORSPACE_SRGB;

	return 0;
}

static int ar0330_enum_mbus_code(struct v4l2_subdev *subdev,
				 struct v4l2_subdev_state *sd_state,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	struct ar0330 *ar0330 = to_ar0330(subdev);

	if (code->pad || code->index)
		return -EINVAL;

	code->code = ar0330->format.code;
	return 0;
}

static int ar0330_enum_frame_size(struct v4l2_subdev *subdev,
				  struct v4l2_subdev_state *sd_state,
				  struct v4l2_subdev_frame_size_enum *fse)
{
	struct ar0330 *ar0330 = to_ar0330(subdev);

	if (fse->index >= 3 || fse->code != ar0330->format.code)
		return -EINVAL;

	fse->min_width = AR0330_WINDOW_WIDTH_DEF / (fse->index + 1);
	fse->max_width = fse->min_width;
	fse->min_height = AR0330_WINDOW_HEIGHT_DEF / (fse->index + 1);
	fse->max_height = fse->min_height;

	return 0;
}

static int ar0330_get_format(struct v4l2_subdev *subdev,
			     struct v4l2_subdev_state *sd_state,
			     struct v4l2_subdev_format *fmt)
{
	struct ar0330 *ar0330 = to_ar0330(subdev);

	fmt->format = *__ar0330_get_pad_format(ar0330, sd_state, fmt->pad,
						fmt->which);
	return 0;
}

static int ar0330_set_format(struct v4l2_subdev *subdev,
			     struct v4l2_subdev_state *sd_state,
			     struct v4l2_subdev_format *format)
{
	struct ar0330 *ar0330 = to_ar0330(subdev);
	struct v4l2_mbus_framefmt *__format;
	struct v4l2_rect *__crop;
	unsigned int width;
	unsigned int height;
	unsigned int hratio;
	unsigned int vratio;

	__crop = __ar0330_get_pad_crop(ar0330, sd_state, format->pad,
					format->which);

	/* Clamp the width and height to avoid dividing by zero. */
	width = clamp_t(unsigned int, ALIGN(format->format.width, 2),
			max(__crop->width / 3, AR0330_WINDOW_WIDTH_MIN),
			__crop->width);
	height = clamp_t(unsigned int, ALIGN(format->format.height, 2),
			 max(__crop->height / 3, AR0330_WINDOW_HEIGHT_MIN),
			 __crop->height);

	hratio = DIV_ROUND_CLOSEST(__crop->width, width);
	vratio = DIV_ROUND_CLOSEST(__crop->height, height);

	__format = __ar0330_get_pad_format(ar0330, sd_state, format->pad,
					    format->which);
	__format->width = __crop->width / hratio;
	__format->height = __crop->height / vratio;

	format->format = *__format;

	return 0;
}

static int ar0330_get_selection(struct v4l2_subdev *subdev,
				struct v4l2_subdev_state *sd_state,
				struct v4l2_subdev_selection *sel)
{
	struct ar0330 *ar0330 = to_ar0330(subdev);

	if (sel->target != V4L2_SEL_TGT_CROP)
		return -EINVAL;

	sel->r = *__ar0330_get_pad_crop(ar0330, sd_state, sel->pad,
					sel->which);
	return 0;
}

static int ar0330_set_selection(struct v4l2_subdev *subdev,
				struct v4l2_subdev_state *sd_state,
				struct v4l2_subdev_selection *sel)
{
	struct ar0330 *ar0330 = to_ar0330(subdev);
	struct v4l2_mbus_framefmt *__format;
	struct v4l2_rect *__crop;
	struct v4l2_rect rect;

	if (sel->target != V4L2_SEL_TGT_CROP)
		return -EINVAL;

	/*
	 * Clamp the crop rectangle boundaries and align them to a multiple of 2
	 * pixels to ensure a GRBG Bayer pattern.
	 */
	rect.left = clamp_t(unsigned int, ALIGN(sel->r.left, 2), 0,
			    AR0330_WINDOW_WIDTH_MAX - AR0330_WINDOW_WIDTH_MIN);
	rect.top = clamp_t(unsigned int, ALIGN(sel->r.top, 2), 0,
			   AR0330_WINDOW_HEIGHT_MAX - AR0330_WINDOW_HEIGHT_MIN);
	rect.width = clamp(ALIGN(sel->r.width, 2),
			   AR0330_WINDOW_WIDTH_MIN,
			   AR0330_WINDOW_WIDTH_MAX);
	rect.height = clamp(ALIGN(sel->r.height, 2),
			    AR0330_WINDOW_HEIGHT_MIN,
			    AR0330_WINDOW_HEIGHT_MAX);

	rect.width = min(rect.width, AR0330_WINDOW_WIDTH_MAX - rect.left);
	rect.height = min(rect.height, AR0330_WINDOW_HEIGHT_MAX - rect.top);

	__crop = __ar0330_get_pad_crop(ar0330, sd_state, sel->pad, sel->which);

	if (rect.width != __crop->width || rect.height != __crop->height) {
		/*
		 * Reset the output image size if the crop rectangle size has
		 * been modified.
		 */
		__format = __ar0330_get_pad_format(ar0330, sd_state, sel->pad,
						    sel->which);
		__format->width = rect.width;
		__format->height = rect.height;
	}

	*__crop = rect;
	sel->r = rect;

	return 0;
}

/* -----------------------------------------------------------------------------
 * V4L2 subdev control operations
 */

static int ar0330_s_ctrl(struct v4l2_ctrl *ctrl)
{
	static const u16 test_pattern[] = { 0, 1, 2, 3, 256, };
	struct ar0330 *ar0330 =
			container_of(ctrl->handler, struct ar0330, ctrls);
	int ret = 0;

	if (!ar0330->streaming)
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE:
		return ar0330_write16(ar0330, AR0330_COARSE_INTEGRATION_TIME,
				      ctrl->val, NULL);

	case V4L2_CID_GAIN:
		return ar0330_write16(ar0330, AR0330_GLOBAL_GAIN, ctrl->val,
				      NULL);

	case V4L2_CID_HFLIP:
	case V4L2_CID_VFLIP: {
		u16 clr = 0;
		u16 set = 0;

		ret = ar0330_write16(ar0330, AR0330_LOCK_CONTROL,
				     AR0330_LOCK_CONTROL_UNLOCK, NULL);
		if (ret < 0)
			return ret;

		if (ar0330->flip[0]->val)
			set |= AR0330_READ_MODE_HORIZ_MIRROR;
		else
			clr |= AR0330_READ_MODE_HORIZ_MIRROR;

		if (ar0330->flip[1]->val)
			set |= AR0330_READ_MODE_VERT_FLIP;
		else
			clr |= AR0330_READ_MODE_VERT_FLIP;

		ret = ar0330_set_read_mode(ar0330, clr, set);

		ar0330_write16(ar0330, AR0330_LOCK_CONTROL, 0, &ret);
		return ret;
	}

	case V4L2_CID_TEST_PATTERN:
		if (ctrl->val == 1) {
			ar0330_write16(ar0330, AR0330_TEST_DATA_RED, 0x0eb0,
				       &ret);
			ar0330_write16(ar0330, AR0330_TEST_DATA_GREENR, 0x0690,
				       &ret);
			ar0330_write16(ar0330, AR0330_TEST_DATA_BLUE, 0x00b0,
				       &ret);
			ar0330_write16(ar0330, AR0330_TEST_DATA_GREENB, 0x0690,
				       &ret);
			if (ret < 0)
				return ret;
		}

		return ar0330_write16(ar0330, AR0330_TEST_PATTERN_MODE,
				      test_pattern[ctrl->val], NULL);
	}

	return 0;
}

static const struct v4l2_ctrl_ops ar0330_ctrl_ops = {
	.s_ctrl = ar0330_s_ctrl,
};

static const char * const ar0330_test_pattern_menu[] = {
	"Disabled",
	"Solid Color",
	"Full Color Bar",
	"Fade-to-gray Color Bar",
	"Walking 1s",
};

/* -----------------------------------------------------------------------------
 * V4L2 subdev operations
 */

static const struct v4l2_subdev_video_ops ar0330_subdev_video_ops = {
	.s_stream       = ar0330_s_stream,
};

static const struct v4l2_subdev_pad_ops ar0330_subdev_pad_ops = {
	.init_cfg = ar0330_init_cfg,
	.enum_mbus_code = ar0330_enum_mbus_code,
	.enum_frame_size = ar0330_enum_frame_size,
	.get_fmt = ar0330_get_format,
	.set_fmt = ar0330_set_format,
	.get_selection = ar0330_get_selection,
	.set_selection = ar0330_set_selection,
};

static const struct v4l2_subdev_ops ar0330_subdev_ops = {
	.video  = &ar0330_subdev_video_ops,
	.pad    = &ar0330_subdev_pad_ops,
};

/* -----------------------------------------------------------------------------
 * Power management
 */

static int ar0330_power_on(struct ar0330 *ar0330)
{
	int ret;

	ret = regulator_bulk_enable(ARRAY_SIZE(ar0330->supplies),
				    ar0330->supplies);
	if (ret < 0) {
		dev_err(ar0330->dev, "Failed to enable power supplies: %d\n",
			ret);
		return ret;
	}

	/* Assert reset for 1ms */
	if (ar0330->reset) {
		gpiod_set_value(ar0330->reset, 1);
		usleep_range(2000, 3000);
		gpiod_set_value(ar0330->reset, 0);
		usleep_range(10000, 11000);
	}

	return 0;
}

static int ar0330_power_on_init(struct ar0330 *ar0330)
{
	return ar0330_otpm_patch(ar0330);
}

static void ar0330_power_off(struct ar0330 *ar0330)
{
	regulator_bulk_disable(ARRAY_SIZE(ar0330->supplies), ar0330->supplies);
}

static int ar0330_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *subdev = i2c_get_clientdata(client);
	struct ar0330 *ar0330 = to_ar0330(subdev);
	int ret;

	ret = ar0330_power_on(ar0330);
	if (ret < 0)
		return ret;

	ret = ar0330_power_on_init(ar0330);
	if (ret < 0) {
		ar0330_power_off(ar0330);
		return ret;
	}

	return 0;
}

static int ar0330_runtime_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *subdev = i2c_get_clientdata(client);
	struct ar0330 *ar0330 = to_ar0330(subdev);

	ar0330_power_off(ar0330);

	return 0;
}

static const struct dev_pm_ops ar0330_pm_ops = {
        SET_RUNTIME_PM_OPS(ar0330_runtime_suspend, ar0330_runtime_resume, NULL)
};

/* -----------------------------------------------------------------------------
 * Driver initialization and probing
 */

static int ar0330_identify(struct ar0330 *ar0330)
{
	s32 revision;
	s32 data;

	/* Read out the chip version register */
	data = ar0330_read16(ar0330, AR0330_CHIP_VERSION);
	if (data < 0)
		return data;

	if (data != AR0330_CHIP_VERSION_VALUE) {
		dev_err(ar0330->dev, "AR0330 not detected, wrong version "
			"0x%04x\n", data);
		return -ENODEV;
	}

	revision = ar0330_read8(ar0330, AR0330_CHIP_REVISION);
	if (revision < 0)
		return revision;

	data = ar0330_read16(ar0330, AR0330_TEST_DATA_RED);
	if (data < 0)
		return data;

	if (revision == 0x10)
		ar0330->version = 1;
	else if (revision != 0x20)
		ar0330->version = 2;
	else if (data == 0x0006)
		ar0330->version = 3;
	else if (data == 0x0007)
		ar0330->version = 4;
	else
		ar0330->version = 0;

	dev_info(ar0330->dev, "AR0330 rev. %02x ver. %u detected at address "
		 "0x%02x\n", revision, ar0330->version, ar0330->client->addr);

	return 0;
}

static int ar0330_probe(struct i2c_client *client,
			 const struct i2c_device_id *did)
{
	unsigned long rate = 24000000;
	struct ar0330 *ar0330;
	unsigned int i;
	int ret;

	ar0330 = kzalloc(sizeof(*ar0330), GFP_KERNEL);
	if (ar0330 == NULL)
		return -ENOMEM;

	ar0330->client = client;
	ar0330->dev = &client->dev;
	ar0330->read_mode = 0;

	ar0330->reset = devm_gpiod_get_optional(ar0330->dev, "reset",
						GPIOD_OUT_HIGH);
	if (IS_ERR(ar0330->reset)) {
		ret = PTR_ERR(ar0330->reset);
		if (ret != -EPROBE_DEFER)
			dev_err(ar0330->dev, "Failed to get reset GPIO: %d\n",
				ret);
		goto error_free;
	}

	for (i = 0; i < ARRAY_SIZE(ar0330->supplies); ++i)
		ar0330->supplies[i].supply = ar0330_supply_names[i];

	ret = devm_regulator_bulk_get(ar0330->dev, ARRAY_SIZE(ar0330->supplies),
				      ar0330->supplies);
	if (ret < 0) {
		if (ret != -EPROBE_DEFER)
			dev_err(ar0330->dev, "Failed to get power supplies: %d\n",
				ret);
		goto error_free;
	}

	/*
	 * Enable power management. The driver supports runtime PM, but needs to
	 * work when runtime PM is disabled in the kernel. To that end, power
	 * the sensor on manually here, identify it, and fully initialize it.
	 */
	ret = ar0330_power_on(ar0330);
	if (ret < 0)
		goto error_free;

	ret = ar0330_identify(ar0330);
	if (ret < 0)
		goto error_power;

	ret = ar0330_power_on_init(ar0330);
	if (ret < 0)
		goto error_power;

	/* Create V4L2 controls. */
	v4l2_ctrl_handler_init(&ar0330->ctrls, 6);

	v4l2_ctrl_new_std(&ar0330->ctrls, &ar0330_ctrl_ops,
			  V4L2_CID_EXPOSURE, AR0330_COARSE_INTEGRATION_TIME_MIN,
			  AR0330_COARSE_INTEGRATION_TIME_MAX, 1,
			  AR0330_COARSE_INTEGRATION_TIME_DEF);
	v4l2_ctrl_new_std(&ar0330->ctrls, &ar0330_ctrl_ops,
			  V4L2_CID_GAIN, AR0330_GLOBAL_GAIN_MIN,
			  AR0330_GLOBAL_GAIN_MAX, 1, AR0330_GLOBAL_GAIN_DEF);
	ar0330->flip[0] = v4l2_ctrl_new_std(&ar0330->ctrls, &ar0330_ctrl_ops,
					    V4L2_CID_HFLIP, 0, 1, 1, 0);
	ar0330->flip[1] = v4l2_ctrl_new_std(&ar0330->ctrls, &ar0330_ctrl_ops,
					    V4L2_CID_VFLIP, 0, 1, 1, 0);
	ar0330->pixel_rate = v4l2_ctrl_new_std(&ar0330->ctrls, &ar0330_ctrl_ops,
					       V4L2_CID_PIXEL_RATE,
					       0, 0, 1, 0);
	v4l2_ctrl_new_std_menu_items(&ar0330->ctrls, &ar0330_ctrl_ops,
				     V4L2_CID_TEST_PATTERN,
				     ARRAY_SIZE(ar0330_test_pattern_menu) - 1,
				     0, 0, ar0330_test_pattern_menu);

	v4l2_ctrl_cluster(ARRAY_SIZE(ar0330->flip), ar0330->flip);

	if (ar0330->ctrls.error)
		dev_err(ar0330->dev, "%s: control initialization error %d\n",
			__func__, ar0330->ctrls.error);

	ar0330->subdev.ctrl_handler = &ar0330->ctrls;

	/* Initialize the media entity and V4L2 subdev. */
	ar0330->pad.flags = MEDIA_PAD_FL_SOURCE;
	ret = media_entity_pads_init(&ar0330->subdev.entity, 1, &ar0330->pad);
	if (ret < 0)
		goto error_ctrl;

	v4l2_i2c_subdev_init(&ar0330->subdev, client, &ar0330_subdev_ops);
	ar0330->subdev.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	ar0330->subdev.entity.function = MEDIA_ENT_F_CAM_SENSOR;

	ar0330_init_cfg(&ar0330->subdev, NULL);

	ret = ar0330_pll_init(ar0330, rate);
	if (ret < 0) {
		dev_err(ar0330->dev, "PLL initialization failed\n");
		goto error_media;
	}

	/*
	 * Enable runtime PM. As the device has been powered manually, mark it
	 * as active, and increase the usage count without resuming the device.
	 */
	pm_runtime_set_active(ar0330->dev);
	pm_runtime_get_noresume(ar0330->dev);
	pm_runtime_enable(ar0330->dev);

	ret = v4l2_async_register_subdev(&ar0330->subdev);
	if (ret < 0) {
		dev_err(ar0330->dev, "Subdev registration failed\n");
		goto error_pm;
	}

	/*
	 * Finally, enable autosuspend and decrease the usage count. The device
	 * will get suspended after the autosuspend delay, turning the power
	 * off.
	 */
	pm_runtime_set_autosuspend_delay(ar0330->dev, 1000);
	pm_runtime_use_autosuspend(ar0330->dev);
	pm_runtime_put_autosuspend(ar0330->dev);

	return 0;

error_pm:
	pm_runtime_disable(ar0330->dev);
	pm_runtime_put_noidle(ar0330->dev);
error_media:
	media_entity_cleanup(&ar0330->subdev.entity);
error_ctrl:
	v4l2_ctrl_handler_free(&ar0330->ctrls);
error_power:
	ar0330_power_off(ar0330);
error_free:
	kfree(ar0330);

	return ret;
}

static int ar0330_remove(struct i2c_client *client)
{
	struct v4l2_subdev *subdev = i2c_get_clientdata(client);
	struct ar0330 *ar0330 = to_ar0330(subdev);

	v4l2_ctrl_handler_free(&ar0330->ctrls);
	v4l2_async_unregister_subdev(subdev);
	media_entity_cleanup(&subdev->entity);

	/*
	 * Disable runtime PM. In case runtime PM is disabled in the kernel,
	 * make sure to turn power off manually.
	 */
	pm_runtime_disable(ar0330->dev);
	if (!pm_runtime_status_suspended(ar0330->dev))
		ar0330_power_off(ar0330);
	pm_runtime_set_suspended(ar0330->dev);

	kfree(ar0330);

	return 0;
}

static const struct i2c_device_id ar0330_id[] = {
	{ "ar0330", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ar0330_id);

static struct i2c_driver ar0330_i2c_driver = {
	.driver = {
		.name = "ar0330",
		.pm = &ar0330_pm_ops,
	},
	.probe          = ar0330_probe,
	.remove         = ar0330_remove,
	.id_table       = ar0330_id,
};

module_i2c_driver(ar0330_i2c_driver);

MODULE_DESCRIPTION("Aptina AR0330 Camera driver");
MODULE_AUTHOR("Laurent Pinchart <laurent.pinchart@ideasonboard.com>");
MODULE_LICENSE("GPL v2");
