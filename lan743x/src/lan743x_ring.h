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

#ifndef LAN743X_RING_INCLUDE_GUARD
#define LAN743X_RING_INCLUDE_GUARD

#include <stdint.h>
#include <sys/types.h>
#include <sys/bus.h>
#include <sys/param.h>

#pragma pack(push, 1)
struct lan743x_ring_desc_addr {
	uint32_t low;
	uint32_t high;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct lan743x_ring_desc {
	uint32_t ctl;			    /* data0 */
	struct lan743x_ring_desc_addr addr; /* data(1|2) */
	uint32_t sts;			    /* data3 */
};
#pragma pack(pop)

#pragma pack(push, 1)
struct lan743x_ring_desc_rx_swown {
	uint32_t desc_info;
	uint32_t status;
	uint32_t offloads;
	uint32_t rss_hash;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct lan743x_ring_desc_tx_ext {
	uint32_t ctl;
	uint32_t reserved1_;
	uint32_t reserved2_;
	uint32_t reserved3_;
};
#pragma pack(pop)

#if 0
struct lan743x_ring_info {
	uint32_t				head_wb;
	struct lan743x_ring_desc			*ring;
}
#endif
#define LAN743X_HEAD_WB_PTR(_ring_info_ptr) ((uint32_t *)(_ring_info_ptr))

#define LAN743X_RING_PTR(_ring_info_ptr) \
	((struct lan743x_ring_desc *)(LAN743X_HEAD_WB_PTR(_ring_info_ptr) + 1))

#define LAN743X_RING_SPACE(_sc)                                            \
	((((_sc)->tx_ring_data.last_head - (_sc)->tx_ring_data.last_tail - \
	      1) +                                                         \
	     LAN743X_DMA_RING_SIZE) %                                      \
	    LAN743X_DMA_RING_SIZE)

struct lan743x_ring_data {
	uint32_t head_wb;
	union lan743x_ring_desc_u {
		struct lan743x_ring_desc generic;
		struct lan743x_ring_desc_rx_swown rx_swown;
		struct lan743x_ring_desc_tx_ext tx_ext;
	} *ring;

	bus_addr_t head_wb_bus_addr;
	bus_addr_t ring_bus_addr;

	uint16_t last_head;
	uint16_t last_tail;
};

/** DMA Rings **/
/**
 * Page size is 256 bytes
 *
 * Ring size, however, these could be tunable (for RX & TX)
 * to be a multiple of 4 (max is 65532)
 *
 **/
/* In linux driver these numbers are 50 and 65 for tx and rx .... */
/* Set to 2k for now to fix broken TX in FreeBSD driver. May do some testing
 * to determine possible better values. */
#define LAN743X_DMA_RING_SIZE_MIN \
	((uint16_t)(PAGE_SIZE) / (uint16_t)sizeof(struct lan743x_ring_desc))
#define LAN743X_DMA_RING_SIZE                                                 \
	LAN743X_DMA_RING_SIZE_MIN /* in programming guide, this number is 100 \
				   */
#define LAN743X_DMA_MAXSEGS 32U
#define LAN743X_DMA_RING_LIST_SIZE \
	((uint16_t)sizeof(struct lan743x_ring_desc) * LAN743X_DMA_RING_SIZE)
#define LAN743X_DMA_RING_INFO_SIZE \
	((uint8_t)sizeof(uint32_t) + LAN743X_DMA_RING_LIST_SIZE)

static inline uint16_t
lan743x_next_ring_idx(uint16_t idx)
{
	/* TODO: get ring size at run-time */
	return (idx + 1U) % LAN743X_DMA_RING_SIZE;
}

#endif
