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

#include <stdint.h>

#include "lan743x_rx.h"
#include "lan743x_assert.h"
#include "lan743x_ctx.h"
#include "lan743x_ring.h"
#include "lan743x_csr.h"
#include "lan743x_reg_defs.h"
#include "lan743x_util.h"
#include "lan743x_iflib.h"
#include "lan743x_trace.h"

static uint16_t
lan743x_get_rx_ring_size(const struct lan743x_softc *sc)
{
	return LAN743X_DMA_RING_SIZE;
}

int
lan743x_rx_queues_alloc(if_ctx_t ctx, caddr_t *vaddrs, uint64_t *paddrs,
    int nrxqs, int nrxqsets)
{
	if (nrxqsets != 1) {
		return EINVAL;
	}
	if (nrxqs != 1) {
		return EINVAL;
	}
	struct lan743x_softc *sc = iflib_get_softc(ctx);
	struct lan743x_ring_data *rdata = &sc->rx_ring_data;
	for (int q = 0; q < nrxqsets; q++) {
		/* Ring */
		rdata->ring = (union lan743x_ring_desc_u *)
		    vaddrs[q * nrxqs + 0];
		rdata->ring_bus_addr = paddrs[q * nrxqs + 0];
	}
	return 0;
}

int
lan743x_isc_rxd_available(void *xsc, uint16_t rxqid, qidx_t idx, qidx_t budget)
{
	struct lan743x_softc *sc = xsc;
	LAN743X_TS_TRACE_MSG(sc, "lan743x_isc_rxd_available()");
	LAN743X_ASSERT(rxqid == 0U,
	    ("tried to check availability in RX Channel %d.\n", rxqid));

	uint32_t const head_wb_reg = sc->rx_ring_data.head_wb;
	LAN743X_ASSERT(head_wb_reg < ((uint32_t)1U << 16U),
	    ("head wb value is too big: 0x%08X.\n", head_wb_reg));
	uint16_t const head_wb = (uint16_t)head_wb_reg;
	LAN743X_DBG_TRACE(sc,
	    "lan743x_isc_rxd_available(); idx: %d; head_wb: %d\n", idx,
	    head_wb);
	uint16_t avail = 0U;
	while ((idx != head_wb) && (budget != 0U)) {
		/* XXX: Need to verify whether the desc is device-owned? */

		uint16_t advanced;
		if (idx < head_wb) {
			uint16_t const distance_to_head = head_wb - idx;
			advanced = MIN(distance_to_head, budget);
			idx += advanced;
		} else {
			LAN743X_ASSERT(idx < lan743x_get_rx_ring_size(sc),
			    ("lan743x_isc_rxd_available; idx too big: %d.\n",
				idx));
			// clang-format off
			uint16_t const distance_to_end = lan743x_get_rx_ring_size(sc) - idx;
			// clang-format on
			advanced = MIN(distance_to_end, budget);
			/* Set index to beginning here; either we hit out budget
			 * and will exit the loop, or we'll continue counting
			 * from the beginning of the ring. */
			idx = 0U;
		}
		LAN743X_ASSERT((UINT16_MAX - (int)avail - (int)advanced) >= 0,
		    ("lan743x_isc_rxd_available: available overflow\n"));
		avail += advanced;
		budget -= advanced;
	}
	LAN743X_DBG_TRACE(sc, "lan743x_isc_rxd_available(); avail: %d\n",
	    avail);

	return (int)avail;
}

static int
lan743x_rxd_pkt_get_check_state(struct lan743x_softc const *sc,
    uint32_t desc_info, bool fs, bool frm_start_found)
{
	if ((desc_info & LAN743X_DESC_CTL_OWN) != 0U) {
		(void)device_printf(sc->dev,
		    "Tried to read descriptor ... "
		    "found that it's owned by the hardware\n");
		return EBUSY;
	}
	if (frm_start_found && fs) {
		(void)device_printf(sc->dev,
		    "Found first desc bit set in not the first Rx descriptor\n");
		/* XXX: advance ring and continue? */
		return EIO;
	}
	if (!frm_start_found && !fs) {
		(void)device_printf(sc->dev,
		    "Did not find first desc bit set in first Rx descriptor\n");
		/* XXX: advance ring and continue? */
		return EIO;
	}
	return EOK;
}

static void
lan743x_rx_configure_csum_flags(if_rxd_info_t ri,
    struct lan743x_ring_data const *rdata)
{
	uint32_t const status = le32toh(
	    rdata->ring[rdata->last_head].rx_swown.status);
	bool const ip_csum_err = lan743x_u32_to_bool(
	    status & ((uint32_t)1U << 17U));
	bool const l4_csum_err = lan743x_u32_to_bool(
	    status & ((uint32_t)1U << 16U));
	uint8_t const protocol_id = (uint8_t)((status >> 13U) & 0x3U);
	switch (protocol_id) {
	case 1U:
		ri->iri_csum_flags |= (uint32_t)CSUM_IP |
		    (uint32_t)CSUM_IP_TCP | (uint32_t)CSUM_L3_CALC |
		    (uint32_t)CSUM_L4_CALC;
		break;
	case 2U:
		ri->iri_csum_flags |= (uint32_t)CSUM_IP |
		    (uint32_t)CSUM_IP_UDP | (uint32_t)CSUM_L3_CALC |
		    (uint32_t)CSUM_L4_CALC;
		break;
	case 3U:
		ri->iri_csum_flags |= (uint32_t)CSUM_IP |
		    (uint32_t)CSUM_L3_CALC | (uint32_t)CSUM_L4_CALC;
		break;
	default:
		/* Nothing to be done here. */
		break;
	}
	bool const l3_calc_en = lan743x_u32_to_bool(
	    ri->iri_csum_flags & (uint32_t)CSUM_L3_CALC);
	if (l3_calc_en && !ip_csum_err) {
		ri->iri_csum_flags |= (uint32_t)CSUM_L3_VALID;
	}
	bool const ignore_l4_csum = lan743x_u32_to_bool(status & (1U << 0U));
	bool const l4_calc_en = lan743x_u32_to_bool(
	    ri->iri_csum_flags & (uint32_t)CSUM_L4_CALC);
	if (l4_calc_en && !ignore_l4_csum && !l4_csum_err) {
		ri->iri_csum_data = htons(0xFFFFU);
		ri->iri_csum_flags |= (uint32_t)CSUM_L4_VALID;
	}
	if (ignore_l4_csum) {
		ri->iri_csum_flags &= ~(uint32_t)CSUM_L4_CALC;
	}
}

