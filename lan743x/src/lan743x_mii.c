/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023
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

#include "lan743x_mii.h"
#include "lan743x_ctx.h"
#include "lan743x_csr.h"
#include "lan743x_util.h"
#include <sys/types.h>
#include <net/iflib.h>
#include <errno.h>
#include <net/if_media.h> //needed by miivar.h
#include <dev/mii/miivar.h>
#include <sys/bus.h>
#include <miibus_if.h>
#include <sys/kernel.h>
#include <stddef.h>

#define LAN743X_MII_ACCESS 0x120U
#define LAN743X_MII_DATA 0x124U
#define LAN743X_MII_PHY_ADDR_MASK 0x1FU
#define LAN743X_MII_PHY_ADDR_SHIFT 11U
#define LAN743X_MII_REG_ADDR_MASK 0x1FU
#define LAN743X_MII_REG_ADDR_SHIFT 6U
#define LAN743X_MII_READ 0x0U
#define LAN743X_MII_WRITE 0x2U
#define LAN743X_MII_BUSY 0x1U

static int
lan743x_mii_access_wait(device_t dev, struct lan743x_softc const *sc)
{
	int const ret = lan743x_wait_for_bits(sc, LAN743X_MII_ACCESS, 0U,
	    LAN743X_MII_BUSY);
	if (ret == EBUSY) {
		(void)device_printf(dev,
		    "lan743x_mii_access_wait; mii timeout\n");
	}
	return ret;
}

int
lan743x_miibus_readreg(device_t dev, int phy, int reg)
{
	if_ctx_t ictx = (if_ctx_t)device_get_softc(dev);
	struct lan743x_softc *sc = iflib_get_softc(ictx);

	if (lan743x_mii_access_wait(dev, sc) == EBUSY) {
		return 0;
	}
	if (phy < 0) {
		(void)device_printf(dev,
		    "lan743x_miibus_readreg; Invalid phy value: %d\n", phy);
		return 0;
	}
	if (reg < 0) {
		(void)device_printf(dev,
		    "lan743x_miibus_readreg; Invalid reg value: %d\n", reg);
		return 0;
	}
	uint32_t mii_access = ((uint32_t)phy & LAN743X_MII_PHY_ADDR_MASK)
	    << LAN743X_MII_PHY_ADDR_SHIFT;
	mii_access |= ((uint32_t)reg & LAN743X_MII_REG_ADDR_MASK)
	    << LAN743X_MII_REG_ADDR_SHIFT;
	mii_access |= LAN743X_MII_BUSY | LAN743X_MII_READ;
	lan743x_csr_write_32(sc, LAN743X_MII_ACCESS, mii_access);
	if (lan743x_mii_access_wait(dev, sc) == EBUSY) {
		return 0;
	}
	return (int)(lan743x_csr_read_16(sc, LAN743X_MII_DATA));
}

int
lan743x_miibus_writereg(device_t dev, int phy, int reg, int data)
{
	if_ctx_t ictx = (if_ctx_t)device_get_softc(dev);
	struct lan743x_softc *sc = iflib_get_softc(ictx);

	if (phy < 0) {
		(void)device_printf(dev,
		    "lan743x_miibus_writereg; Invalid phy value: %d\n", phy);
		return 0;
	}
	if (reg < 0) {
		(void)device_printf(dev,
		    "lan743x_miibus_writereg; Invalid reg value: %d\n", reg);
		return 0;
	}
	if (data < 0) {
		(void)device_printf(dev,
		    "lan743x_miibus_writereg; Invalid data value: %d\n", data);
		return 0;
	}
	if (lan743x_mii_access_wait(dev, sc) == EBUSY) {
		return 0;
	}
	uint32_t mii_access = ((uint32_t)phy & LAN743X_MII_PHY_ADDR_MASK)
	    << LAN743X_MII_PHY_ADDR_SHIFT;
	mii_access |= ((uint32_t)reg & LAN743X_MII_REG_ADDR_MASK)
	    << LAN743X_MII_REG_ADDR_SHIFT;
	mii_access |= LAN743X_MII_BUSY | LAN743X_MII_WRITE;
	lan743x_csr_write_32(sc, LAN743X_MII_DATA, (uint32_t)data);
	lan743x_csr_write_32(sc, LAN743X_MII_ACCESS, mii_access);
	if (lan743x_mii_access_wait(dev, sc) == EBUSY) {
		return 0;
	}
	return 0;
}

static int
lan743x_get_link_state(struct mii_data const *miid)
{
	bool const ifm_avalid = lan743x_u32_to_bool(
	    miid->mii_media_status & IFM_AVALID);
	if (!ifm_avalid) {
		return LINK_STATE_DOWN;
	}
	bool const ifm_active = lan743x_u32_to_bool(
	    miid->mii_media_status & IFM_ACTIVE);
	if (!ifm_active) {
		return LINK_STATE_DOWN;
	}
	return LINK_STATE_UP;
}

static char const *
lan743x_get_link_state_str(int const link_state)
{
	switch (link_state) {
	case LINK_STATE_UP:
		return "UP";
	case LINK_STATE_DOWN:
		return "DOWN";
	default:
		return "UNKNOWN";
	}
}

void
lan743x_miibus_linkchg(device_t dev)
{
	struct lan743x_softc *sc = iflib_get_softc(device_get_softc(dev));
	struct mii_data *miid = device_get_softc(sc->miibus);
	int const link_state = lan743x_get_link_state(miid);
	(void)device_printf(sc->dev,
	    "lan743x_miibus_linkchg(); link state: %s\n",
	    lan743x_get_link_state_str(link_state));
	uint64_t const baudrate = ifmedia_baudrate((int)miid->mii_media_active);
	iflib_link_state_change(sc->ctx, link_state, baudrate);
}

void
lan743x_miibus_statchg(device_t dev)
{
	(void)device_printf(dev, "lan743x_miibus_statchg()\n");
}

#if 0  /* MIIBUS_DEBUG */
/* If MIIBUS debug stuff is in attach then order matters. Use below instead. */
DRIVER_MODULE_ORDERED(miibus, lan743x, miibus_driver, miibus_devclass, NULL, NULL,
    SI_ORDER_ANY);
#endif /* MIIBUS_DEBUG */
DRIVER_MODULE(miibus, lan743x, miibus_driver, miibus_devclass, NULL, NULL);
