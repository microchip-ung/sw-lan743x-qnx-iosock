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
#include "lan743x_tx.h"
#include "lan743x_assert.h"
#include "lan743x_ctx.h"
#include "lan743x_ring.h"
#include "lan743x_reg_defs.h"
#include "lan743x_iflib.h"
#include "lan743x_csr.h"
#include "lan743x_util.h"
#include "lan743x_trace.h"
#include <string.h>
#include <stdbool.h>

static uint32_t
make_txd_ctl(struct lan743x_softc *sc, bus_size_t seg_len, bool const first_seg,
    bool const last_seg, if_pkt_info_t ipi)
{
	LAN743X_ASSERT((seg_len & ~(bus_size_t)LAN743X_DESC_CTL_BUFLEN_MASK) ==
		0U,
	    ("make_txd_ctl; seg_len too big: %lu.\n", seg_len));
	uint32_t ret = 0U;
	ret |= (uint32_t)(seg_len & (bus_size_t)LAN743X_DESC_CTL_BUFLEN_MASK);
	/* first segment */
	if (first_seg) {
		ret |= LAN743X_TX_DESC_CTL_FS;
		/* TODO: need to learn about FCS */
		ret |= LAN743X_DESC_CTL_FCS;
		/* csum offload */
		bool const tcp_udp = lan743x_u32_to_bool(
		    ipi->ipi_csum_flags & (CSUM_IP_TCP | CSUM_IP_UDP));
		if (tcp_udp) {
			ret |= LAN743X_DESC_CTL_TPE;
		}
		bool const ip = lan743x_u32_to_bool(
		    ipi->ipi_csum_flags & CSUM_IP);
		if (ip) {
			ret |= LAN743X_DESC_CTL_IPE;
		}
		bool const tso = lan743x_u32_to_bool(
		    ipi->ipi_csum_flags & CSUM_TSO);
		if (tso) {
			ret |= LAN743X_DESC_CTL_EXT;
		}
	}
	/* last segment */
	if (last_seg) {
		ret |= LAN743X_TX_DESC_CTL_LS;
	}
	return ret;
}

static void
lan743x_isc_txd_encap_fs_handler(if_pkt_info_t ipi, qidx_t *pidx,
    struct lan743x_ring_data *rdata)
{
	struct lan743x_ring_desc *txd = &rdata->ring[*pidx].generic;
	bool const is_tso = lan743x_u32_to_bool(
	    ipi->ipi_csum_flags & (uint32_t)CSUM_TSO);
	if (!is_tso) {
		txd->sts = htole32(
		    (ipi->ipi_len << 16) & LAN743X_DESC_FRAME_LEN_MASK);
		return;
	}

	// handle TSO
	/* Load the TSO seg size into MSS */
	txd->sts = htole32(
	    ((uint32_t)ipi->ipi_tso_segsz << 16) & LAN743X_DESC_FRAME_LEN_MASK);
	/* need ext desc for tso, so make one */
	*pidx = lan743x_next_ring_idx(*pidx);
	struct lan743x_ring_desc_tx_ext *extd = &rdata->ring[*pidx].tx_ext;
	uint32_t ext_ctl = 0U;
	ext_ctl |= LAN743X_DESC_CTL_DTYPE_EXT | LAN743X_EXT_DESC_CTL_LSO;
	uint32_t payload_len = ipi->ipi_len;
	bool const payload_len_overflow = lan743x_u32_to_bool(
	    payload_len & ~LAN743X_EXT_DESC_CTL_PAYLENGTH_MASK);
	LAN743X_ASSERT(!payload_len_overflow,
	    ("TSO payload length overflow: %d.\n", payload_len));
	ext_ctl |= (payload_len & LAN743X_EXT_DESC_CTL_PAYLENGTH_MASK)
	    << LAN743X_EXT_DESC_CTL_PAYLENGTH_POS;
	extd->ctl = htole32(ext_ctl);
}