int
lan743x_isc_rxd_pkt_get(void *xsc, if_rxd_info_t ri)
{
	LAN743X_ASSERT(ri->iri_qsidx == 0U,
	    ("tried to check availability in RX Channel %d\n", ri->iri_qsidx));

	struct lan743x_softc *const sc = xsc;
	LAN743X_TS_TRACE_MSG(sc, "lan743x_isc_rxd_pkt_get()");
	struct lan743x_ring_data *const rdata = &sc->rx_ring_data;
	bool frm_start_found = false;
	bool frm_end_found = false;
	ri->iri_nfrags = 0U;
	ri->iri_len = 0U;

	uint16_t i;
	for (i = 0U; !frm_end_found && (rdata->head_wb != rdata->last_head);
	    i++, rdata->last_head = lan743x_next_ring_idx(rdata->last_head)) {
		/* copy first 32-bits of ring desc and do swapping */
		uint32_t const desc_info = le32toh(
		    rdata->ring[rdata->last_head].rx_swown.desc_info);
		bool const fs = (desc_info & LAN743X_RX_DESC_CTL_FS) != 0U;
		LAN743X_TRY_ERR(lan743x_rxd_pkt_get_check_state(sc, desc_info,
		    fs, frm_start_found));
		/* If here, then we got a valid start at some point */
		frm_start_found = true;
		frm_end_found = ((desc_info & LAN743X_RX_DESC_CTL_LS) != 0U);
		ri->iri_frags[i].irf_flid = 0U;
		ri->iri_frags[i].irf_len = (uint16_t)LAN743X_DESC_GET_BUF_LEN(
		    desc_info);
		ri->iri_frags[i].irf_idx = rdata->last_head;
		if (!frm_end_found) {
			continue;
		}
		/* If here, then it's an eof segment */
		/* TODO: add checking of the RXSTATUS field */
		ri->iri_len = (uint16_t)LAN743X_DESC_GET_FRAME_LEN(desc_info);
		lan743x_rx_configure_csum_flags(ri, rdata);
	}
	if (i >= ((uint16_t)1U << (sizeof(ri->iri_nfrags) * 8U))) {
		(void)device_printf(sc->dev,
		    "lan743x_isc_rxd_pkt_get: more fragments were processed than iri_nfrags can hold: %d\n",
		    i);
		return EIO;
	}
	LAN743X_DBG_TRACE(sc, "lan743x_isc_rxd_pkt_get(); frags: %d\n", i);
	ri->iri_nfrags = (uint8_t)i;
	if (!frm_end_found) {
		return EIO;
	}
	return EOK;
}

void
lan743x_isc_rxd_refill(void *xsc, if_rxd_update_t iru)
{
	struct lan743x_softc *sc = (struct lan743x_softc *)xsc;
	LAN743X_TS_TRACE_MSG(sc, "lan743x_isc_rxd_refill()");
	struct lan743x_ring_data *rdata = &sc->rx_ring_data;

	LAN743X_ASSERT(iru->iru_qsidx == 0U,
	    ("tried to refill RX Channel %d.\n", iru->iru_qsidx));
	LAN743X_DBG_TRACE(sc, "lan743x_isc_rxd_refill(); iru->iru_count: %d\n",
	    iru->iru_count);
	for (uint16_t i = 0U; i != iru->iru_count; i++) {
		qidx_t idx = iru->iru_idxs[i];
		struct lan743x_ring_desc *rxd = &rdata->ring[idx].generic;
		rxd->sts = 0U;
		uint64_t const paddr64 = iru->iru_paddrs[i];
		rxd->addr.low = htole32(lan743x_csr_get_low32_addr(paddr64));
		rxd->addr.high = htole32(lan743x_csr_get_high32_addr(paddr64));
		rxd->ctl = htole32((uint32_t)(LAN743X_DESC_CTL_OWN |
		    ((uint32_t)iru->iru_buf_size &
			LAN743X_DESC_CTL_BUFLEN_MASK)));
	}
}

void
lan743x_isc_rxd_flush(void *xsc, uint16_t rxqid, uint8_t flid, qidx_t pidx)
{
	struct lan743x_softc *sc = (struct lan743x_softc *)xsc;
	LAN743X_TS_TRACE_MSG(sc, "lan743x_isc_rxd_flush()");

	LAN743X_ASSERT(rxqid == 0U, ("tried to flush RX Channel %d.\n", rxqid));
	LAN743X_DBG_TRACE(sc,
	    "lan743x_isc_rxd_flush(); pidx: %d; prev last_tail: %d\n", pidx,
	    sc->rx_ring_data.last_tail);
	sc->rx_ring_data.last_tail = pidx;
	lan743x_csr_write_32(sc, LAN743X_DMA_RX_TAIL(rxqid),
	    sc->rx_ring_data.last_tail);
}
