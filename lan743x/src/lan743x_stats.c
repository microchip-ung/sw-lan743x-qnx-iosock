/*
 * Copyright 2016, QNX Software Systems.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You
 * may not reproduce, modify or distribute this software except in
 * compliance with the License. You may obtain a copy of the License
 * at: apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OF ANY KIND, either express or implied.
 *
 * This file may contain contributions from others, either as
 * contributors under the License or as licensors under other terms.
 * Please review this entire file for other proprietary rights or license
 * notices, as well as the QNX Development Suite License Guide at
 * licensing.qnx.com/license-guide/ for other information.
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

#include "lan743x_stats.h"
#include "lan743x_stats_types.h"
#include "lan743x_csr.h"
#include "lan743x_reg_defs.h"
#include "lan743x_ctx.h"
#include "lan743x_iflib.h"

#include <sys/param.h>
#include <sys/sysctl.h>

#include <string.h>

static const uint8_t eth_pause_frame_bytes = 64U;

/*
 * Optional generic statistics that are being collected.
 */
#define LAN743X_VALID_GENERIC_STATS                             \
	(NIC_STAT_RXED_MULTICAST | NIC_STAT_RXED_BROADCAST |    \
	    NIC_STAT_TXED_MULTICAST | NIC_STAT_TXED_BROADCAST | \
	    NIC_STAT_TX_FAILED_ALLOCS | NIC_STAT_RX_FAILED_ALLOCS)

/*
 * Optional Ethernet statistics that are being collected.
 */
#define LAN743X_VALID_ETHERNET_STATS                                         \
	(NIC_ETHER_STAT_ALIGN_ERRORS | NIC_ETHER_STAT_SINGLE_COLLISIONS |    \
	    NIC_ETHER_STAT_MULTI_COLLISIONS | NIC_ETHER_STAT_FCS_ERRORS |    \
	    NIC_ETHER_STAT_LATE_COLLISIONS | NIC_ETHER_STAT_XCOLL_ABORTED |  \
	    NIC_ETHER_STAT_NO_CARRIER | NIC_ETHER_STAT_EXCESSIVE_DEFERRALS | \
	    NIC_ETHER_STAT_OVERSIZED_PACKETS |                               \
	    NIC_ETHER_STAT_JABBER_DETECTED | NIC_ETHER_STAT_SHORT_PACKETS)

/**
 * @brief  Check for a rollover in a MIB counter.
 *
 * The LAN743X MIB counters are all 32-bit wide but the
 * QNX 802.3 MIB byte counts are 64-bits wide.
 *
 * Increment the rollover count if the hardware MIB counter has rolled over.
 *
 * It is assumed the counter rolls over no more than once between
 * successive checks.
 *
 * @param  lan743x      This device instance.
 * @param  reg          Hardware counter to check.
 * @param  counter      Current counter value.
 * @param  rollover     Rollover count.
 * @param  rollover_inc Counter rollover increment value.
 *
 * @return Void.
 */
static inline void
lan743x_count_64_rollover_check(struct lan743x_softc *sc, uint32_t reg,
    uint32_t *counter, uint64_t *rollover, uint64_t rollover_inc)
{
	uint32_t val = lan743x_csr_read_32(sc, reg);

	if (val < *counter) {
		*rollover += rollover_inc;
	}

	*counter = val;
}

uint64_t
lan743x_get_counter(if_ctx_t ctx, ift_counter cnt)
{
	struct lan743x_softc *sc = iflib_get_softc(ctx);
	if_t ifp = iflib_get_ifp(ctx);

	switch (cnt) {
	case IFCOUNTER_COLLISIONS:
		return ((uint64_t)sc->stats.single_collisions +
		    (uint64_t)sc->stats.multi_collisions +
		    (uint64_t)sc->stats.tx_late_collisions +
		    (uint64_t)sc->stats.tx_xcollisions);
	case IFCOUNTER_IERRORS:
		return ((uint64_t)sc->stats.rx_fcs_errors +
		    (uint64_t)sc->stats.align_errors +
		    (uint64_t)sc->stats.rx_fragment_errors +
		    (uint64_t)sc->stats.jabber_errors +
		    (uint64_t)sc->stats.undersized_frame_errors +
		    (uint64_t)sc->stats.oversized_frame_errors +
		    (uint64_t)sc->stats.rx_dropped_frames);
	case IFCOUNTER_OERRORS:
		return ((uint64_t)sc->stats.tx_xcollisions +
		    (uint64_t)sc->stats.tx_late_collisions);
	default:
		return (if_get_counter_default(ifp, cnt));
	}
}