int
lan743x_isc_txd_encap(void *xsc, if_pkt_info_t ipi)
{
	LAN743X_ASSERT(ipi->ipi_qsidx == 0U,
	    ("tried to refill TX Channel %d.\n", ipi->ipi_qsidx));

	struct lan743x_softc *const sc = xsc;
	LAN743X_TS_TRACE_MSG(sc, "lan743x_isc_txd_encap()");
	struct lan743x_ring_data *const rdata = &sc->tx_ring_data;
	bus_dma_segment_t *const segs = ipi->ipi_segs;
	qidx_t const nsegs = ipi->ipi_nsegs;

	/* For each seg, create a descriptor */
	qidx_t pidx = ipi->ipi_pidx;
	LAN743X_DBG_TRACE(sc, "lan743x_isc_txd_encap(); pidx: %d; nsegs: %d\n",
	    pidx, nsegs);
	for (qidx_t i = 0; i < nsegs; ++i, pidx = lan743x_next_ring_idx(pidx)) {
		bool const first_seg = (i == 0U);
		bool const last_seg = ((i + 1U) == nsegs);

		struct lan743x_ring_desc *const txd =
		    &rdata->ring[pidx].generic;
		txd->ctl = htole32(
		    make_txd_ctl(sc, segs[i].ds_len, first_seg, last_seg, ipi));
		txd->addr.low = htole32(
		    lan743x_csr_get_low32_addr(segs[i].ds_addr));
		txd->addr.high = htole32(
		    lan743x_csr_get_high32_addr(segs[i].ds_addr));
		/* write frame length for first packet desc only; otherwise continue */
		if (!first_seg) {
			continue;
		}
		// first segment
		lan743x_isc_txd_encap_fs_handler(ipi, &pidx, rdata);
	}
	ipi->ipi_new_pidx = pidx;
	LAN743X_DBG_TRACE(sc, "lan743x_isc_txd_encap(); ipi_new_pidx: %d\n",
	    ipi->ipi_new_pidx);
	return (0);
}

void
lan743x_isc_txd_flush(void *xsc, uint16_t txqid, qidx_t pidx)
{
	struct lan743x_softc *sc = (struct lan743x_softc *)xsc;
	LAN743X_TS_TRACE_MSG(sc, "lan743x_isc_txd_flush()");
	struct lan743x_ring_data *rdata = &sc->tx_ring_data;

	LAN743X_ASSERT(txqid == 0U, ("tried to flush TX Channel %d.\n", txqid));

	uint16_t const prev_tail = rdata->last_tail;
	LAN743X_DBG_TRACE(sc,
	    "lan743x_isc_txd_flush(); prev_tail: %d; pidx: %d\n", prev_tail,
	    pidx);
	if (prev_tail == pidx) {
		return;
	}
	rdata->last_tail = pidx;
	lan743x_csr_write_32(sc, (uint32_t)LAN743X_DMA_TX_TAIL(txqid),
	    rdata->last_tail);
}

