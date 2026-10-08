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

#include "lan743x_intr.h"
#include "lan743x_ctx.h"
#include "lan743x_csr.h"
#include "lan743x_assert.h"
#include "lan743x_reg_defs.h"
#include "lan743x_mii.h"
#include "lan743x_util.h"
#include "lan743x_trace.h"

#include <sys/bus.h>

#include <stdbool.h>

int
lan743x_legacy_intr(void *xsc)
{
	struct lan743x_softc *sc = (struct lan743x_softc *)xsc;

	iflib_admin_intr_deferred(sc->ctx);
	return (FILTER_HANDLED);
}

void
lan743x_configure_auto_clear(struct lan743x_softc const *sc)
{
	if_softc_ctx_t scctx = iflib_get_softc_ctx(sc->ctx);

	/* For each RX channel, auto-clear DMA interrupt status when MSI-X
	 * message is sent. */
	for (uint8_t i = 0U; i < (uint8_t)scctx->isc_nrxqsets; i++) {
		uint32_t const mask = LAN743X_RX_DMA_INT_STS_AUTO_CLR;
		lan743x_csr_write_32(sc, LAN743X_DMA_RX_CFG_C(i), mask);
	}

	/* For each TX channel, same auto-clear configuration. */
	for (uint8_t i = 0U; i < (uint8_t)scctx->isc_ntxqsets; i++) {
		uint32_t const mask = LAN743X_TX_DMA_INT_STS_AUTO_CLR;
		lan743x_csr_write_32(sc, LAN743X_DMA_TX_CFG_C(i), mask);
	}
}

static void
lan743x_map_vectors(struct lan743x_softc const *sc)
{
	/* Map RX channel 0 to vector 1 (shared with TX queue 0) so that
	 * RX interrupts fire on the RXTX vector. */
	lan743x_csr_write_32(sc, LAN743X_INTR_VEC_RX_MAP,
	    LAN743X_INTR_VEC_MAP(1U, 0U));

	/* Map TX channel 0 to vector 1 (shared with RX queue 0) so that
	 * TX interrupts fire on the RXTX vector. */
	lan743x_csr_write_32(sc, LAN743X_INTR_VEC_TX_MAP,
	    LAN743X_INTR_VEC_MAP(1U, 0U));

	/* "Other" sources (PHY, GPIO, MAC, etc.) stay on vector 0 (admin). */
	lan743x_csr_write_32(sc, LAN743X_INTR_VEC_OTHER_MAP, 0);
}

void
lan743x_init_intr_config(struct lan743x_softc *sc)
{
	if_softc_ctx_t scctx = iflib_get_softc_ctx(sc->ctx);

	lan743x_map_vectors(sc);
	lan743x_configure_auto_clear(sc);
}

int
lan743x_msix_intr_assign(if_ctx_t ctx, int msix)
{
	struct lan743x_softc *sc = iflib_get_softc(ctx);
	if_softc_ctx_t scctx = iflib_get_softc_ctx(sc->ctx);
	int vectorid = 0;
	static const char *rxq_names[] = { "rxq0" };
	static const char *txq_names[] = { "txq0" };
	static const int n_rxq_names = (int)(sizeof(rxq_names) /
	    sizeof(rxq_names[0]));
	static const int n_txq_names = (int)(sizeof(txq_names) /
	    sizeof(txq_names[0]));

	(void)device_printf(sc->dev, "enter lan743x_msix_intr_assign\n");

	LAN743X_ASSERT(scctx->isc_nrxqsets == 1 && scctx->isc_ntxqsets == 1,
	    ("num rxqsets/txqsets != 1 "));
	LAN743X_ASSERT(scctx->isc_nrxqsets == n_rxq_names,
	    ("Mismatch in number of rxqsets: %d, and rxqset names: %d",
		scctx->isc_nrxqsets, n_rxq_names));
	LAN743X_ASSERT(scctx->isc_ntxqsets == n_txq_names,
	    ("Mismatch in number of txqsets: %d, and txqset names: %d",
		scctx->isc_ntxqsets, n_txq_names));

	/*
	 * RIDs start at 1, and vector ids start at 0.
	 */
	LAN743X_TRY_ERR_PRINT(sc,
	    iflib_irq_alloc_generic(ctx, &sc->admin_irq, vectorid + 1,
		IFLIB_INTR_ADMIN, NULL, NULL, 0, "admin"),
	    "Failed to register admin interrupt handler\n");

	for (int i = 0; i < scctx->isc_nrxqsets; i++) {
		vectorid++;
		if (i >= n_rxq_names) {
			return ENODEV;
		}
		LAN743X_TRY_ERR_PRINT(sc,
		    iflib_irq_alloc_generic(ctx, &sc->rx_irq, vectorid + 1,
			IFLIB_INTR_RXTX, NULL, NULL, i, rxq_names[i]),
		    "Failed to register rxq %d interrupt handler\n", i);
	}
	for (int i = 0; i < scctx->isc_ntxqsets; i++) {
		if (i >= n_txq_names) {
			return ENODEV;
		}
		iflib_softirq_alloc_generic(ctx, NULL, IFLIB_INTR_TX, NULL, i,
		    txq_names[i]);
	}

	return EOK;
}