/**
 * @brief  Check the transmit byte counters for a rollover.
 *
 * Check each transmit byte counter for a rollover and increment the rollover
 * count if a rollover is detected.
 *
 * @param  lan743x  This device instance.
 *
 * @return Void.
 */
static void
lan743x_tx_stats_rollover_check(struct lan743x_softc *sc)
{
	hw_stats_t *hw_stats = &sc->hw_stats;

	lan743x_count_64_rollover_check(sc, LAN743X_MAC_TX_UNICAST_BYTES,
	    &hw_stats->tx_unicast_bytes, &hw_stats->n_tx_byte_rollover, 1);

	lan743x_count_64_rollover_check(sc, LAN743X_MAC_TX_BROADCAST_BYTES,
	    &hw_stats->tx_broadcast_bytes, &hw_stats->n_tx_byte_rollover, 1);

	lan743x_count_64_rollover_check(sc, LAN743X_MAC_TX_MULTICAST_BYTES,
	    &hw_stats->tx_multicast_bytes, &hw_stats->n_tx_byte_rollover, 1);

	lan743x_count_64_rollover_check(sc, LAN743X_MAC_TX_PAUSE_FRAMES,
	    &hw_stats->tx_pause_frames, &hw_stats->n_tx_byte_rollover,
	    eth_pause_frame_bytes);
}

/**
 * @brief  Check the receive byte counters for a rollover.
 *
 * Check each receive byte counter for a rollover and increment the rollover
 * count if a rollover is detected.
 *
 * @param  lan743x  This device instance.
 *
 * @return Void.
 */
static void
lan743x_rx_stats_rollover_check(struct lan743x_softc *sc)
{
	hw_stats_t *hw_stats = &sc->hw_stats;

	lan743x_count_64_rollover_check(sc, LAN743X_MAC_RX_UNICAST_BYTE,
	    &hw_stats->rx_unicast_bytes, &hw_stats->n_rx_byte_rollover, 1);

	lan743x_count_64_rollover_check(sc, LAN743X_MAC_RX_BROADCAST_BYTE,
	    &hw_stats->rx_broadcast_bytes, &hw_stats->n_rx_byte_rollover, 1);

	lan743x_count_64_rollover_check(sc, LAN743X_MAC_RX_MULTICAST_BYTE,
	    &hw_stats->rx_multicast_bytes, &hw_stats->n_rx_byte_rollover, 1);

	lan743x_count_64_rollover_check(sc, LAN743X_MAC_RX_PAUSE_FRAMES,
	    &hw_stats->rx_pause_frames, &hw_stats->n_rx_byte_rollover,
	    eth_pause_frame_bytes);
}

/**
 * @brief  Check for rollover in the hardware byte counters.
 *
 * @param  lan743x  This device instance.
 *
 * @return
 */
static void
lan743x_stats_rollover_check(struct lan743x_softc *sc)
{
	lan743x_tx_stats_rollover_check(sc);
	lan743x_rx_stats_rollover_check(sc);
}

