/*
 * Copyright 2016, QNX Software Systems.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You
 * may not reproduce, modify or distribute this software except in
 * compliance with the License. You may obtain a copy of the License
 * at: www.apache.org/licenses/LICENSE-2.0
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

#ifndef LAN743X_STATS_TYPES_INCLUDE_GUARD
#define LAN743X_STATS_TYPES_INCLUDE_GUARD

#include <stdint.h>

typedef struct {
	uint32_t tx_unicast_bytes;   /* total unicast bytes sent             */
	uint32_t tx_broadcast_bytes; /* total broadcast bytes sent           */
	uint32_t tx_multicast_bytes; /* total multicast bytes sent           */
	uint32_t tx_pause_frames;    /* total pause frames sent              */

	uint32_t rx_unicast_bytes;   /* total unicast bytes received         */
	uint32_t rx_broadcast_bytes; /* total broadcast bytes received       */
	uint32_t rx_multicast_bytes; /* total multicast bytes received       */
	uint32_t rx_pause_frames;    /* total pause frames received          */

	uint64_t n_tx_byte_rollover; /* num. Tx byte counter rollovers       */
	uint64_t n_rx_byte_rollover; /* num. Rx byte counter rollovers       */
} hw_stats_t;

typedef struct {
	/* IEEE 802.3 MIB */
	uint64_t octets_txed_ok;
	uint64_t octets_rxed_ok;
	uint32_t txed_ok;
	uint32_t rxed_ok;
	uint32_t single_collisions;
	uint32_t multi_collisions;
	uint32_t rx_fcs_errors;
	uint32_t tx_fcs_errors;
	uint32_t align_errors;
	uint32_t txed_multicast;
	uint32_t txed_broadcast;
	uint32_t excessive_deferrals;
	uint32_t rxed_multicast;
	uint32_t rxed_broadcast;
	uint32_t oversized_frame_errors;
	uint32_t tx_xcollisions;
	uint32_t tx_late_collisions;
	uint32_t jabber_errors;
	uint32_t undersized_frame_errors;
	uint32_t tx_failed_allocs;
	uint32_t rx_failed_allocs;
	uint32_t rx_fragment_errors;
	uint32_t rx_dropped_frames;
} lan743x_stats_t;

#endif
