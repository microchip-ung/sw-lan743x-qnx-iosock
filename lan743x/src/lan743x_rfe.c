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

#include "lan743x_rfe.h"
#include "lan743x_ctx.h"
#include "lan743x_csr.h"
#include "lan743x_reg_defs.h"
#include "lan743x_assert.h"
#include "lan743x_iflib.h"

#include <net/if_dl.h>
#include <net/ethernet.h>
#include <stdbool.h>
#include <errno.h>

/*
 * If true: No multicast filtering; pass all multicast packets
 * If false: Per-address multicast filtering
 */
#define LAN743X_RFE_FORCE_ALLMULTI true

static void
lan743x_rfe_perfect_filter_clear(struct lan743x_softc const *sc, uint32_t index)
{
	LAN743X_ASSERT(index < LAN743X_RFE_ADDR_FILT_NUM,
	    ("lan743x_rfe_perfect_filter_clear: index out of range: %u",
		index));
	lan743x_csr_write_32(sc, LAN743X_RFE_ADDR_FILT_HI(index), 0U);
	lan743x_csr_write_32(sc, LAN743X_RFE_ADDR_FILT_LO(index), 0U);
}

static void
lan743x_rfe_perfect_filter_set(struct lan743x_softc const *sc, uint32_t index,
    uint8_t const *mac_addr)
{
	LAN743X_ASSERT(index < LAN743X_RFE_ADDR_FILT_NUM,
	    ("lan743x_rfe_perfect_filter_set: index out of range: %u", index));

	/* Clear HI first to invalidate the entry while programming */
	lan743x_csr_write_32(sc, LAN743X_RFE_ADDR_FILT_HI(index), 0U);

	uint32_t const addr_lo = (uint32_t)mac_addr[0] |
	    ((uint32_t)mac_addr[1] << 8U) | ((uint32_t)mac_addr[2] << 16U) |
	    ((uint32_t)mac_addr[3] << 24U);
	uint32_t const addr_hi = (uint32_t)mac_addr[4] |
	    ((uint32_t)mac_addr[5] << 8U) | LAN743X_RFE_ADDR_FILT_HI_VALID;

	lan743x_csr_write_32(sc, LAN743X_RFE_ADDR_FILT_LO(index), addr_lo);
	lan743x_csr_write_32(sc, LAN743X_RFE_ADDR_FILT_HI(index), addr_hi);
}

static void
lan743x_rfe_perfect_filter_init(struct lan743x_softc const *sc)
{
	for (uint32_t i = 0U; i < LAN743X_RFE_ADDR_FILT_NUM; i++) {
		lan743x_rfe_perfect_filter_clear(sc, i);
	}
}

static void
lan743x_rfe_perfect_filter_enable(struct lan743x_softc const *sc, bool enable)
{
	if (enable) {
		lan743x_csr_set_bits_32(sc, LAN743X_RFE_CTL,
		    LAN743X_RFE_ALLOW_PERFECT_FILTER);
	} else {
		lan743x_csr_clr_bits_32(sc, LAN743X_RFE_CTL,
		    LAN743X_RFE_ALLOW_PERFECT_FILTER);
	}
}

static void
lan743x_rfe_broadcast_enable(struct lan743x_softc const *sc, bool enable)
{
	if (enable) {
		lan743x_csr_set_bits_32(sc, LAN743X_RFE_CTL,
		    LAN743X_RFE_ALLOW_BROADCAST);
	} else {
		lan743x_csr_clr_bits_32(sc, LAN743X_RFE_CTL,
		    LAN743X_RFE_ALLOW_BROADCAST);
	}
}

static void
lan743x_rfe_allmulti_enable(struct lan743x_softc const *sc, bool enable)
{
	if (enable) {
		lan743x_csr_set_bits_32(sc, LAN743X_RFE_CTL,
		    LAN743X_RFE_ALLOW_MULTICAST);
	} else {
		lan743x_csr_clr_bits_32(sc, LAN743X_RFE_CTL,
		    LAN743X_RFE_ALLOW_MULTICAST);
	}
}