static void
lan743x_stats_add(struct lan743x_softc *sc)
{
	device_t dev = iflib_get_dev(sc->ctx);
	struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(dev);
	struct sysctl_oid *tree = device_get_sysctl_tree(dev);
	struct sysctl_oid_list *child = SYSCTL_CHILDREN(tree);

	SYSCTL_ADD_UINT(ctx, child, OID_AUTO, "txed_ok", CTLFLAG_RD,
	    &sc->stats.txed_ok, 0, "Frames Transmitted OK");

	SYSCTL_ADD_UINT(ctx, child, OID_AUTO, "rxed_ok", CTLFLAG_RD,
	    &sc->stats.rxed_ok, 0, "Frames Received OK");

	SYSCTL_ADD_UINT(ctx, child, OID_AUTO, "single_collisions", CTLFLAG_RD,
	    &sc->stats.single_collisions, 0, "TX Single Collisions");

	SYSCTL_ADD_UINT(ctx, child, OID_AUTO, "multi_collisions", CTLFLAG_RD,
	    &sc->stats.multi_collisions, 0, "TX Multiple Collisions");

	SYSCTL_ADD_UINT(ctx, child, OID_AUTO, "rx_fcs_errors", CTLFLAG_RD,
	    &sc->stats.rx_fcs_errors, 0, "RX FCS Errors");

	SYSCTL_ADD_UINT(ctx, child, OID_AUTO, "align_errors", CTLFLAG_RD,
	    &sc->stats.align_errors, 0, "RX Alignment Errors");

	SYSCTL_ADD_UQUAD(ctx, child, OID_AUTO, "octets_txed_ok", CTLFLAG_RD,
	    &sc->stats.octets_txed_ok, "Octets Transmitted OK");

	SYSCTL_ADD_UQUAD(ctx, child, OID_AUTO, "octets_rxed_ok", CTLFLAG_RD,
	    &sc->stats.octets_rxed_ok, "Octets Received OK");

	SYSCTL_ADD_UINT(ctx, child, OID_AUTO, "txed_multicast_ok", CTLFLAG_RD,
	    &sc->stats.txed_multicast, 0, "Multicast Frames Transmitted OK");

	SYSCTL_ADD_UINT(ctx, child, OID_AUTO, "txed_broadcast_ok", CTLFLAG_RD,
	    &sc->stats.txed_broadcast, 0, "Broadcast Frames Transmitted OK");

	SYSCTL_ADD_UINT(ctx, child, OID_AUTO, "execssived_deferrals",
	    CTLFLAG_RD, &sc->stats.excessive_deferrals, 0,
	    "TX Excessive Deferral Error");

	SYSCTL_ADD_UINT(ctx, child, OID_AUTO, "rxed_multicast", CTLFLAG_RD,
	    &sc->stats.rxed_multicast, 0, "Multicast Frames Received OK");

	SYSCTL_ADD_UINT(ctx, child, OID_AUTO, "rxed_broadcast", CTLFLAG_RD,
	    &sc->stats.rxed_broadcast, 0, "Broadcast Frames Received OK");

	SYSCTL_ADD_UINT(ctx, child, OID_AUTO, "oversized_frame_errors",
	    CTLFLAG_RD, &sc->stats.oversized_frame_errors, 0,
	    "RX Oversize Frame Errors");

	SYSCTL_ADD_UINT(ctx, child, OID_AUTO, "tx_xcollisions", CTLFLAG_RD,
	    &sc->stats.tx_xcollisions, 0, "TX Excessive Collisions");

	SYSCTL_ADD_UINT(ctx, child, OID_AUTO, "tx_late_collisions", CTLFLAG_RD,
	    &sc->stats.tx_late_collisions, 0, "TX Late Collisions");

	SYSCTL_ADD_UINT(ctx, child, OID_AUTO, "jabber_errors", CTLFLAG_RD,
	    &sc->stats.jabber_errors, 0, "RX Jabber Errors");

	SYSCTL_ADD_UINT(ctx, child, OID_AUTO, "undersized_frame_errors",
	    CTLFLAG_RD, &sc->stats.undersized_frame_errors, 0,
	    "RX Undersize Frame Errors");

	SYSCTL_ADD_UINT(ctx, child, OID_AUTO, "tx_failed_allocs", CTLFLAG_RD,
	    &sc->stats.tx_failed_allocs, 0, "TX Failed Allocations");

	SYSCTL_ADD_UINT(ctx, child, OID_AUTO, "rx_failed_allocs", CTLFLAG_RD,
	    &sc->stats.rx_failed_allocs, 0, "RX Failed Allocations");
}

/**
 * @brief  Update the interface statistics
 *
 * Update the interface statistics with the current values from the
 * 803.2 MIB counters in the MAC.
 *
 * These statistics are displayed by the nicinfo command.
 *
 * @param  sc  This device instance.
 *
 * @return
 */
