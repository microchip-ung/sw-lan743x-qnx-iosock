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

#include "lan743x_ctx.h"
#include "lan743x_csr.h"
#include "lan743x_ring.h"
#include <sys/bus.h>
#include <sys/systm.h>
#include <stdint.h>
#include <stddef.h>

void
lan743x_dump_some_stats(struct lan743x_softc *sc)
{
	static const uint16_t first_stat = 0x1200U;
	static const uint16_t last_stat = 0x12FCU;

	for (uint16_t i = first_stat; i <= last_stat; i += 4) {
		if (lan743x_csr_read_32(sc, i) != 0) {
			(void)device_printf(sc->dev, "0x%04x: 0x%08x\n", i,
			    lan743x_csr_read_32(sc, i));
		}
	}
	static char const *stat_names[] = { "MAC_ERR_STS ", "FCT_INT_STS ",
		"DMAC_CFG ", "DMAC_CMD ", "DMAC_INT_STS ", "DMAC_INT_EN ",
		"DMAC_RX_ERR_STS0 ", "DMAC_RX_ERR_STS1 ", "DMAC_RX_ERR_STS2 ",
		"DMAC_RX_ERR_STS3 ", "INT_STS ", "INT_EN ", "INT_VEC_EN ",
		"INT_VEC_MAP0 ", "INT_VEC_MAP1 ", "INT_VEC_MAP2 ", "TX_HEAD0",
		"TX_TAIL0", "DMAC_TX_ERR_STS0 ", NULL };
	static const uint16_t stats[] = { 0x114, 0xA0, 0xC00, 0xC0C, 0xC10,
		0xC14, 0xC60, 0xCA0, 0xCE0, 0xD20, 0x780, 0x788, 0x794, 0x7A0,
		0x7A4, 0x780, 0xD58, 0xD5C, 0xD60, 0x0 };
	printf("==============================\n");
	for (int i = 0; stats[i]; i++) {
		(void)device_printf(sc->dev, "%s at offset 0x%04x = 0x%08x\n",
		    stat_names[i], stats[i], lan743x_csr_read_32(sc, stats[i]));
	}
	printf("==== TX RING DESCS ====\n");
	for (uint32_t i = 0; i < LAN743X_DMA_RING_SIZE; i++) {
		(void)device_printf(sc->dev,
		    "ring[%d].data0=0x%08x\n"
		    "ring[%d].data1=0x%08x\n"
		    "ring[%d].data2=0x%08x\n"
		    "ring[%d].data3=0x%08x\n",
		    i, sc->tx_ring_data.ring[i].generic.ctl, i,
		    sc->tx_ring_data.ring[i].generic.addr.low, i,
		    sc->tx_ring_data.ring[i].generic.addr.high, i,
		    sc->tx_ring_data.ring[i].generic.sts);
	}
	(void)device_printf(sc->dev, "==== DUMP_TX_DMA_RAM ====\n");
	lan743x_csr_write_32(sc, 0x24, 0xF); // DP_SEL & TX_RAM_0
	for (uint8_t i = 0; i < 128; i++) {
		lan743x_csr_write_32(sc, 0x2C, i); // DP_ADDR

		lan743x_csr_write_32(sc, 0x28, 0); // DP_CMD

		while ((lan743x_csr_read_32(sc, 0x24) & 0x80000000) ==
		    0) { // DP_SEL & READY
			DELAY(1000);
		}

		(void)device_printf(sc->dev, "DMAC_TX_RAM_0[%u]=%08x\n", i,
		    lan743x_csr_read_32(sc, 0x30)); // DP_DATA
	}
}