void
lan743x_rfe_init(struct lan743x_softc const *sc)
{
	/* Write base RFE_CTL with checksum offload bits */
	lan743x_csr_write_32(sc, LAN743X_RFE_CTL,
	    LAN743X_RFE_ENABLE_IGMP_CSUM | LAN743X_RFE_ENABLE_ICMP_CSUM |
		LAN743X_RFE_ENABLE_L4_CSUM | LAN743X_RFE_ENABLE_L3_CSUM);

	/* Clear all filter entries */
	lan743x_rfe_perfect_filter_init(sc);

	/* Program slot 0 with the device MAC address */
	if_t ifp = iflib_get_ifp(sc->ctx);
	uint8_t const *mac = (uint8_t const *)IF_LLADDR(ifp);
	lan743x_rfe_perfect_filter_set(sc, 0U, mac);

	/* Enable perfect filter and broadcast. */
	lan743x_rfe_perfect_filter_enable(sc, true);
	lan743x_rfe_broadcast_enable(sc, true);
	/* When LAN743X_RFE_FORCE_ALLMULTI is
	 * true, all multicast is passed through; otherwise multicast filtering
	 * is configured in the ifdi_multi_set callback. */
	lan743x_rfe_allmulti_enable(sc, LAN743X_RFE_FORCE_ALLMULTI);
}

struct lan743x_mcast_cb_ctx {
	struct lan743x_softc const *sc;
	uint32_t count;
	bool overflow;
};

static u_int
lan743x_rfe_mcast_cb(void *arg, struct sockaddr_dl *sdl, u_int count)
{
	struct lan743x_mcast_cb_ctx *cb_ctx = (struct lan743x_mcast_cb_ctx *)
	    arg;

	if (cb_ctx->overflow) {
		return 0U;
	}

	/* Slots 1 through 32 are for multicast (slot 0 is the device MAC) */
	if (cb_ctx->count >= (LAN743X_RFE_ADDR_FILT_NUM - 1U)) {
		cb_ctx->overflow = true;
		return 0U;
	}

	cb_ctx->count++;
	uint8_t const *mac = (uint8_t const *)LLADDR(sdl);
	lan743x_rfe_perfect_filter_set(cb_ctx->sc, cb_ctx->count, mac);
	return 1U;
}

void
lan743x_rfe_multi_set(if_ctx_t ctx)
{
	struct lan743x_softc *sc = iflib_get_softc(ctx);
	if_t ifp = iflib_get_ifp(ctx);

	if (LAN743X_RFE_FORCE_ALLMULTI) {
		if_setflagbits(ifp, IFF_ALLMULTI, 0);
		lan743x_rfe_allmulti_enable(sc, true);
		return;
	}

	/* Clear multicast filter slots (1 through 32) */
	for (uint32_t i = 1U; i < LAN743X_RFE_ADDR_FILT_NUM; i++) {
		lan743x_rfe_perfect_filter_clear(sc, i);
	}

	struct lan743x_mcast_cb_ctx cb_ctx = { .sc = sc,
		.count = 0U,
		.overflow = false };

	(void)if_foreach_llmaddr(ifp, lan743x_rfe_mcast_cb, &cb_ctx);

	if (cb_ctx.overflow) {
		/* Too many multicast addresses for the perfect filter table */
		if_setflagbits(ifp, IFF_ALLMULTI, 0);
		lan743x_rfe_allmulti_enable(sc, true);
	} else {
		if_setflagbits(ifp, 0, IFF_ALLMULTI);
		lan743x_rfe_allmulti_enable(sc, false);
	}
}

int
lan743x_rfe_promisc_set(if_ctx_t ctx, int flags)
{
	struct lan743x_softc *sc = iflib_get_softc(ctx);

	if (((unsigned)flags & (unsigned)IFF_PROMISC) != 0U) {
		/* Promiscuous mode: accept everything */
		lan743x_rfe_perfect_filter_enable(sc, false);
		lan743x_rfe_broadcast_enable(sc, true);
		lan743x_rfe_allmulti_enable(sc, true);
		lan743x_csr_set_bits_32(sc, LAN743X_RFE_CTL,
		    LAN743X_RFE_ALLOW_UNICAST);
	} else {
		/* Non-promiscuous: re-enable perfect filtering */
		lan743x_csr_clr_bits_32(sc, LAN743X_RFE_CTL,
		    LAN743X_RFE_ALLOW_UNICAST);
		lan743x_rfe_perfect_filter_enable(sc, true);
		lan743x_rfe_broadcast_enable(sc, true);
		/* Re-run multicast filter setup */
		lan743x_rfe_multi_set(ctx);
	}
	return EOK;
}
