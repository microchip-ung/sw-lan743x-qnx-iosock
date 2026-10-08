/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Copyright (C) 2019-2026. Microchip Technology Inc. and its
 *       subsidiaries (Microchip).
 *
 * You are permitted to use the software and its derivatives with Microchip
 * products. See the license agreement accompanying this software, if any,
 * for additional info regarding your rights and obligations.
 *
 * SOFTWARE AND DOCUMENTATION ARE PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
 * KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION, ANY
 * WARRANTY OF MERCHANTABILITY, TITLE, NON-INFRINGEMENT AND FITNESS FOR A
 * PARTICULAR PURPOSE. IN NO EVENT SHALL MICROCHIP, SMSC, OR ITS LICENSORS
 * BE LIABLE OR OBLIGATED UNDER CONTRACT, NEGLIGENCE, STRICT LIABILITY,
 * CONTRIBUTION, BREACH OF WARRANTY, OR OTHER LEGAL EQUITABLE THEORY FOR
 * ANY DIRECT OR INDIRECT DAMAGES OR EXPENSES INCLUDING BUT NOT LIMITED TO
 * ANY INCIDENTAL, SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES, OR OTHER
 * SIMILAR COSTS. TO THE FULLEST EXTENT ALLOWED BY LAW, MICROCHIP AND ITS
 * LICENSORS LIABILITY WILL NOT EXCEED THE AMOUNT OF FEES, IF ANY, THAT YOU
 * PAID DIRECTLY TO MICROCHIP TO USE THIS SOFTWARE. MICROCHIP PROVIDES THIS
 * SOFTWARE CONDITIONALLY UPON YOUR ACCEPTANCE OF THESE TERMS.
 */

#include <sys/cdefs.h>
/*
 * Driver for Microchip LAN8870 100/1000BASE-T1 PHY
 */

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/resource.h>
#include <sys/rman.h>
#include <sys/socket.h>
#include <sys/slog.h>
#include <sys/slogcodes.h>
#include <machine/resource.h>

#include <sys/slog.h>
#include <sys/slogcodes.h>

#include <net/if.h>
#include <net/if_media.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#ifdef FDT
#include <dev/mii/mii_fdt.h>
#endif

#include <qnx/qnx_modload.h>
#include <sys/sbuf.h>
#include <sys/sysctl.h>
#include <stdbool.h>

#include "miidevs.h"
#include "miibus_if.h"
#include "lan887x.h"

static void load_cfg_sysctl_overrides(struct lan887x_softc *phy);