int
lan743x_phy_intr_enable(struct lan743x_softc *sc)
{
	(void)device_printf(sc->dev, "enter lan743x_phy_intr_enable\n");
	/* Read PHY interrupt CSR to clear any stale status bits before
	 * enabling interrupts. The CSR status is read-to-clear. */
	(void)lan743x_miibus_readreg(sc->dev, sc->phyaddr,
	    LAN743X_PHY_INTR_CSR);

	uint16_t const intr_en = PHY_INTR_LINK_DOWN_MASK |
	    PHY_INTR_LINK_UP_MASK | PHY_INTR_RX_ERROR_MASK;

	return lan743x_miibus_writereg(sc->dev, sc->phyaddr,
	    LAN743X_PHY_INTR_CSR, (int)intr_en);
}

void
lan743x_intr_enable_all(if_ctx_t ctx)
{
	struct lan743x_softc *sc = iflib_get_softc(ctx);
	if_softc_ctx_t scctx = iflib_get_softc_ctx(ctx);
	uint32_t dmac_enable = 0U;
	uint32_t intr_sts = LAN743X_INTR_STS_ANY | LAN743X_INTR_STS_PHY;
	uint32_t vec_en = LAN743X_INTR_STS_ANY;

	(void)device_printf(sc->dev, "enter lan743x_intr_enable_all\n");

	bool const nrxqsets_ok = ((scctx->isc_nrxqsets) > 0 &&
	    (scctx->isc_nrxqsets <= 4));
	LAN743X_ASSERT(nrxqsets_ok,
	    ("lan743x_intr_enable_all; isc_nrxqsets: %d\n",
		scctx->isc_nrxqsets));
	uint8_t const u_nrxqsets = (uint8_t)scctx->isc_nrxqsets;
	for (uint8_t i = 0U; i < u_nrxqsets; i++) {
		intr_sts |= LAN743X_INTR_STS_RX(i);
		dmac_enable |= LAN743X_DMAC_RX_INTR_ENBL(i);
		vec_en |= LAN743X_INTR_RX_VEC_STS(i);
	}

	bool const ntxqsets_ok = ((scctx->isc_ntxqsets) > 0 &&
	    (scctx->isc_ntxqsets <= 4));
	LAN743X_ASSERT(ntxqsets_ok,
	    ("lan743x_intr_enable_all; isc_ntxqsets: %d\n",
		scctx->isc_ntxqsets));
	uint8_t const u_ntxqsets = (uint8_t)scctx->isc_ntxqsets;
	for (uint8_t i = 0U; i < u_ntxqsets; i++) {
		intr_sts |= LAN743X_INTR_STS_TX(i);
		dmac_enable |= LAN743X_DMAC_TX_INTR_ENBL(i);
	}

	lan743x_csr_write_32(sc, LAN743X_INTR_ENBL_SET, intr_sts);
	lan743x_csr_write_32(sc, LAN743X_INTR_VEC_ENBL_SET, vec_en);
	lan743x_csr_write_32(sc, LAN743X_DMAC_INTR_STS, dmac_enable);
	lan743x_csr_write_32(sc, LAN743X_DMAC_INTR_ENBL_SET, dmac_enable);

	int const enable_result = lan743x_phy_intr_enable(sc);
	LAN743X_ASSERT(enable_result == EOK,
	    ("lan743x_intr_enable_all: Error enabling interrupts:%d\n",
		enable_result));
}

void
lan743x_intr_disable_all(if_ctx_t ctx)
{
	struct lan743x_softc *sc = iflib_get_softc(ctx);

	lan743x_csr_write_32(sc, LAN743X_INTR_ENBL_CLR, UINT32_MAX);
	lan743x_csr_write_32(sc, LAN743X_INTR_VEC_ENBL_CLR, UINT32_MAX);
	lan743x_csr_write_32(sc, LAN743X_INTR_STS, UINT32_MAX);

	lan743x_csr_write_32(sc, LAN743X_DMAC_INTR_ENBL_CLR, UINT32_MAX);
	lan743x_csr_write_32(sc, LAN743X_DMAC_INTR_STS, UINT32_MAX);
}

int
lan743x_rx_queue_intr_enable(if_ctx_t ctx, uint16_t qid)
{
	// don't need to do anything; using int status auto-clear and leaving the enables set
	return EOK;
}

int
lan743x_tx_queue_intr_enable(if_ctx_t ctx, uint16_t qid)
{
	// don't need to do anything; using int status auto-clear and leaving the enables set
	return EOK;
}