void
lan743x_stats_update(struct lan743x_softc *sc)
{
	/* Get current byte counts */

	lan743x_stats_rollover_check(sc);

	/* Generic transmit statistics */

	uint32_t const tx_total_frames = lan743x_csr_read_32(sc,
	    LAN743X_MAC_TX_TOTAL_FRAMES);
	uint32_t const tx_pause_frames = lan743x_csr_read_32(sc,
	    LAN743X_MAC_TX_PAUSE_FRAMES);
	sc->stats.txed_ok = tx_total_frames + tx_pause_frames;

	sc->stats.octets_txed_ok = (sc->hw_stats.n_tx_byte_rollover << 32U) +
	    sc->hw_stats.tx_unicast_bytes + sc->hw_stats.tx_multicast_bytes +
	    sc->hw_stats.tx_broadcast_bytes +
	    (eth_pause_frame_bytes * (uint64_t)sc->hw_stats.tx_pause_frames);

	sc->stats.txed_multicast = lan743x_csr_read_32(sc,
	    LAN743X_MAC_TX_MULTICAST_FRAMES);
	sc->stats.txed_broadcast = lan743x_csr_read_32(sc,
	    LAN743X_MAC_TX_BROADCAST_FRAMES);

	/* Generic receive statistics */

	uint32_t const rx_total_frames = lan743x_csr_read_32(sc,
	    LAN743X_MAC_RX_TOTAL_FRAMES);
	uint32_t const rx_pause_frames = lan743x_csr_read_32(sc,
	    LAN743X_MAC_RX_PAUSE_FRAMES);
	sc->stats.rxed_ok = rx_total_frames + rx_pause_frames;

	sc->stats.octets_rxed_ok = (sc->hw_stats.n_rx_byte_rollover << 32U) +
	    sc->hw_stats.rx_unicast_bytes + sc->hw_stats.rx_multicast_bytes +
	    sc->hw_stats.rx_broadcast_bytes +
	    (eth_pause_frame_bytes * (uint64_t)sc->hw_stats.rx_pause_frames);

	sc->stats.rxed_multicast = lan743x_csr_read_32(sc,
	    LAN743X_MAC_RX_MULTICAST_FRAMES);
	sc->stats.rxed_broadcast = lan743x_csr_read_32(sc,
	    LAN743X_MAC_RX_BROADCAST_FRAMES);

	/*
	 * Ethernet-specific transmit statistics
	 */
	sc->stats.single_collisions = lan743x_csr_read_32(sc,
	    LAN743X_MAC_TX_SINGLE_COLLISIONS);
	sc->stats.multi_collisions = lan743x_csr_read_32(sc,
	    LAN743X_MAC_TX_MULTIPLE_COLLISIONS);
	sc->stats.tx_late_collisions = lan743x_csr_read_32(sc,
	    LAN743X_MAC_TX_LATE_COLLISIONS);
	sc->stats.tx_xcollisions = lan743x_csr_read_32(sc,
	    LAN743X_MAC_TX_EXCESSIVE_COLLISIONS);
	sc->stats.excessive_deferrals = lan743x_csr_read_32(sc,
	    LAN743X_MAC_TX_EXCESS_DEFER_ERRORS);

	/*
	 * Ethernet-specific receive statistics
	 */
	sc->stats.align_errors = lan743x_csr_read_32(sc,
	    LAN743X_MAC_RX_ALIGNMENT_ERRORS);
	sc->stats.rx_fcs_errors = lan743x_csr_read_32(sc,
	    LAN743X_MAC_RX_FCS_ERRORS);
	sc->stats.oversized_frame_errors = lan743x_csr_read_32(sc,
	    LAN743X_MAC_RX_OVERSIZE_FRAME_ERRORS);
	sc->stats.jabber_errors = lan743x_csr_read_32(sc,
	    LAN743X_MAC_RX_JABBER_ERRORS);
	sc->stats.undersized_frame_errors = lan743x_csr_read_32(sc,
	    LAN743X_MAC_RX_UNDERSIZE_FRAME_ERRORS);
	sc->stats.rx_dropped_frames = lan743x_csr_read_32(sc,
	    LAN743X_MAC_RX_DROPPED_FRAMES);
}

/**
 * @brief  Reset the interface statistics.
 *
 * Reset the interface statistics and the 802.3 MIB counters in the MAC.
 *
 * @param  sc  This device instance.
 *
 * @return Void.
 */
void
lan743x_stats_reset(struct lan743x_softc *sc)
{
	/* reset the MAC counters */
	lan743x_csr_set_bits_32(sc, LAN743X_MAC_CR, LAN743X_MAC_CR_CNTR_RST);
	/* todo : wait for LAN743X_MAC_CR_CNTR_RST to clear */

	/* reset the statistics */
	(void)memset(&sc->stats, 0, sizeof(sc->stats));
	(void)memset(&sc->hw_stats, 0, sizeof(sc->hw_stats));
}

/**
 * @brief  Initialise the interface statistics.
 *
 * Initialise the interface statistics and the 802.3 MIB counters in the MAC.
 *
 * @param  sc  This device instance.
 *
 * @return Void.
 */
void
lan743x_stats_init(struct lan743x_softc *sc)
{
	lan743x_stats_reset(sc);

	lan743x_stats_add(sc);

	lan743x_stats_update(sc);
}