/* Logging Macros */
#define SLOG_ERR(fmt, ...) \
	slogf(_SLOGC_NETWORK, _SLOG_ERROR, "lan887x: " fmt, ##__VA_ARGS__)
#define SLOG_INF(fmt, ...) \
	slogf(_SLOGC_NETWORK, _SLOG_INFO, "lan887x: " fmt, ##__VA_ARGS__)
#define SLOG_DBG(fmt, ...) \
	slogf(_SLOGC_NETWORK, _SLOG_DEBUG1, "lan887x: " fmt, ##__VA_ARGS__)

static int lan887x_probe(device_t);
static int lan887x_attach(device_t);
static uint16_t lan887x_read_mmd(struct mii_softc *, uint16_t, uint16_t);
static void lan887x_write_mmd(struct mii_softc *, uint16_t, uint16_t, uint16_t);

static int lan887x_service(struct mii_softc *, struct mii_data *, int);
static void lan887x_status(struct mii_softc *);
static void lan887x_reset(struct mii_softc *);

static int lan887x_sqi(struct lan887x_softc *);
static void lan887x_add_sysctl(device_t);

static device_method_t lan887x_methods[] = {
	DEVMETHOD(device_probe, lan887x_probe),
	DEVMETHOD(device_attach, lan887x_attach),
	DEVMETHOD(device_detach, mii_phy_detach),
	DEVMETHOD_END,
};

static driver_t lan887x_driver = {
	"lan887x",
	lan887x_methods,
	sizeof(struct lan887x_softc),
	NULL,
	0,
	NULL,
};

DRIVER_MODULE(lan887x, miibus, lan887x_driver, 0, 0);
MODULE_DEPEND(lan887x, ether, 1, 1, 1);
MODULE_DEPEND(lan887x, miibus, 1, 1, 1);
MODULE_DEPEND(lan887x, iflib, 1, 1, 1);

static char *lan887x_drvr_version = "0.1.1";

int drvr_ver = (int)IOSOCK_VERSION_CUR;

struct _iosock_module_version iosock_module_version =
    IOSOCK_MODULE_VER_SYM_INIT;

static int
lan887x_sqi_handler(SYSCTL_HANDLER_ARGS)
{
	struct lan887x_softc *phy = (struct lan887x_softc *)arg1;
	int sqi = 0;

	sqi = lan887x_sqi(phy);
	sysctl_handle_int(oidp, &sqi, 0, req);

	return (0);
}

static const struct mii_phydesc lan887xphys[] = {
	MII_PHY_DESC(MICROCHIP, LAN887X),
	MII_PHY_END,
};

static const struct mii_phy_funcs lan887x_funcs = {
	lan887x_service,
	lan887x_status,
	lan887x_reset,
};

static uint16_t
lan887x_read(struct mii_softc *phy, uint16_t reg)
{
	return PHY_READ(phy, reg);
}

static void
lan887x_write(struct mii_softc *phy, uint16_t reg, uint16_t val)
{
	PHY_WRITE(phy, reg, val);
}

static void
lan887x_modify(struct mii_softc *phy, uint16_t addr, uint16_t mask,
    uint16_t val)
{
	uint16_t rd_val = lan887x_read(phy, addr);
	uint16_t reg_val;

	reg_val = (rd_val & ~mask) | val;
	if (reg_val != rd_val)
		lan887x_write(phy, addr, reg_val);
}

static uint16_t
lan887x_read_mmd(struct mii_softc *phy, uint16_t devaddr, uint16_t reg)
{
	/* Set up device address and register. */
	PHY_WRITE(phy, MII_MMDACR, devaddr);
	PHY_WRITE(phy, MII_MMDAADR, reg);

	/* Select register data for MMD and read the value. */
	PHY_WRITE(phy, MII_MMDACR, (MMDACR_FN_DATANPI | devaddr));

	return (PHY_READ(phy, MII_MMDAADR));
}

static void
lan887x_write_mmd(struct mii_softc *phy, uint16_t devaddr, uint16_t reg,
    uint16_t val)
{

	/* Set up device address and register. */
	PHY_WRITE(phy, MII_MMDACR, devaddr);
	PHY_WRITE(phy, MII_MMDAADR, reg);

	/* Select register data for MMD and write the value. */
	PHY_WRITE(phy, MII_MMDACR, MMDACR_FN_DATANPI | devaddr);
	PHY_WRITE(phy, MII_MMDAADR, val);
}

static void
lan887x_modify_mmd(struct mii_softc *phy, uint16_t devad, uint16_t addr,
    uint16_t mask, uint16_t val)
{
	uint16_t rd_val = lan887x_read_mmd(phy, devad, addr);
	uint16_t reg_val;

	reg_val = (rd_val & ~mask) | val;
	if (reg_val != rd_val)
		lan887x_write_mmd(phy, devad, addr, reg_val);
}

static void
lan887x_clear_bits_mmd(struct mii_softc *phy, uint16_t devaddr, uint16_t reg,
    uint16_t val)
{
	lan887x_modify_mmd(phy, devaddr, reg, val, 0U);
}

static void
lan887x_set_bits_mmd(struct mii_softc *phy, uint16_t devaddr, uint16_t reg,
    uint16_t val)
{
	lan887x_modify_mmd(phy, devaddr, reg, 0U, val);
}

static int
lan887x_poll_reg(struct mii_softc *sc,
			     uint16_t offset, uint16_t *val,
				 uint16_t mask, uint16_t exp_val,
				 const uint32_t timeout_us,
				 const uint32_t sleepint_us)
{
    uint16_t retries = timeout_us / sleepint_us;

    for (uint16_t i = 0U; i < retries; i++) {
            DELAY((int)sleepint_us);
            *val = lan887x_read(sc, offset);
            if (((*val) & mask) == exp_val) {
                    return EOK;
            }
    }

    SLOG_ERR("poll_timedout!(0x%x)", *val);
    return EBUSY;
}

static int
lan887x_poll_mmd_reg(struct mii_softc *sc,
					 uint8_t mmd, uint32_t offset, uint32_t *val,
					 uint32_t mask, uint32_t exp_val,
					 const uint32_t timeout_us,
					 const uint32_t sleepint_us)
{
    uint32_t retries = timeout_us / sleepint_us;

    for (uint16_t i = 0; i < retries; i++) {
            DELAY((int)sleepint_us);
            *val = lan887x_read_mmd(sc, mmd, offset);
            if (((*val) & mask) == exp_val) {
                    return EOK;
            }
    }

    SLOG_ERR("mmd_poll_timedout!(0x%x)", *val);
    return EBUSY;
}

static int
lan887x_rgmii_init(struct mii_softc *mii_sc)
{
	SLOG_DBG("rgmii_init");
	/* SGMII mux disable */
	lan887x_clear_bits_mmd(mii_sc, CL45_MMD_VEND1, LAN887X_SGMII_CTL,
	    LAN887X_SGMII_CTL_SGMII_MUX_EN);

	/* Select MAC_MODE as RGMII */
	lan887x_modify_mmd(mii_sc, CL45_MMD_VEND1, LAN887X_MIS_CFG_REG0,
	    LAN887X_MIS_CFG_REG0_MAC_MODE_SEL, LAN887X_MAC_MODE_RGMII);

	/* Disable PCS */
	lan887x_clear_bits_mmd(mii_sc, CL45_MMD_VEND1, LAN887X_SGMII_PCS_CFG,
	    LAN887X_SGMII_PCS_CFG_PCS_ENA);

	/* LAN887X Errata: RGMII rx clock active in SGMII mode
	 * Disabled it for SGMII mode
	 * Re-enabling it for RGMII mode
	 */
	lan887x_clear_bits_mmd(mii_sc, CL45_MMD_VEND1, LAN887X_MIS_CFG_REG0,
	    LAN887X_MIS_CFG_REG0_RCLKOUT_DIS);

	return (0);
}

static int
lan887x_sgmii_init(struct mii_softc *phy)
{
	SLOG_DBG("sgmii_init");
	/* SGMII mux enable */
	lan887x_set_bits_mmd(phy, CL45_MMD_VEND1, LAN887X_SGMII_CTL,
	    LAN887X_SGMII_CTL_SGMII_MUX_EN);

	/* Select MAC_MODE as SGMII */
	lan887x_modify_mmd(phy, CL45_MMD_VEND1, LAN887X_MIS_CFG_REG0,
	    LAN887X_MIS_CFG_REG0_MAC_MODE_SEL, LAN887X_MAC_MODE_SGMII);

	/* LAN887X Errata: RGMII rx clock active in SGMII mode.
	 * So disabling it for SGMII mode
	 */
	lan887x_set_bits_mmd(phy, CL45_MMD_VEND1, LAN887X_MIS_CFG_REG0,
	    LAN887X_MIS_CFG_REG0_RCLKOUT_DIS);

	/* Enable PCS */
	lan887x_set_bits_mmd(phy, CL45_MMD_VEND1, LAN887X_SGMII_PCS_CFG,
	    LAN887X_SGMII_PCS_CFG_PCS_ENA);

	return 0;
}

static int
lan887x_config_rgmii_en(struct lan887x_softc *phy)
{
	struct mii_softc *sc = &phy->mii_sc;
	int txc;
	int rxc;
	int ret;

	SLOG_DBG("config_rgmii_en");

	ret = lan887x_rgmii_init(sc);
	if (ret < 0)
		return ret;

	/* Control bit to enable/disable TX DLL delay line in signal path */
	txc = lan887x_read_mmd(sc, CL45_MMD_VEND1, LAN887X_MIS_DLL_CFG_REG0);
	if (txc < 0)
		return txc;

	/* Control bit to enable/disable RX DLL delay line in signal path */
	rxc = lan887x_read_mmd(sc, CL45_MMD_VEND1, LAN887X_MIS_DLL_CFG_REG1);
	if (rxc < 0)
		return rxc;

	/* Configures the phy to enable RX/TX delay
	 * RGMII        - TX & RX delays are either added by MAC or not needed,
	 *                phy should not add
	 * RGMII_ID     - Configures phy to enable TX & RX delays, MAC shouldn't add
	 * RGMII_RX_ID  - Configures the PHY to enable the RX delay.
	 *                The MAC shouldn't add the RX delay
	 * RGMII_TX_ID  - Configures the PHY to enable the TX delay.
	 *                The MAC shouldn't add the TX delay in this case
	 */
	switch (phy->cfg.contype) {
	case MII_CONTYPE_RGMII:
		txc &= ~LAN887X_MIS_DLL_CONF;
		rxc &= ~LAN887X_MIS_DLL_CONF;
		SLOG_INF("RGMII\n");
		break;
	case MII_CONTYPE_RGMII_ID:
		txc |= LAN887X_MIS_DLL_CONF;
		rxc |= LAN887X_MIS_DLL_CONF;
		SLOG_INF("RGMII_ID\n");
		break;
	case MII_CONTYPE_RGMII_RXID:
		txc &= ~LAN887X_MIS_DLL_CONF;
		rxc |= LAN887X_MIS_DLL_CONF;
		SLOG_INF("RGMII_RXID\n");
		break;
	case MII_CONTYPE_RGMII_TXID:
		txc |= LAN887X_MIS_DLL_CONF;
		rxc &= ~LAN887X_MIS_DLL_CONF;
		SLOG_INF("RGMII_TXID\n");
		break;
	default:
		SLOG_ERR("Invalid phy interface %d\n", phy->cfg.contype);
		return 0;
	}

	/* Configures the PHY to enable/disable RX delay in signal path */
	lan887x_modify_mmd(sc, CL45_MMD_VEND1, LAN887X_MIS_DLL_CFG_REG1,
	    LAN887X_MIS_DLL_CONF, rxc);

	/* Configures the PHY to enable/disable the TX delay in signal path */
	lan887x_modify_mmd(sc, CL45_MMD_VEND1, LAN887X_MIS_DLL_CFG_REG0,
	    LAN887X_MIS_DLL_CONF, txc);

	return (0);
}

static int
lan887x_config_phy_interface(struct lan887x_softc *phy)
{
	struct mii_softc *sc = &phy->mii_sc;
	int interface_mode;
	int sgmii_dis;
	int ret;

	SLOG_DBG("config_phy_interface");

	/* Read sku efuse data for interfaces supported by sku */
	ret = lan887x_read_mmd(sc, CL45_MMD_VEND1, LAN887X_EFUSE_READ_DAT9);
	if (ret < 0)
		return ret;

	/* If interface_mode is 1 then efuse sets RGMII operations.
	 * If interface mode is 3 then efuse sets SGMII operations.
	 */
	interface_mode = ret & LAN887X_EFUSE_MAC_MODE_M;
	/* SGMII disable is set for RGMII operations */
	sgmii_dis = ret & LAN887X_EFUSE_SGMII_DIS;
	if (mii_contype_is_rgmii(phy->cfg.contype)) {
		ret = -EOPNOTSUPP;
		if (interface_mode & LAN887X_MAC_MODE_RGMII)
			ret = lan887x_config_rgmii_en(phy);
	} else if (phy->cfg.contype == MII_CONTYPE_SGMII) {
		ret = -EOPNOTSUPP;
		if (!sgmii_dis)
			ret = lan887x_sgmii_init(sc);
	} else {
		ret = 0;
	}

	return (ret);
}

static int
lan887x_phy_init(struct lan887x_softc *phy)
{
	struct mii_softc *mii_sc = &phy->mii_sc;

	SLOG_DBG("phy_init");

	/* Clear loopback */
	lan887x_clear_bits_mmd(mii_sc, CL45_MMD_VEND1, LAN887X_MIS_CFG_REG2,
	    LAN887X_MIS_CFG_REG2_FE_LPBK_EN);

	/* Configure default behavior of led to link and activity for any
	 * speed
	 */
	lan887x_modify_mmd(mii_sc, CL45_MMD_VEND1, LAN887X_COMMON_LED3_LED2,
	    LAN887X_COMMON_LED2_MODE_SEL_MASK, LAN887X_LED_LINK_ACT_ANY_SPEED);

	/* PHY interface setup */
	return lan887x_config_phy_interface(phy);
}

static int
lan887x_load_values(struct mii_softc *phy,
    const struct lan887x_regwr_map *reg_map, int cnt)
{
	SLOG_DBG("load_values");

	for (int i = 0; i < cnt; i++) {
		lan887x_write_mmd(phy, reg_map[i].mmd, reg_map[i].reg,
		    reg_map[i].val);
	}

	return (0);
}

static int
lan887x_phy_setup(struct mii_softc *phy)
{
	static const struct lan887x_regwr_map phy_cfg[] = {
		/* PORT_AFE writes */
		{ CL45_MMD_PMAPMD, LAN887X_ZQCAL_CONTROL_1, 0x4008 },
		{ CL45_MMD_PMAPMD, LAN887X_AFE_PORT_TESTBUS_CTRL2, 0x0000 },
		{ CL45_MMD_PMAPMD, LAN887X_AFE_PORT_TESTBUS_CTRL6, 0x0040 },
		/* 100T1_PCS_VENDOR writes */
		{ CL45_MMD_PCS, LAN887X_IDLE_ERR_CNT_THRESH, 0x0008 },
		{ CL45_MMD_PCS, LAN887X_IDLE_ERR_TIMER_WIN, 0x800d },
		/* 100T1 DSP writes */
		{ CL45_MMD_VEND1, LAN887X_CDR_CONFIG1_100, 0x0ab1 },
		{ CL45_MMD_VEND1, LAN887X_LOCK1_EQLSR_CONFIG_100, 0x5274 },
		{ CL45_MMD_VEND1, LAN887X_SLV_HD_MUFAC_CONFIG_100, 0x0d74 },
		{ CL45_MMD_VEND1, LAN887X_PLOCK_MUFAC_CONFIG_100, 0x0aea },
		{ CL45_MMD_VEND1, LAN887X_PROT_DISABLE_100, 0x0360 },
		{ CL45_MMD_VEND1, LAN887X_KF_LOOP_SAT_CONFIG_100, 0x0c30 },
		/* 1000T1 DSP writes */
		{ CL45_MMD_VEND1, LAN887X_LOCK1_EQLSR_CONFIG, 0x2a78 },
		{ CL45_MMD_VEND1, LAN887X_LOCK3_EQLSR_CONFIG, 0x1368 },
		{ CL45_MMD_VEND1, LAN887X_PROT_DISABLE, 0x1354 },
		{ CL45_MMD_VEND1, LAN887X_FFE_GAIN6, 0x3C84 },
		{ CL45_MMD_VEND1, LAN887X_FFE_GAIN7, 0x3ca5 },
		{ CL45_MMD_VEND1, LAN887X_FFE_GAIN8, 0x3ca5 },
		{ CL45_MMD_VEND1, LAN887X_FFE_GAIN9, 0x3ca5 },
		{ CL45_MMD_VEND1, LAN887X_ECHO_DELAY_CONFIG, 0x0024 },
		{ CL45_MMD_VEND1, LAN887X_FFE_MAX_CONFIG, 0x227f },
		/* 1000T1 PCS writes */
		{ CL45_MMD_PCS, LAN887X_SCR_CONFIG_3, 0x1e00 },
		{ CL45_MMD_PCS, LAN887X_INFO_FLD_CONFIG_5, 0x0fa1 },
	};
	int ret;

	SLOG_DBG("phy_setup");

	ret = lan887x_load_values(phy, phy_cfg,
	    (sizeof(phy_cfg) / sizeof((phy_cfg)[0])));
	if (ret < 0)
		return ret;

	return (0);
}

static int
lan887x_100M_setup(struct mii_softc *sc, bool is_master)
{
	struct lan887x_softc *phy = (struct lan887x_softc *)sc;
	int ret;

	/* (Re)configure the speed/mode dependent T1 settings */
	if (phy->cfg.aneg || is_master) {
		static const struct lan887x_regwr_map phy_cfg[] = {
			{ CL45_MMD_PMAPMD, LAN887X_AFE_PORT_TESTBUS_CTRL4, 0x00b8 },
			{ CL45_MMD_PMAPMD, LAN887X_TX_AMPLT_1000T1_REG, 0x0038 },
			{ CL45_MMD_VEND1, LAN887X_INIT_COEFF_DFE1_100, 0x000f },
		};

		SLOG_INF("100M_setup aneg/master");
		ret = lan887x_load_values(sc, phy_cfg, (sizeof(phy_cfg) / sizeof((phy_cfg)[0])));
	} else {
		static const struct lan887x_regwr_map slv_cfg[] = {
			{ CL45_MMD_PMAPMD, LAN887X_AFE_PORT_TESTBUS_CTRL4, 0x0038 },
			{ CL45_MMD_VEND1, LAN887X_INIT_COEFF_DFE1_100, 0x0014 },
		};

		SLOG_INF("100M_setup forced/slave");
		ret = lan887x_load_values(sc, slv_cfg, (sizeof(slv_cfg) / sizeof((slv_cfg)[0])));
	}
	if (ret < 0)
		return ret;

	if (phy->cfg.aneg)
		lan887x_set_bits_mmd(sc, CL45_MMD_PMAPMD, LAN887X_DSP_PMA_CONTROL, LAN887X_DSP_PMA_CONTROL_LNK_SYNC);
	else
		lan887x_set_bits_mmd(sc, CL45_MMD_VEND1, LAN887X_REG_REG26, LAN887X_REG_REG26_HW_INIT_SEQ_EN);

	return (0);
}

static int
lan887x_1000M_setup(struct mii_softc *phy)
{
	static const struct lan887x_regwr_map phy_cfg[] = {
		{ CL45_MMD_PMAPMD, LAN887X_TX_AMPLT_1000T1_REG, 0x003f },
		{ CL45_MMD_PMAPMD, LAN887X_AFE_PORT_TESTBUS_CTRL4, 0x00b8 },
	};
	int ret;

	SLOG_INF("1000M_setup");

	/* (Re)configure the speed/mode dependent T1 settings */
	ret = lan887x_load_values(phy, phy_cfg,
	    (sizeof(phy_cfg) / sizeof((phy_cfg)[0])));
	if (ret < 0)
		return ret;

	lan887x_set_bits_mmd(phy, CL45_MMD_PMAPMD, LAN887X_DSP_PMA_CONTROL,
	    LAN887X_DSP_PMA_CONTROL_LNK_SYNC);

	return (0);
}

static int
lan887x_link_setup(struct mii_softc *sc, u_int speed, bool is_master)
{
	if (speed == IFM_1000_T1)
		return lan887x_1000M_setup(sc);
	else if (speed == IFM_100_T1)
		return lan887x_100M_setup(sc, is_master);

	return -EINVAL;
}

/* LAN887X Errata: speed configuration changes require soft reset
 * and chip soft reset
 */
static int
lan887x_phy_reset(struct mii_softc *phy)
{
	uint16_t val = 0;
	int rc;

	SLOG_INF("phy_reset");

	/* Clear aneg */
	lan887x_clear_bits_mmd(phy, CL45_MMD_AN, LAN887X_AN_CTRL,
	    (LAN887X_AN_CTRL1_ENABLE | LAN887X_AN_CTRL1_RESTART));

	/* Clear 1000M link sync */
	lan887x_clear_bits_mmd(phy, CL45_MMD_PMAPMD, LAN887X_DSP_PMA_CONTROL,
	    LAN887X_DSP_PMA_CONTROL_LNK_SYNC);

	/* Clear 100M link sync */
	lan887x_clear_bits_mmd(phy, CL45_MMD_VEND1, LAN887X_REG_REG26,
	    LAN887X_REG_REG26_HW_INIT_SEQ_EN);

	/* Chiptop soft-reset to allow the speed/mode change */
	lan887x_write_mmd(phy, CL45_MMD_VEND1, LAN887X_CHIP_SOFT_RST,
	    LAN887X_CHIP_SOFT_RST_RESET);

	/* CL22 soft-reset to let the link re-train */
	lan887x_modify(phy, MII_BMCR, BMCR_RESET, BMCR_RESET);

	rc = lan887x_poll_reg(phy, MII_BMCR, &val, BMCR_RESET, 0U, 100U, 10U);
	if (rc != EOK) {
		SLOG_ERR("phy_reset pending!(0x%x)", val);
	} else {
		SLOG_INF("phy_reset complete!");
	}

	return (0);
}

static int
lan887x_phy_cfg_setup(struct mii_softc *sc)
{
	struct lan887x_softc *phy = (struct lan887x_softc *)sc;
	int ret;

	ret = lan887x_phy_setup(sc);
	if (ret) {
		SLOG_ERR("PHY setup failed, error: %d\n", ret);
		return (ret);
	}
	SLOG_DBG("PHY setup complete\n");

	ret = lan887x_phy_init(phy);
	if (ret) {
		SLOG_ERR("PHY init failed, error: %d\n", ret);
		return (ret);
	}
	SLOG_DBG("PHY init complete\n");

	return (0);
}

static int
lan887x_phy_auto_cfg(struct mii_softc *sc)
{
	struct lan887x_softc *phy = (struct lan887x_softc *)sc;
	uint16_t adv_m = 0;
	uint16_t adv_l = 0;
	uint32_t val = 0;
	int ret;

	//Reset PHY configuration
	ret = lan887x_phy_reset(sc);
	if (ret < 0)
		return ret;

	phy->sts.aneg = 1;

	adv_m = lan887x_read_mmd(sc, CL45_MMD_AN, LAN887X_AN_ADV_M);
	adv_l = lan887x_read_mmd(sc, CL45_MMD_AN, LAN887X_AN_ADV_L);

	//Default Master mode
	adv_l &= ~LAN887X_AN_ADV_L_FORCE_MS;
	adv_m &= ~LAN887X_AN_ADV_M_MST;
	if (phy->cfg.master) {
		SLOG_INF("auto_cfg preferred master");
		adv_m |= LAN887X_AN_ADV_M_MST;
	}

	/* Ref. 802.3-2022 : Section 45.2.7.22
	 * The Base Page value is transferred to mr_adv_ability when register
	 * 7.514 is written.
	 * Therefore, registers 7.515 and 7.516 should be written before 7.514.
	 */
	adv_m &= ~(LAN887X_AN_ADV_M_1000BT1 | LAN887X_AN_ADV_M_100BT1);
	if ((sc->mii_extcapabilities & IFM_1000_T1) && phy->cfg.speed_1000) {
		adv_m |= LAN887X_AN_ADV_M_1000BT1;
		SLOG_INF("auto_cfg 1000M");
	} else if (!phy->cfg.speed_1000) {
		adv_m |= LAN887X_AN_ADV_M_100BT1;
		SLOG_INF("auto_cfg 100M");
	} else {
		SLOG_INF("auto_cfg invalid speed");
		return (EINVAL);
	}
	lan887x_write_mmd(sc, CL45_MMD_AN, LAN887X_AN_ADV_M, adv_m);

	lan887x_write_mmd(sc, CL45_MMD_AN, LAN887X_AN_ADV_L, adv_l);

	ret = lan887x_link_setup(sc,
	    ((adv_m & LAN887X_AN_ADV_M_1000BT1) ? IFM_1000_T1 : IFM_100_T1),
		phy->cfg.master);
	if (ret < 0)
		return ret;

	lan887x_set_bits_mmd(sc, CL45_MMD_AN, LAN887X_AN_CTRL,
	    (LAN887X_AN_CTRL1_ENABLE | LAN887X_AN_CTRL1_RESTART));

	ret = lan887x_poll_mmd_reg(sc, CL45_MMD_AN, LAN887X_AN_CTRL, &val,
							   LAN887X_AN_CTRL1_RESTART, 0U, 200U, 100U);
	if (ret != EOK) {
		SLOG_ERR("auto_cfg reset pending!(0x%x)", val);
	} else {
		SLOG_INF("auto_cfg reset complete!");
	}

	return (0);
}

static int
lan887x_phy_read_aneg_link_sts(struct mii_softc *sc)
{
	struct lan887x_softc *phy = (struct lan887x_softc *)sc;
	struct mii_data *mii = sc->mii_pdata;
	uint32_t val = 0;
	int rc;

	mii->mii_media_active |= IFM_AUTO;

	/* latch-low */
	val = (lan887x_read_mmd(sc, CL45_MMD_AN, LAN887X_AN_STAT) |
		   lan887x_read_mmd(sc, CL45_MMD_AN, LAN887X_AN_STAT));
	if ((val & LAN887X_STAT1_ACOMP) &&
		(val & LAN887X_STAT1_LSTATUS)) {
		mii->mii_media_status |= IFM_ACTIVE;
		phy->sts.link = 1;
		SLOG_INF("auto_sts Link Up!");
	} else {
		SLOG_INF("auto_sts Link Down!(0x%x)", val);
	}

	/* resolve lp caps */
	rc = lan887x_read_mmd(sc, CL45_MMD_AN, LAN887X_AN_LP_L);
	if (rc < 0)
		return rc;

	rc = lan887x_read_mmd(sc, CL45_MMD_AN, LAN887X_AN_LP_M);
	if (rc < 0)
		return rc;

	if ((sc->mii_extcapabilities & IFM_1000_T1) && phy->cfg.speed_1000 &&
		(rc & LAN887X_AN_ADV_M_1000BT1)) {
			SLOG_INF("auto_sts speed 1000M!");
			mii->mii_media_active |= IFM_1000_T1;
			phy->sts.speed = 1000;
	} else if (!phy->cfg.speed_1000 && (rc & LAN887X_AN_ADV_M_100BT1)) {
				SLOG_INF("auto_sts speed 100M!");
				mii->mii_media_active |= IFM_100_T1;
				phy->sts.speed = 100;
	} else {
		SLOG_ERR("auto_sts invalid speed!(0x%x)", rc);
		return (EINVAL);
	}

	/* Fetch resolved mode */
	rc = lan887x_read_mmd(sc, CL45_MMD_AN, LAN887X_VEND_CTRL_STAT_REG);
	if (rc < 0)
		return rc;

	if (rc & LAN887X_AN_LOCAL_MASTER) {
		mii->mii_media_active |= IFM_ETH_MASTER;
		phy->sts.master = 1;
		SLOG_INF("auto_sts Master mode!");
	} else if (rc & LAN887X_AN_LOCAL_SLAVE) {
		phy->sts.master = 0;
		SLOG_INF("auto_sts Slave mode!");
	} else {
		SLOG_ERR("auto_sts invalid mode!(0x%x)", rc);
		return (EINVAL);
	}

	//T1 Full duplex ONLY
	mii->mii_media_active |= IFM_FDX;

	return (0);
}

static int
lan887x_phy_pma_cfg(struct mii_softc *sc, struct ifmedia_entry *ife)
{
	struct lan887x_softc *phy = (struct lan887x_softc *)sc;
	uint16_t ctl = 0;
	uint16_t mask;
	int ret;

	phy->sts.aneg = 0;
	phy->sts.master = 0;

	//Reset PHY configuration
	ret = lan887x_phy_reset(sc);
	if (ret < 0)
		return ret;

	//Mode setting
	mask = LAN8X8X_BT1_CTRL_CFG_MST;
	if ((ife->ifm_media & IFM_ETH_MASTER) != 0) {
		ctl |= LAN8X8X_BT1_CTRL_CFG_MST;
		SLOG_INF("pma_cfg master");
		phy->sts.master = 1;
	}

	//speed setup
	mask |= LAN8X8X_BT1_CTRL_STRAP;
	if ((sc->mii_extcapabilities & IFM_1000_T1) && phy->cfg.speed_1000) {
		phy->sts.speed = 1000;
		ctl |= LAN8X8X_BT1_CTRL_STRAP_B1000;
		SLOG_INF("pma_cfg 1000M");
	} else if (!phy->cfg.speed_1000) {
		phy->sts.speed = 100;
		SLOG_INF("pma_cfg 100M");
	} else {
		SLOG_INF("pma_cfg invalid speed");
		return (EINVAL);
	}

	lan887x_modify_mmd(sc, CL45_MMD_PMAPMD, LAN8X8X_BT1_CTRL, mask, ctl);

	return lan887x_link_setup(sc, IFM_SUBTYPE(ife->ifm_media),
	    (((ife->ifm_media & IFM_ETH_MASTER) != 0) ? true : false));
}

/*
 * All internal APIs must be written above
 */
static int
lan887x_service(struct mii_softc *sc, struct mii_data *mii, int cmd)
{
	struct lan887x_softc *phy = (struct lan887x_softc *)sc;
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;

	SLOG_INF("service cmd(%d)", cmd);
	switch (cmd) {
	case MII_POLLSTAT:
		break;
	case MII_MEDIACHG:
		if ((if_getflags(mii->mii_ifp) & IFF_UP) == 0)
			break;

		switch (IFM_SUBTYPE(ife->ifm_media)) {
		case IFM_AUTO:
			lan887x_phy_auto_cfg(sc);
			break;
		default:
			mii_phy_setmedia(sc);
			break;
		}
		break;
	case MII_TICK:
		/*
		 * Only used for autonegotiation.
		 */
		if (IFM_SUBTYPE(ife->ifm_media) != IFM_AUTO) {
			sc->mii_ticks = 0;
			break;
		}

		if (phy->sts.link) {
			SLOG_INF("auto_cfg Link Up!");
			sc->mii_ticks = 0;
			return (0);
		}

		if (++sc->mii_ticks <= MII_ANEGTICKS_GIGE)
			break;

		sc->mii_ticks = 0;
		PHY_RESET(sc);
		lan887x_phy_auto_cfg(sc);
		break;
	}

	PHY_STATUS(sc);

	mii_phy_update(sc, cmd);
	return (0);
}

static void
lan887x_reset(struct mii_softc *sc)
{
	lan887x_phy_reset(sc);
}

static void
lan887x_strap_read(struct mii_softc *sc)
{
	struct lan887x_softc *phy = (struct lan887x_softc *)sc;
	int strap_sts;
	int rc;

	SLOG_INF("Strap Read");

	rc = lan887x_read_mmd(sc, CL45_MMD_VEND1, LAN887X_MX_SKU_DBG_STS);
	strap_sts = LAN887X_MX_SKU_STRAP_STS_GET(rc);

	phy->cfg.speed_1000 = 0; //100M
	if ((strap_sts & LAN887X_SKU_STRAP_SPEED) &&
		(sc->mii_extcapabilities & IFM_1000_T1))
		phy->cfg.speed_1000 = 1;

	phy->cfg.master = 0; //slave
	if (strap_sts & LAN887X_SKU_STRAP_MODE)
		phy->cfg.master = 1;

	phy->cfg.aneg = 0; //disabled
	if (strap_sts & LAN887X_SKU_STRAP_ANEG)
		phy->cfg.aneg = 1;

	phy->cfg.contype = MII_CONTYPE_RGMII_RXID;
}

static int
lan887x_probe(device_t dev)
{
	return (mii_phy_dev_probe(dev, lan887xphys, BUS_PROBE_DEFAULT));
}

#ifdef FDT
static void
lan887x_fdt_get_config(struct lan887x_softc *phy)
{
	mii_fdt_phy_config_t *cfg;
	uint8_t rx_delay_en = 0, tx_delay_en = 0;
	uint8_t sgmii_en = 0;
	pcell_t val;

	cfg = mii_fdt_get_config(phy->dev);
	if (OF_getencprop(cfg->phynode, "sgmii_en", &val, sizeof(val)) > 0)
		sgmii_en = !!(val);
	if (sgmii_en) {
		phy->cfg.contype = MII_CONTYPE_SGMII;
	} else {
		if (OF_getencprop(cfg->phynode, "rx_delay_en", &val, sizeof(val)) > 0)
			rx_delay_en = !!(val);
		if (OF_getencprop(cfg->phynode, "tx_delay_en", &val, sizeof(val)) > 0)
			tx_delay_en = !!(val);
		phy->cfg.contype = MII_CONTYPE_RGMII;
		if (rx_delay_en)
			phy->cfg.contype = MII_CONTYPE_RGMII_RXID;
		if (tx_delay_en)
			phy->cfg.contype = MII_CONTYPE_RGMII_TXID;
		if (tx_delay_en && rx_delay_en)
			phy->cfg.contype = MII_CONTYPE_RGMII_ID;
	}
	if (OF_getencprop(cfg->phynode, "autoneg", &val, sizeof(val)) > 0)
		phy->cfg.aneg = !!(val);
	if (OF_getencprop(cfg->phynode, "speed", &val, sizeof(val)) > 0)
		phy->cfg.speed_1000 = !!(val);
	if (OF_getencprop(cfg->phynode, "primary", &val, sizeof(val)) > 0)
		phy->cfg.master = !!(val);
	mii_fdt_free_config(cfg);
}
#endif

static int
lan887x_attach(device_t dev)
{
	struct lan887x_softc *phy = device_get_softc(dev);
	struct mii_softc *mii_sc = &phy->mii_sc;
	int ret;

	(void)device_printf(dev, "lan887x_attach\n");

	phy->dev = dev;

	mii_sc->mii_flags &= ~MIIF_NOISOLATE;
	//Supports Gigabit Ethernet functionality
	mii_sc->mii_flags |= (MIIF_HAVE_GTCR | MIIF_NOMANPAUSE | MIIF_FORCEANEG);

	mii_phy_dev_attach(dev, mii_sc->mii_flags, &lan887x_funcs, 0);

	PHY_RESET(mii_sc);

	//Add sysctl params
	lan887x_add_sysctl(dev);
	load_cfg_sysctl_overrides(phy);

	ret = lan887x_phy_cfg_setup(mii_sc);
	if (ret < 0)
		return ret;

	mii_sc->mii_capabilities = 0;
	mii_sc->mii_extcapabilities = 0;

	(void)device_printf(dev,
	    "lan887x_attach: mii_sc->mii_mpd_oui=%x mii_sc->mii_mpd_model=%x\n",
	    mii_sc->mii_mpd_oui, mii_sc->mii_mpd_model);

	mii_sc->mii_capabilities = lan887x_read_mmd(mii_sc, CL45_MMD_PMAPMD, CL45_STAT2);
	mii_sc->mii_capmask = CL45_PMA_STAT2_EXTABLE;
	if (mii_sc->mii_capabilities & CL45_PMA_STAT2_EXTABLE) {
		ret = lan887x_read_mmd(mii_sc, CL45_MMD_PMAPMD, CL45_PMA_EXTREG_BASET1);
		if (ret < 0)
			return (ret);

		if (ret & CL45_PMA_EXTREG_BASET1_B1000_ABLE) {
			SLOG_INF("PHY Extended cap 1000M_ABLE\n");
			mii_sc->mii_extcapabilities = IFM_1000_T1;
		}
		if (ret & CL45_PMA_EXTREG_BASET1_B100_ABLE) {
			SLOG_INF("PHY Extended cap 100M_ABLE\n");
			mii_sc->mii_extcapabilities |= IFM_100_T1;
		}
	}

	/* Save tunable values before strap/FDT can overwrite cfg */
	bool const tune_master = phy->cfg.master;
	bool const tune_speed_1000 = phy->cfg.speed_1000;
	bool const tune_aneg = phy->cfg.aneg;

#ifdef FDT
	lan887x_fdt_get_config(phy);
#else
	/* Jumper/dipswitch selects media */
	lan887x_strap_read(mii_sc);
#endif

	if (phy->strap_override) {
		/* Forced speed/mode configurations
		 * Ex-1: 1000/slave
		 * 		phy->cfg.master = 1;
		 * 		phy->cfg.speed_1000 =  1;
		 *
		 * Ex-2: 100/master
		 * 		phy->cfg.master = 1;
		 * 		phy->cfg.speed_1000 =  0;
		 *
		 * Ex-3: 100/slave
		 * 		phy->cfg.master = 0;
		 * 		phy->cfg.speed_1000 =  0;
		 */
		phy->cfg.master = tune_master;
		phy->cfg.speed_1000 = tune_speed_1000;
		phy->cfg.aneg = tune_aneg;
	}

	SLOG_INF("phy config: autoneg: %s, speed: %d, mode: %s",
	    phy->cfg.aneg ? "on" : "off", phy->cfg.speed_1000 ? 1000 : 100,
	    phy->cfg.master ? "master" : "slave");

	mii_sc->mii_pdata->mii_media_active |= (IFM_ETHER | IFM_FDX);
	if (phy->cfg.aneg) {
		mii_sc->mii_pdata->mii_media_active |= IFM_AUTO;
	} else {
		if (phy->cfg.master)
			mii_sc->mii_pdata->mii_media_active |= IFM_ETH_MASTER;
		if (phy->cfg.speed_1000)
			mii_sc->mii_pdata->mii_media_active |= IFM_1000_T1;
		else
			mii_sc->mii_pdata->mii_media_active |= IFM_100_T1;
	}
	ifmedia_add(&mii_sc->mii_pdata->mii_media, mii_sc->mii_pdata->mii_media_active, 0, NULL);
	ifmedia_set(&mii_sc->mii_pdata->mii_media, mii_sc->mii_pdata->mii_media_active);

	if (phy->cfg.aneg) {
		lan887x_phy_auto_cfg(mii_sc);
	} else {
		lan887x_phy_pma_cfg(mii_sc, mii_sc->mii_pdata->mii_media.ifm_cur);
	}

	MIIBUS_MEDIAINIT(mii_sc->mii_dev);

	return (0);
}

static void
lan887x_status(struct mii_softc *sc)
{
	struct mii_data *mii = sc->mii_pdata;
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;
	struct lan887x_softc *phy = (struct lan887x_softc *)sc;
	int val;

	SLOG_INF("status");

	mii->mii_media_status = IFM_AVALID;
	mii->mii_media_active = IFM_ETHER;
	phy->sts.link = 0;

	if (phy->cfg.aneg) {
		lan887x_phy_read_aneg_link_sts(sc);
		SLOG_INF("AUTO Link change to %s",
			(mii->mii_media_status & IFM_ACTIVE ? "up" : "down"));
	} else {
		//latch-low register
		val = (lan887x_read_mmd(sc, CL45_MMD_PMAPMD, CL45_STAT1) |
			   lan887x_read_mmd(sc, CL45_MMD_PMAPMD, CL45_STAT1));
		if (val & CL45_STAT1_LSTATUS) {
			mii->mii_media_status |= IFM_ACTIVE;
			phy->sts.link = 1;
		}
		SLOG_INF("PMA Link change to %s",
			(mii->mii_media_status & IFM_ACTIVE ? "up" : "down"));

		 mii->mii_media_active = ife->ifm_media;
	}
}

/* Compare block to sort in ascending order */
static int
lan887x_cmp_sqi(const void *a, const void *b)
{
	return *(const uint16_t *)a - *(const uint16_t *)b;
}

static int
lan887x_sqi_100(struct lan887x_softc *phy)
{
	struct mii_softc *sc = &phy->mii_sc;
	uint16_t rawtable[SQI_SAMPLES];
	uint32_t sqiavg = 0;
	int sqinum = 0;
	int rc, i;

	/* Configuration of SQI 100M */
	lan887x_write_mmd(sc, CL45_MMD_VEND1,
	    LAN887X_COEFF_PWR_DN_CONFIG_100,
	    LAN887X_COEFF_PWR_DN_CONFIG_100_V);

	lan887x_write_mmd(sc, CL45_MMD_VEND1, LAN887X_SQI_CONFIG_100,
	    LAN887X_SQI_CONFIG_100_V);

	rc = lan887x_read_mmd(sc, CL45_MMD_VEND1, LAN887X_SQI_CONFIG_100);
	if (rc != LAN887X_SQI_CONFIG_100_V)
		return -EINVAL;

	lan887x_modify_mmd(sc, CL45_MMD_VEND1, LAN887X_POKE_PEEK_100,
	    LAN887X_POKE_PEEK_100_EN,
	    LAN887X_POKE_PEEK_100_EN);

	/* Link check before raw readings */
	lan887x_status(sc);

	if (!phy->sts.link)
		return -ENETDOWN;

	/* Get 200 SQI raw readings */
	for (i = 0; i < SQI_SAMPLES; i++) {
		lan887x_write_mmd(sc, CL45_MMD_VEND1,
		    LAN887X_POKE_PEEK_100,
		    LAN887X_POKE_PEEK_100_EN);

		rc = lan887x_read_mmd(sc, CL45_MMD_VEND1,
		    LAN887X_SQI_MSE_100);
		if (rc < 0)
			return rc;

		rawtable[i] = (uint16_t)rc;
	}

	/* Link check after raw readings */
	lan887x_status(sc);

	if (!phy->sts.link)
		return -ENETDOWN;

	/* Sort SQI raw readings in ascending order */
	qsort(rawtable, SQI_SAMPLES, sizeof(uint16_t), lan887x_cmp_sqi);

	/* Keep inliers and discard outliers */
	for (i = SQI_INLIERS_START; i < SQI_INLIERS_END; i++)
		sqiavg += rawtable[i];

	/* Handle invalid samples */
	if (sqiavg != 0) {
		/* Get SQI average */
		sqiavg /= SQI_INLIERS_NUM;

		if (sqiavg < 75)
			sqinum = 7;
		else if (sqiavg < 94)
			sqinum = 6;
		else if (sqiavg < 119)
			sqinum = 5;
		else if (sqiavg < 150)
			sqinum = 4;
		else if (sqiavg < 189)
			sqinum = 3;
		else if (sqiavg < 237)
			sqinum = 2;
		else if (sqiavg < 299)
			sqinum = 1;
		else
			sqinum = 0;
	}

	return sqinum;
}

static int
lan887x_sqi(struct lan887x_softc *phy)
{
	struct mii_softc *sc = &phy->mii_sc;
	int sqi = 0;
	int val;

	if (phy->sts.link) {
		if (phy->sts.speed == 100)
			return lan887x_sqi_100(phy);

		/* DCQ_COEFF_EN to trigger a SQI read */
		lan887x_write_mmd(sc, CL45_MMD_VEND1, LAN887X_COEFF_MOD_CONFIG,
		    LAN887X_COEFF_MOD_CONFIG_DCQ_COEFF_EN);

		/* Writing DCQ_COEFF_EN to trigger a SQI read */
		val = lan887x_read_mmd(sc, CL45_MMD_VEND1, LAN887X_COEFF_MOD_CONFIG);
		if ((val & LAN887X_COEFF_MOD_CONFIG_DCQ_COEFF_EN) == 0) {
			val = lan887x_read_mmd(sc, CL45_MMD_VEND1, LAN887X_DCQ_SQI_STATUS);
			sqi = LAN8X8X_SQI_GET(val);
		}
	}

	return (sqi);
}

static int lan887x_cd_hw_reset(struct lan887x_softc *phy,
                            enum lan887x_cd_state cd_done)
{
	uint32_t val = 0;
	int rc;

	/* Chip hard-reset */
	lan887x_write_mmd(&phy->mii_sc, CL45_MMD_VEND1, LAN887X_CHIP_HARD_RST,
					   LAN887X_CHIP_HARD_RST_RESET);

	/* Wait for reset to complete */
	rc = lan887x_poll_mmd_reg(&phy->mii_sc, CL45_MMD_VEND1, LAN887X_CHIP_HARD_RST, &val,
			LAN887X_CHIP_HARD_RST_RESET, 0U, 200U, 100U);
	if (rc != EOK) {
			SLOG_ERR("cd_test hard_reset pending!(0x%x)", val);
	} else {
			SLOG_INF("cd_test hard_reset done!");
	}

	if (cd_done == LAN887X_CD_STATE_DONE) {
		SLOG_INF("cd_test done");

		lan887x_phy_cfg_setup(&phy->mii_sc);

		/* Cable diagnostics complete. Restore PHY. */
		if (phy->cfg.aneg) {
			lan887x_phy_auto_cfg(&phy->mii_sc);
		} else {
			lan887x_phy_pma_cfg(&phy->mii_sc, phy->mii_sc.mii_pdata->mii_media.ifm_cur);
		}
	}

	return 0;
}

static int lan887x_cable_test_start(struct lan887x_softc *phy,
                                   	enum lan887x_cd_mode mode)
{
        static const struct lan887x_regwr_map values[] = {
                {CL45_MMD_VEND1, LAN887X_MAX_PGA_GAIN_100, 0x1f},
                {CL45_MMD_VEND1, LAN887X_MIN_PGA_GAIN_100, 0x0},
                {CL45_MMD_VEND1, LAN887X_CBL_DIAG_TDR_THRESH_100, 0x1},
                {CL45_MMD_VEND1, LAN887X_CBL_DIAG_AGC_THRESH_100, 0x3c},
                {CL45_MMD_VEND1, LAN887X_CBL_DIAG_MIN_WAIT_CONFIG_100, 0x0},
                {CL45_MMD_VEND1, LAN887X_CBL_DIAG_MAX_WAIT_CONFIG_100, 0x46},
                {CL45_MMD_VEND1, LAN887X_CBL_DIAG_CYC_CONFIG_100, 0x5a},
                {CL45_MMD_VEND1, LAN887X_CBL_DIAG_TX_PULSE_CONFIG_100, 0x44d5},
                {CL45_MMD_VEND1, LAN887X_CBL_DIAG_MIN_PGA_GAIN_100, 0x0},

        };
	uint32_t arr_sz;
	uint32_t val = 0;
        int rc;

        rc = lan887x_cd_hw_reset(phy, LAN887X_CD_STATE_INIT);
        if (rc != EOK)
                return rc;

        /* Forcing DUT to master mode, as we don't care about
         * mode during diagnostics
         */
        lan887x_write_mmd(&phy->mii_sc, CL45_MMD_PMAPMD, LAN8X8X_BT1_CTRL,
                           LAN8X8X_BT1_CTRL_CFG_MST);

        lan887x_write_mmd(&phy->mii_sc, CL45_MMD_PMAPMD, 0x80b0, 0x0038);

        lan887x_modify_mmd(&phy->mii_sc, CL45_MMD_VEND1,
                            LAN887X_CALIB_CONFIG_100, 0,
                            LAN887X_CALIB_CONFIG_100_VAL);

	arr_sz = (sizeof(values) / sizeof((values)[0]));
		for (uint32_t i = 0; i < arr_sz; i++) {
			lan887x_write_mmd(&phy->mii_sc, values[i].mmd, values[i].reg,
							   values[i].val);

			if (mode &&
				values[i].reg == LAN887X_CBL_DIAG_MAX_WAIT_CONFIG_100) {
					lan887x_write_mmd(&phy->mii_sc, values[i].mmd,
									   values[i].reg, 0xa);
			}
		}

		if (mode == LAN887X_CD_MODE_HYBRID) {
			lan887x_modify_mmd(&phy->mii_sc, CL45_MMD_PMAPMD,
								LAN887X_AFE_PORT_TESTBUS_CTRL4,
								BIT(0), BIT(0));
		}

		/* HW_INIT 100T1, Get DUT running in 100T1 mode */
		lan887x_modify_mmd(&phy->mii_sc, CL45_MMD_VEND1, LAN887X_REG_REG26,
							LAN887X_REG_REG26_HW_INIT_SEQ_EN,
							LAN887X_REG_REG26_HW_INIT_SEQ_EN);

		/* Cable diag requires hard reset and is sensitive regarding the delays.
		 * Hard reset is expected into and out of cable diag.
		 * Wait for 50ms
		 */
		//msleep(50);
		lan887x_poll_mmd_reg(&phy->mii_sc, CL45_MMD_VEND1, LAN887X_REG_REG26, &val,
				LAN887X_REG_REG26_HW_INIT_SEQ_EN, LAN887X_REG_REG26_HW_INIT_SEQ_EN, 200U, 100U);

		/* Start cable diag */
		lan887x_write_mmd(&phy->mii_sc, CL45_MMD_VEND1,
						   LAN887X_START_CBL_DIAG_100,
						   LAN887X_CBL_DIAG_START);

		SLOG_INF("cd_test started!");

		return (EOK);
}

static int lan887x_cable_test_chk(struct lan887x_softc *phy,
                                  enum lan887x_cd_mode mode)
{
	uint32_t val = 0;
	int rc;

	rc= lan887x_poll_mmd_reg(&phy->mii_sc, CL45_MMD_VEND1, LAN887X_START_CBL_DIAG_100, &val,
						   LAN887X_CBL_DIAG_DONE, LAN887X_CBL_DIAG_DONE, 500000U, 1000U);
	if (rc != EOK) {
		   SLOG_INF("cd_test %s pending!(0x%x)",
				    (mode == LAN887X_CD_MODE_HYBRID ? "hybrid" : "normal"), val);
		   return (rc);
	}

	SLOG_INF("cd_test %s complete!",
		     (mode == LAN887X_CD_MODE_HYBRID ? "hybrid" : "normal"));

	/* Stop cable diag */
	lan887x_write_mmd(&phy->mii_sc, CL45_MMD_VEND1,
						 LAN887X_START_CBL_DIAG_100,
						 LAN887X_CBL_DIAG_STOP);
	return (EOK);
}

static int
lan887x_phy_cd_start(struct lan887x_softc *phy)
{
        int rc, ret;

        rc = lan887x_cable_test_start(phy, LAN887X_CD_MODE_NORMAL);
        if (rc != EOK) {
        	SLOG_ERR("cd_test failed to start!");
			ret = lan887x_cd_hw_reset(phy, LAN887X_CD_STATE_DONE);
			if (ret != EOK)
					return ret;
			return rc;
        }

        return 0;
}

static int lan887x_cable_test_report(struct lan887x_softc *phy)
{
        int pos_peak_cycle, pos_peak_cycle_hybrid, pos_peak_in_phases;
        int pos_peak_time, pos_peak_time_hybrid, neg_peak_time;
        int neg_peak_cycle, neg_peak_in_phases;
        int pos_peak_in_phases_hybrid;
        int gain_idx, gain_idx_hybrid;
        int pos_peak_phase_hybrid;
        int pos_peak, neg_peak;
        int detect;
        int distance = -1;
        int ret;
        int rc;

        SLOG_INF("cd_test reporting!");

        /* Read non-hybrid results */
        gain_idx = lan887x_read_mmd(&phy->mii_sc, CL45_MMD_VEND1,
                                LAN887X_CBL_DIAG_AGC_GAIN_100);
        if (gain_idx < 0) {
                rc = gain_idx;
                goto error;
        }

        pos_peak = lan887x_read_mmd(&phy->mii_sc, CL45_MMD_VEND1,
                                LAN887X_CBL_DIAG_POS_PEAK_VALUE_100);
        if (pos_peak < 0) {
                rc = pos_peak;
                goto error;
        }

        neg_peak = lan887x_read_mmd(&phy->mii_sc, CL45_MMD_VEND1,
                                LAN887X_CBL_DIAG_NEG_PEAK_VALUE_100);
        if (neg_peak < 0) {
                rc = neg_peak;
                goto error;
        }

        pos_peak_time = lan887x_read_mmd(&phy->mii_sc, CL45_MMD_VEND1,
                                     LAN887X_CBL_DIAG_POS_PEAK_TIME_100);
        if (pos_peak_time < 0) {
                rc = pos_peak_time;
                goto error;
        }

        neg_peak_time = lan887x_read_mmd(&phy->mii_sc, CL45_MMD_VEND1,
                                     LAN887X_CBL_DIAG_NEG_PEAK_TIME_100);
        if (neg_peak_time < 0) {
                rc = neg_peak_time;
                goto error;
        }

        /* Calculate non-hybrid values */
        pos_peak_cycle = (pos_peak_time >> 7) & 0x7f;
        pos_peak_in_phases = (pos_peak_cycle * 96) + (pos_peak_time & 0x7f);
        neg_peak_cycle = (neg_peak_time >> 7) & 0x7f;
        neg_peak_in_phases = (neg_peak_cycle * 96) + (neg_peak_time & 0x7f);

        /* Deriving the status of cable */
        if (pos_peak > MICROCHIP_CABLE_NOISE_MARGIN &&
            neg_peak > MICROCHIP_CABLE_NOISE_MARGIN && gain_idx >= 0) {
                if (pos_peak_in_phases > neg_peak_in_phases &&
                    ((pos_peak_in_phases - neg_peak_in_phases) >=
                     MICROCHIP_CABLE_MIN_TIME_DIFF) &&
                    ((pos_peak_in_phases - neg_peak_in_phases) <
                     MICROCHIP_CABLE_MAX_TIME_DIFF) &&
                    pos_peak_in_phases > 0) {
                        detect = LAN887X_CABLE_TEST_SAME_SHORT;
                        SLOG_INF("cd_test status=CABLE_SHORT!");
                } else if (neg_peak_in_phases > pos_peak_in_phases &&
                           ((neg_peak_in_phases - pos_peak_in_phases) >=
                            MICROCHIP_CABLE_MIN_TIME_DIFF) &&
                           ((neg_peak_in_phases - pos_peak_in_phases) <
                            MICROCHIP_CABLE_MAX_TIME_DIFF) &&
                           neg_peak_in_phases > 0) {
                        detect = LAN887X_CABLE_TEST_OPEN;
                        SLOG_INF("cd_test status=CABLE_OPEN!");
                } else {
                        detect = LAN887X_CABLE_TEST_OK;
                        SLOG_INF("cd_test status=CABLE_OK!");
                }
        } else {
        	SLOG_INF("cd_test status=CABLE_OK! check pos_peak(%u), neg_peak(%u), gain_idx(%u)",
        			 pos_peak, neg_peak, gain_idx);
			detect = LAN887X_CABLE_TEST_OK;
        }

        if (detect == LAN887X_CABLE_TEST_OK) {
        	SLOG_INF("cd_test status=CABLE_OK!");
			distance = 0;
			goto get_len;
        }

        /* Re-initialize PHY and start cable diag test */
        rc = lan887x_cable_test_start(phy, LAN887X_CD_MODE_HYBRID);
        if (rc != EOK)
                goto cd_stop;

        /* Wait for cable diag test completion */
        rc = lan887x_cable_test_chk(phy, LAN887X_CD_MODE_HYBRID);
        if (rc != EOK)
                goto cd_stop;

        /* Read hybrid results */
        gain_idx_hybrid = lan887x_read_mmd(&phy->mii_sc, CL45_MMD_VEND1,
                                       LAN887X_CBL_DIAG_AGC_GAIN_100);
        if (gain_idx_hybrid < 0) {
                rc = gain_idx_hybrid;
                goto error;
        }

        pos_peak_time_hybrid = lan887x_read_mmd(&phy->mii_sc, CL45_MMD_VEND1,
                                            LAN887X_CBL_DIAG_POS_PEAK_TIME_100);
        if (pos_peak_time_hybrid < 0) {
                rc = pos_peak_time_hybrid;
                goto error;
        }

        SLOG_INF("cd_test hybrid test complete!");

        /* Calculate hybrid values to derive cable length to fault */
        pos_peak_cycle_hybrid = (pos_peak_time_hybrid >> 7) & 0x7f;
        pos_peak_phase_hybrid = pos_peak_time_hybrid & 0x7f;
        pos_peak_in_phases_hybrid = pos_peak_cycle_hybrid * 96 +
                                    pos_peak_phase_hybrid;

        /* Distance to fault calculation.
         * distance = (peak_in_phases - peak_in_phases_hybrid) *
         *             propagationconstant.
         * constant to convert number of phases to meters
         * propagationconstant = 0.015953
         *                       (0.6811 * 2.9979 * 156.2499 * 0.0001 * 0.5)
         * Applying constant 1.5953 and further devides by 100 to
         * convert to meters.
         */
        if (detect == LAN887X_CABLE_TEST_OPEN) {
                distance = (((pos_peak_in_phases - pos_peak_in_phases_hybrid)
                             * 15953) / 10000);
        } else if (detect == LAN887X_CABLE_TEST_SAME_SHORT) {
                distance = (((neg_peak_in_phases - pos_peak_in_phases_hybrid)
                             * 15953) / 10000);
        } else {
                distance = 0;
        }

get_len:
		SLOG_INF("cd_test status(%d), distance(%d)!", detect, distance);

        rc = lan887x_cd_hw_reset(phy, LAN887X_CD_STATE_DONE);
        if (rc < 0)
                return rc;

        phy->cd_res.state = LAN887X_CD_STATE_DONE;
        phy->cd_res.len = ((uint32_t)distance & GENMASK(15, 0)); //in cm
        phy->cd_res.res[0]='\0';
        switch (detect) {
        case LAN887X_CABLE_TEST_OK:
        	strcpy(phy->cd_res.res, "ok");
        	break;
        case LAN887X_CABLE_TEST_OPEN:
        	strcpy(phy->cd_res.res, "open");
        	break;
        case LAN887X_CABLE_TEST_SAME_SHORT:
        	strcpy(phy->cd_res.res, "short");
        	break;
        default:
        	strcpy(phy->cd_res.res, "error");
        	break;
        }

        return 0;

cd_stop:
	SLOG_INF("cd_test stop!");
	/* Stop cable diag */
	lan887x_write_mmd(&phy->mii_sc, CL45_MMD_VEND1,
						LAN887X_START_CBL_DIAG_100,
						LAN887X_CBL_DIAG_STOP);

error:
	SLOG_INF("cd_test error!");
	/* Cable diag test failed */
	ret = lan887x_cd_hw_reset(phy, LAN887X_CD_STATE_DONE);
	if (ret < 0)
			return ret;

	/* Return error in failure case */
	return rc;
}

static int lan887x_cable_test_get_status(struct lan887x_softc *phy)
{
	int rc;

	SLOG_INF("cd_test get_status!");

	rc = lan887x_cable_test_chk(phy, LAN887X_CD_MODE_NORMAL);
	if (rc != EOK) {
		lan887x_cd_hw_reset(phy, LAN887X_CD_STATE_DONE);
		return rc;
	}

	SLOG_INF("cd_test get_report!");

	/* Retrieve test status and cable length to fault */
	return lan887x_cable_test_report(phy);
}


static int
lan887x_cd_test(SYSCTL_HANDLER_ARGS)
{
	struct lan887x_softc *phy = (struct lan887x_softc*) arg1;
	char buff[128];
	int rc;

	memset(buff, 0 ,sizeof(buff));

	if (phy->cd_res.state == LAN887X_CD_STATE_NONE) {
		phy->cd_res.state = LAN887X_CD_STATE_INIT;

		rc = lan887x_phy_cd_start(phy);
		if (rc != EOK) {
			SLOG_INF("sysctl_cd failed to start!");
			snprintf(buff, sizeof(buff) - 1, "failed to start");
		} else {
			SLOG_INF("sysctl_cd test initiated!");
			snprintf(buff, sizeof(buff) - 1, "test initiated");
		}
	} else if (phy->cd_res.state == LAN887X_CD_STATE_INIT) {
		SLOG_INF("sysctl_cd test started!");
		snprintf(buff, sizeof(buff) - 1, "test started");
	} else {
		SLOG_INF("sysctl_cd test not started!");
		snprintf(buff, sizeof(buff) - 1, "not started");
	}

	return SYSCTL_OUT(req, buff, strlen(buff));
}

static int
lan887x_cd_report(SYSCTL_HANDLER_ARGS)
{
	struct lan887x_softc *phy = (struct lan887x_softc*) arg1;
	char buff[128];
	int rc;

	memset(buff, 0 ,sizeof(buff));

	if (req->oldptr == NULL && phy->cd_res.state == LAN887X_CD_STATE_INIT) {
		rc = lan887x_cable_test_get_status(phy);
		if (rc != EOK) {
			SLOG_INF("sysctl_cd test test failed!");
			snprintf(buff, sizeof(buff) - 1, "test failed");
		} else {
			SLOG_INF("sysctl_cd test report pending!");
			snprintf(buff, sizeof(buff) - 1, "report pending");
		}
	} else if (phy->cd_res.state == LAN887X_CD_STATE_INIT) {
		SLOG_INF("sysctl_cd test report pending!");
		snprintf(buff, sizeof(buff) - 1, "test pending");
	} else if (phy->cd_res.state == LAN887X_CD_STATE_DONE) {
		SLOG_INF("sysctl_cd test complete!");
		snprintf(buff, sizeof(buff) - 1,
		"status(%s) distance(%dm %dcm)", phy->cd_res.res, (phy->cd_res.len/100), (phy->cd_res.len%100));
		phy->cd_res.state = LAN887X_CD_STATE_NONE;
	} else {
		snprintf(buff, sizeof(buff) - 1, "test not started");
	}
	SLOG_INF("sysctl_cd %s", buff);

	return SYSCTL_OUT(req, buff, sizeof(buff));
}

static void
lan887x_add_sysctl(device_t dev)
{
    struct lan887x_softc *phy = device_get_softc(dev);
    struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(dev);
    struct sysctl_oid *tree = device_get_sysctl_tree(dev);
    struct sysctl_oid_list *tree_node = SYSCTL_CHILDREN(tree);

    SYSCTL_ADD_STRING(ctx, tree_node, OID_AUTO, "drvr_version", CTLFLAG_RD, lan887x_drvr_version, 0, "LAN887X Version");
	SYSCTL_ADD_INT(ctx, tree_node, OID_AUTO, "version", CTLFLAG_RD, &drvr_ver, 0, "Version");
	SYSCTL_ADD_U16(ctx, tree_node, OID_AUTO, "speed", CTLFLAG_RD, &phy->sts.speed, 0, "Link Speed");
	SYSCTL_ADD_U8(ctx, tree_node, OID_AUTO, "link", CTLFLAG_RD, &phy->sts.link, 0, "Link Status");
	SYSCTL_ADD_U8(ctx, tree_node, OID_AUTO, "master", CTLFLAG_RD, &phy->sts.master, 0, "master(1)/slave(0)");
	SYSCTL_ADD_U8(ctx, tree_node, OID_AUTO, "autoneg", CTLFLAG_RD, &phy->sts.aneg, 0, "Autoneg");

    SYSCTL_ADD_PROC(ctx, tree_node, OID_AUTO, "sqi",
		    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE, phy, 0,
		    lan887x_sqi_handler, "CU", "SQI");

    SYSCTL_ADD_PROC(ctx, tree_node, OID_AUTO, "cd_test",
    		CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_NEEDGIANT | CTLFLAG_ANYBODY | CTLFLAG_SKIP, phy, 0,
		    lan887x_cd_test, "A", "Start Cable Diagnostics Test");

    SYSCTL_ADD_PROC(ctx, tree_node, OID_AUTO, "cd_report",
    		CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_NEEDGIANT | CTLFLAG_ANYBODY | CTLFLAG_SKIP, phy, 0,
		    lan887x_cd_report, "A", "Get Cable Diagnostics Report");
}

static void
load_cfg_sysctl_overrides(struct lan887x_softc *phy)
{
	struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(phy->dev);
	struct sysctl_oid *tree = device_get_sysctl_tree(phy->dev);
	struct sysctl_oid_list *child = SYSCTL_CHILDREN(tree);

	phy->strap_override = true;
	phy->cfg.speed_1000 = true;
	phy->cfg.master = true;
	phy->cfg.aneg = true;

	SYSCTL_ADD_BOOL(ctx, child, OID_AUTO, "strap_override",
	    CTLFLAG_RDTUN, &phy->strap_override, 0,
	    "Override hardware strap configuration");
	SYSCTL_ADD_BOOL(ctx, child, OID_AUTO, "speed_1000",
	    CTLFLAG_RDTUN, &phy->cfg.speed_1000, 0,
	    "Use 1000M speed (false = 100M)");
	SYSCTL_ADD_BOOL(ctx, child, OID_AUTO, "mode_master",
	    CTLFLAG_RDTUN, &phy->cfg.master, 0,
	    "Use master mode (false = slave)");
	SYSCTL_ADD_BOOL(ctx, child, OID_AUTO, "aneg_enable",
	    CTLFLAG_RDTUN, &phy->cfg.aneg, 0,
	    "Enable autonegotiation");
}