int
lan743x_isc_txd_credits_update(void *xsc, uint16_t txqid, bool clear)
{
	/*
	 * > If clear is true, we need to report the number of TX command ring
	 * > descriptors that have been processed by the device.  If clear is
	 * > false, we just need to report whether or not at least one TX
	 * > command ring descriptor has been processed by the device.
	 * - vmx driver
	 */
	LAN743X_ASSERT(txqid == 0U,
	    ("tried to credits_update TX Channel %d.\n", txqid));
	struct lan743x_softc *const sc = xsc;
	LAN743X_TS_TRACE_MSG(sc, "lan743x_isc_txd_credits_update()");
	struct lan743x_ring_data *const rdata = &sc->tx_ring_data;

	/* Force an immediate head pointer writeback so head_wb reflects the
	 * latest DMA progress.  The SWFLUSH bit is self-clearing (W1S/SC). */
	lan743x_csr_write_32(sc, (uint32_t)LAN743X_DMA_TX_CONFIG0(txqid),
	    sc->tx_cfg_a_cache | LAN743X_TX_HP_WB_SWFLUSH);

	LAN743X_ASSERT(rdata->head_wb < (1U << 16U),
	    ("head wb value is too big: 0x%08X.\n", rdata->head_wb));
	uint16_t const head_wb = (uint16_t)rdata->head_wb;
	/* No descriptors processed, so always return 0 and do nothing. */
	if (head_wb == rdata->last_head) {
		return 0;
	}
	/* If we got here, at least one desc has been processed by the device,
	 * so return 1 if clear is false. */
	if (!clear) {
		return 1;
	}
	uint16_t processed = 0U;
	struct lan743x_ring_desc *txd = &rdata->ring[rdata->last_head].generic;
	/* Now we know we have stale descriptors and clear is true, so need to
	 * clear and report. We'll do the mem clearing in chunks. First count
	 * and clear any processed descriptors between last head and the end of
	 * the ring, in the case that we've rolled over. */
	if (head_wb < rdata->last_head) {
		/* If the last_head is > LAN743X_DMA_RING_SIZE, then something's
		 * wrong, so avoid possibly memsetting invalid memory. */
		if (rdata->last_head <= LAN743X_DMA_RING_SIZE) {
			uint16_t const distance_to_end = LAN743X_DMA_RING_SIZE -
			    rdata->last_head;
			uint16_t const clr_mem_sz =
			    (uint16_t)sizeof(struct lan743x_ring_desc) *
			    distance_to_end;
			(void)memset(txd, 0, clr_mem_sz);
			processed += distance_to_end;
		}
		rdata->last_head = 0U;
		txd = &rdata->ring[rdata->last_head].generic;
	}
	/* Now head_wb will always be ahead of the last_head; clear out
	 * descriptors up to the head_wb. */
	uint16_t const distance_to_head_wb = head_wb - rdata->last_head;
	(void)memset(txd, 0,
	    sizeof(struct lan743x_ring_desc) * distance_to_head_wb);
	rdata->last_head = head_wb;
	LAN743X_ASSERT((uint32_t)processed + (uint32_t)distance_to_head_wb <=
		(uint32_t)UINT16_MAX,
	    ("lan743x_isc_txd_credits_update: processed descriptors value wraparound detected\n"));
	processed += distance_to_head_wb;
	LAN743X_DBG_TRACE(sc,
	    "lan743x_isc_txd_credits_update(); processed: %d\n", processed);

	return (int)processed;
}

int
lan743x_tx_queues_alloc(if_ctx_t ctx, caddr_t *vaddrs, uint64_t *paddrs,
    int ntxqs, int ntxqsets)
{
	struct lan743x_softc *sc = iflib_get_softc(ctx);
	bool const bad_ntxqs_val = (ntxqs < 0) || (ntxqs > 4);
	if (ntxqsets != 1) {
		(void)device_printf(sc->dev,
		    "lan743x_tx_queues_alloc; ntxqsets = %d\n", ntxqsets);
		return EINVAL;
	}
	if (bad_ntxqs_val) {
		(void)device_printf(sc->dev,
		    "lan743x_tx_queues_alloc; ntxqs = %d\n", ntxqs);
		return EINVAL;
	}
	uint8_t const u_ntxqs = (uint8_t)ntxqs;
	uint8_t const u_ntxqsets = (uint8_t)ntxqsets;
	struct lan743x_ring_data *rdata = &sc->tx_ring_data;
	for (uint8_t q = 0U; q < (uint8_t)u_ntxqsets; q++) {
		rdata->ring = (union lan743x_ring_desc_u *)
		    vaddrs[q * u_ntxqs + 0U];
		rdata->ring_bus_addr = paddrs[q * u_ntxqs + 0U];
	}
	return 0;
}
