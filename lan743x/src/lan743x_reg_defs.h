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

#ifndef LAN743X_REG_DEFS_INCLUDE_GUARD
#define LAN743X_REG_DEFS_INCLUDE_GUARD

#include "lan743x_csr.h"
#include <stdint.h>
#include <stdbool.h>
#include "lan743x_assert.h"

/** Control/Status Registers **/
#define LAN743X_BAR 0 /* PCI Base Address */

/** Reset **/
#define LAN743X_HW_CFG 0x10U /** H/W Configuration Register **/
#define LAN743X_LITE_RESET 0x2U
#define LAN743X_CLK125_EN 0x2000000U

/** RGMII **/
#define LAN743X_MAC_RGMII_ID 0x128U
#define LAN743X_MAC_RGMII_ID_TXC_DELAY_EN 0x2U
#define LAN743X_MAC_RGMII_ID_RXC_DELAY_EN 0x1U

/** MAC **/
#define LAN743X_MAC_CR 0x0100U		/** MAC Crontrol Register **/
#define LAN743X_MAC_ADD_ENBL 0x1000U	/* Automatic Duplex Detection */
#define LAN743X_MAC_ASD_ENBL 0x0800U	/* Automatic Speed Detection */
#define LAN743X_MAC_CR_CNTR_RST 0x0020U /* Counter Reset */
#define LAN743X_MAC_CR_MII_EN 0x8000U

#define LAN743X_MAC_ADDR_BASE_L \
	0x11CU /** MAC address lower 4 bytes (read) register **/
#define LAN743X_MAC_ADDR_BASE_H \
	0x118U /** MAC address upper 2 bytes (read) register **/

#define LAN743X_MAC_RX 0x0104U
#define LAN743X_MAC_TX 0x0108U
#define LAN743X_MAC_ENBL ((uint32_t)1U << 0U)
#define LAN743X_MAC_DSBL ((uint32_t)1U << 1U)

/** MAC Statistics **/
#define LAN743X_MAC_RX_FCS_ERRORS 0x1200U
#define LAN743X_MAC_RX_ALIGNMENT_ERRORS 0x1204U
#define LAN743X_MAC_RX_FRAGMENT_ERRORS 0x1208U
#define LAN743X_MAC_RX_JABBER_ERRORS 0x120CU
#define LAN743X_MAC_RX_UNDERSIZE_FRAME_ERRORS 0x1210U
#define LAN743X_MAC_RX_OVERSIZE_FRAME_ERRORS 0x1214U
#define LAN743X_MAC_RX_DROPPED_FRAMES 0x1218U
#define LAN743X_MAC_RX_UNICAST_BYTE 0x121CU
#define LAN743X_MAC_RX_BROADCAST_BYTE 0x1220U
#define LAN743X_MAC_RX_MULTICAST_BYTE 0x1224U
#define LAN743X_MAC_RX_UNICAST_FRAMES 0x1228U
#define LAN743X_MAC_RX_BROADCAST_FRAMES 0x122CU
#define LAN743X_MAC_RX_MULTICAST_FRAMES 0x1230U
#define LAN743X_MAC_RX_PAUSE_FRAMES 0x1234U
#define LAN743X_MAC_RX_TOTAL_FRAMES 0x1254U

/* MAC receive count rollover status register */

#define LAN743X_MAC_RX_CNT_RO_STS 0x127CU

#define RX_EEE_LPI_TIME_RO BIT(23U)
#define RX_EEE_LPI_TRANSITIONS_RO BIT(22U)
#define RX_TOTAL_FRAMES_RO BIT(21U)
#define RX_GT_1518_BYTE_FRAMES_RO BIT(20U)
#define RX_1024_1518_BYTE_FRAMES_RO BIT(19U)
#define RX_512_1023_BYTE_FRAMES_RO BIT(18U)
#define RX_256_511_BYTE_FRAMES_RO BIT(17U)
#define RX_128_256_BYTE_FRAMES_RO BIT(16U)
#define RX_65_127_BYTE_FRAMES_RO BIT(15U)
#define RX_64_BYTE_FRAMES_RO BIT(14U)
#define RX_PAUSE_FRAMES_RO BIT(13U)
#define RX_MULTICAST_FRAMES_RO BIT(12U)
#define RX_BROADCAST_FRAMES_RO BIT(11U)
#define RX_UNICAST_FRAMES_RO BIT(10U)
#define RX_MULTICAST_BYTES_RO BIT(9U)
#define RX_BROADCAST_BYTES_RO BIT(8U)
#define RX_UNICAST_BYTES_RO BIT(7U)
#define RX_DROPPED_FRAMES_RO BIT(6U)
#define RX_OVERSIZE_FRAMES_RO BIT(5U)
#define RX_UNDERSIZE_FRAMES_RO BIT(4U)
#define RX_JABBER_ERRORS_RO BIT(3U)
#define RX_FRAGMENT_ERRORS_RO BIT(2U)
#define RX_ALIGNMENT_ERRORS_RO BIT(1U)
#define RX_FCS_ERRORS_RO BIT(0U)

#define LAN743X_MAC_TX_FCS_ERRORS 0x1280U
#define LAN743X_MAC_TX_EXCESS_DEFER_ERRORS 0x1284U
#define LAN743X_MAC_TX_CARRIER_ERRORS 0x1288U
#define LAN743X_MAC_TX_BAD_BYTES 0x128CU
#define LAN743X_MAC_TX_SINGLE_COLLISIONS 0x1290U
#define LAN743X_MAC_TX_MULTIPLE_COLLISIONS 0x1294U
#define LAN743X_MAC_TX_EXCESSIVE_COLLISIONS 0x1298U
#define LAN743X_MAC_TX_LATE_COLLISIONS 0x129CU
#define LAN743X_MAC_TX_UNICAST_BYTES 0x12A0U
#define LAN743X_MAC_TX_BROADCAST_BYTES 0x12A4U
#define LAN743X_MAC_TX_MULTICAST_BYTES 0x12A8U
#define LAN743X_MAC_TX_UNICAST_FRAMES 0x12ACU
#define LAN743X_MAC_TX_BROADCAST_FRAMES 0x12B0U
#define LAN743X_MAC_TX_MULTICAST_FRAMES 0x12B4U
#define LAN743X_MAC_TX_PAUSE_FRAMES 0x12B8U
#define LAN743X_MAC_TX_TOTAL_FRAMES 0x12D8U

#define LAN743X_MAC_TX_CNT_RO_STS 0x12FCU

#define TX_EEE_LPI_TIME_RO BIT(24U)
#define TX_EEE_LPI_TRANSITIONS_RO BIT(23U)
#define TX_TOTAL_FRAMES_RO BIT(22U)
#define TX_GT_1518_BYTE_FRAMES_RO BIT(21U)
#define TX_1024_1518_BYTE_FRAMES_RO BIT(20U)
#define TX_512_1023_BYTE_FRAMES_RO BIT(19U)
#define TX_256_511_BYTE_FRAMES_RO BIT(18U)
#define TX_128_256_BYTE_FRAMES_RO BIT(17U)
#define TX_65_127_BYTE_FRAMES_RO BIT(16U)
#define TX_64_BYTE_FRAMES_RO BIT(15U)
#define TX_PAUSE_FRAMES_RO BIT(14U)
#define TX_MULTICAST_FRAMES_RO BIT(13U)
#define TX_BROADCAST_FRAMES_RO BIT(12U)
#define TX_UNICAST_FRAMES_RO BIT(11U)
#define TX_MULTICAST_BYTES_RO BIT(10U)
#define TX_BROADCAST_BYTES_RO BIT(9U)
#define TX_UNICAST_BYTES_RO BIT(8U)
#define TX_LATE_COLLISIONS_RO BIT(7U)
#define TX_EXCESSIVE_COLLISIONS_RO BIT(6U)
#define TX_MULTIPLE_COLLISIONS_RO BIT(5U)
#define TX_SINGLE_COLLISIONS_RO BIT(4U)
#define TX_BAD_BYTES_RO BIT(3U)
#define TX_CARRIER_ERRORS_RO BIT(2U)
#define TX_EXCESS_DEFER_ERRORS_RO BIT(1U)
#define TX_FCS_ERRORS_RO BIT(0U)

/* etc. */

/** Recieve Filtering Engine **/
#define LAN743X_RFE_CTL 0x508U
#define LAN743X_RFE_ENABLE_IGMP_CSUM ((uint32_t)1U << 14U)
#define LAN743X_RFE_ENABLE_ICMP_CSUM ((uint32_t)1U << 13U)
#define LAN743X_RFE_ENABLE_L4_CSUM ((uint32_t)1U << 12U)
#define LAN743X_RFE_ENABLE_L3_CSUM ((uint32_t)1U << 11U)
#define LAN743X_RFE_ALLOW_BROADCAST ((uint32_t)1U << 10U)
#define LAN743X_RFE_ALLOW_MULTICAST ((uint32_t)1U << 9U)
#define LAN743X_RFE_ALLOW_UNICAST ((uint32_t)1U << 8U)
#define LAN743X_RFE_ALLOW_PERFECT_FILTER ((uint32_t)1U << 1U)

/** RFE Address Filter Registers **/
#define LAN743X_RFE_ADDR_FILT_HI(_n) (0x0400U + ((_n) * 8U))
#define LAN743X_RFE_ADDR_FILT_LO(_n) (0x0404U + ((_n) * 8U))
#define LAN743X_RFE_ADDR_FILT_HI_VALID ((uint32_t)1U << 31U)
#define LAN743X_RFE_ADDR_FILT_NUM 33U

/** PHY Reset (via power management control) **/
#define LAN743X_PMT_CTL 0x14U /** Power Management Control Register **/
#define LAN743X_PHY_RESET 0x10U
#define LAN743X_PHY_READY 0x80U

/** FIFO Controller **/
#define LAN743X_FCT_TX_CTL 0xC4U
#define LAN743X_FCT_RX_CTL 0xACU

static inline uint32_t
lan743x_make_fct_channel_mask(uint8_t channel, uint8_t mask_offset)
{
	LAN743X_ASSERT(channel < 4U,
	    ("Attempting to use invalid channel: %d\n", channel));
	LAN743X_ASSERT(mask_offset <= 28U,
	    ("Mistake in lan743x_make_fct_channel_mask mask_offset: %d\n",
		mask_offset));
	uint16_t const channel_offset = (uint16_t)mask_offset +
	    (uint16_t)channel;
	return (uint32_t)1U << channel_offset;
}

static inline uint32_t
LAN743X_FCT_ENBL(uint8_t channel)
{
	static const uint8_t fct_en_offset = 28U;
	return lan743x_make_fct_channel_mask(channel, fct_en_offset);
}

static inline uint32_t
LAN743X_FCT_DSBL(uint8_t channel)
{
	static const uint8_t fct_dsbl_offset = 24U;
	return lan743x_make_fct_channel_mask(channel, fct_dsbl_offset);
}

static inline uint32_t
LAN743X_FCT_RESET(uint8_t channel)
{
	static const uint8_t fct_reset_offset = 20U;
	return lan743x_make_fct_channel_mask(channel, fct_reset_offset);
}

/** DMA Controller **/
#define LAN743X_DMAC_CMD 0xC0CU
#define LAN743X_DMAC_RESET ((uint32_t)1U << 31U)
#define LAN743X_DMAC_TX_START 16U
#define LAN743X_DMAC_RX_START 0U

static inline uint32_t
LAN743X_DMAC_CMD_VAL(uint8_t state, uint8_t offset, uint8_t channel)
{
	uint16_t const shift = (uint16_t)state + (uint16_t)offset +
	    (uint16_t)channel;
	LAN743X_ASSERT(shift < 32U,
	    ("LAN743X_DMAC_CMD_VAL: shifting by more than 32: %d\n", shift));
	return (uint32_t)1U << shift;
}

#define LAN743X_DMAC_CMD_RESET(_s, _ch) LAN743X_DMAC_CMD_VAL(_s, 8U, _ch)
#define LAN743X_DMAC_CMD_START(_s, _ch) LAN743X_DMAC_CMD_VAL(_s, 4U, _ch)
#define LAN743X_DMAC_CMD_STOP(_s, _ch) LAN743X_DMAC_CMD_VAL(_s, 0U, _ch)

static inline uint8_t
LAN743X_DMAC_STATE(bool start, bool stop)
{
	return (start ? 2U : 0U) | (stop ? 1U : 0U);
}

#define LAN743X_DMAC_STATE_INITIAL LAN743X_DMAC_STATE(false, false)
#define LAN743X_DMAC_STATE_STARTED LAN743X_DMAC_STATE(true, false)
#define LAN743X_DMAC_STATE_STOP_PENDING LAN743X_DMAC_STATE(true, true)
#define LAN743X_DMAC_STATE_STOPPED LAN743X_DMAC_STATE(false, true)

static inline uint8_t
LAN743X_DMAC_CMD_STATE(struct lan743x_softc *sc, uint8_t state, uint8_t channel)
{
	uint32_t const cmd_reg_val = lan743x_csr_read_32(sc, LAN743X_DMAC_CMD);
	uint32_t const cmd_start_val = cmd_reg_val &
	    LAN743X_DMAC_CMD_START(state, channel);
	uint32_t const cmd_stop_val = cmd_reg_val &
	    LAN743X_DMAC_CMD_STOP(state, channel);
	bool const cmd_start = (cmd_start_val != 0U);
	bool const cmd_stop = (cmd_stop_val != 0U);
	return LAN743X_DMAC_STATE(cmd_start, cmd_stop);
}

#define LAN743X_DMAC_STATE_IS_INITIAL(sc, _s, _ch) \
	(LAN743X_DMAC_CMD_STATE(sc, _s, _ch) == LAN743X_DMAC_STATE_INITIAL)

#define LAN743X_DMAC_INTR_STS 0xC10U
#define LAN743X_DMAC_INTR_ENBL_SET 0xC14U
#define LAN743X_DMAC_INTR_ENBL_CLR 0xC18U
#define LAN743X_DMAC_TX_INTR_ENBL(_ch) ((uint32_t)1U << (_ch))
#define LAN743X_DMAC_RX_INTR_ENBL(_ch) ((uint32_t)1U << (16U + (_ch)))

#define LAN743X_DMA_REG(reg, _channel) \
	((uint32_t)(reg) | ((uint32_t)(_channel) << 6U))

#define LAN743X_DMA_TX_CONFIG0(_channel) LAN743X_DMA_REG(0x0D40U, _channel)
#define LAN743X_DMA_TX_CONFIG1(_channel) LAN743X_DMA_REG(0x0D44U, _channel)
#define LAN743X_DMA_TX_BASE_H(_channel) LAN743X_DMA_REG(0x0D48U, _channel)
#define LAN743X_DMA_TX_BASE_L(_channel) LAN743X_DMA_REG(0x0D4CU, _channel)
#define LAN743X_DMA_TX_HEAD_WB_H(_channel) \
	LAN743X_DMA_REG(0x0D50U, _channel) /* head Writeback */
#define LAN743X_DMA_TX_HEAD_WB_L(_channel) LAN743X_DMA_REG(0x0D54U, _channel)
#define LAN743X_DMA_TX_HEAD(_channel) LAN743X_DMA_REG(0x0D58U, _channel)
#define LAN743X_DMA_TX_TAIL(_channel) LAN743X_DMA_REG(0x0D5CU, _channel)

#define LAN743X_DMA_RX_CONFIG0(_channel) LAN743X_DMA_REG(0x0C40U, _channel)
#define LAN743X_DMA_RX_CONFIG1(_channel) LAN743X_DMA_REG(0x0C44U, _channel)
#define LAN743X_DMA_RX_BASE_H(_channel) LAN743X_DMA_REG(0x0C48U, _channel)
#define LAN743X_DMA_RX_BASE_L(_channel) LAN743X_DMA_REG(0x0C4CU, _channel)
#define LAN743X_DMA_RX_HEAD_WB_H(_channel) \
	LAN743X_DMA_REG(0x0C50U, _channel) /* head Writeback */
#define LAN743X_DMA_RX_HEAD_WB_L(_channel) LAN743X_DMA_REG(0x0C54U, _channel)
#define LAN743X_DMA_RX_HEAD(_channel) LAN743X_DMA_REG(0x0C58U, _channel)
#define LAN743X_DMA_RX_TAIL(_channel) LAN743X_DMA_REG(0x0C5CU, _channel)

#define LAN743X_DMA_RING_LEN_MASK 0xFFFFU
#define LAN743X_DMA_HEAD_WB_IOC_ENBL 0x10000000U
#define LAN743X_DMA_HEAD_WB_LS_ENBL 0x20000000U
#define LAN743X_DMA_HEAD_WB_ENBL ((uint32_t)1U << 5U)
#define LAN743X_DMA_RING_PAD_MASK 0x03000000U
#define LAN743X_DMA_RING_PAD_0 0x00000000U
#define LAN743X_DMA_RING_PAD_2 0x02000000U

/* TX_CFG_A (TX_CONFIG0) field defines — DOS Section 10.10.23 */
#define LAN743X_TX_HP_WB_SWFLUSH       ((uint32_t)1U << 31U)
#define LAN743X_TX_HP_WB_ON_INT_TMR    ((uint32_t)1U << 30U)
#define LAN743X_TX_TMR_HPWB_SEL_SHIFT  28U
#define LAN743X_TX_TMR_HPWB_SEL_MASK   ((uint32_t)0x3U << 28U)
#define LAN743X_TX_PF_THRES_SHIFT      16U
#define LAN743X_TX_PF_PRI_THRES_SHIFT  8U
#define LAN743X_TX_HP_WB_ON_TXTMR      ((uint32_t)1U << 4U)
#define LAN743X_TX_HP_WB_THRES_MASK    ((uint32_t)0xFU)

/* TX Delay Timer Configuration Register — DOS Section 10.10.10 (common) */
#define LAN743X_TXTMR_CFG              0x0C24U
#define LAN743X_TXTMR_WR               ((uint32_t)1U << 19U)
#define LAN743X_TXTMR_CNT_MASK         ((uint32_t)0xFFFFU)

/* TX Absolute Timer Configuration Register — DOS Section 10.10.11 (common) */
#define LAN743X_TX_ABSTMR_CFG          0x0C28U
#define LAN743X_TX_ABSTMR_WR           ((uint32_t)1U << 19U)
#define LAN743X_TX_ABSTMR_CNT_MASK     ((uint32_t)0xFFFFU)

/* RX_CFG_A (RX_CONFIG0) field defines — DOS Section 10.10.13 */
#define LAN743X_RX_WB_THRES_SHIFT      24U
#define LAN743X_RX_PF_THRES_SHIFT      16U
#define LAN743X_RX_PF_PRI_THRES_SHIFT  8U
/* Bit 5 = RX_HP_WB_EN, already defined as LAN743X_DMA_HEAD_WB_ENBL */

/* RX Absolute Timer Configuration Register — DOS Section 10.10.8 (shared) */
#define LAN743X_RX_ABSTMR_CFG          0x0C1CU
#define LAN743X_RX_ABSTMR_WR           ((uint32_t)1U << 19U)
#define LAN743X_RX_ABSTMR_SEL_SHIFT    16U
#define LAN743X_RX_ABSTMR_CNT_MASK     ((uint32_t)0xFFFFU)

/* RX Relative Timer Configuration Register — DOS Section 10.10.9 (shared) */
#define LAN743X_RXRELTMR_CFG           0x0C20U
#define LAN743X_RXRELTMR_WR            ((uint32_t)1U << 19U)
#define LAN743X_RXRELTMR_SEL_SHIFT     16U
#define LAN743X_RXRELTMR_CNT_MASK      ((uint32_t)0xFFFFU)

#define LAN743X_DESC_CTL_DTYPE_DATA ((uint32_t)0U << 30U)
#define LAN743X_DESC_CTL_DTYPE_EXT ((uint32_t)1U << 30U)

#define LAN743X_DESC_CTL_EXT ((uint32_t)1U << 27U)

#define LAN743X_DESC_CTL_IGE ((uint32_t)1U << 23U)
#define LAN743X_DESC_CTL_ICE ((uint32_t)1U << 22U)
#define LAN743X_DESC_CTL_IPE ((uint32_t)1U << 21U)
#define LAN743X_DESC_CTL_TPE ((uint32_t)1U << 20U)

#define LAN743X_DESC_CTL_OWN ((uint32_t)1U << 15U)
#define LAN743X_DESC_CTL_FCS ((uint32_t)1U << 17U)
#define LAN743X_DESC_CTL_IOC ((uint32_t)1U << 26U)
#define LAN743X_TX_DESC_CTL_LS ((uint32_t)1U << 28U)
#define LAN743X_TX_DESC_CTL_FS ((uint32_t)1U << 29U)
#define LAN743X_RX_DESC_CTL_LS ((uint32_t)1U << 30U)
#define LAN743X_RX_DESC_CTL_FS ((uint32_t)1U << 31U)
#define LAN743X_DESC_CTL_BUFLEN_MASK (0x0000FFFFU)
#define LAN743X_DESC_STS_BUFLEN_MASK (0x00003FFFU)
#define LAN743X_DESC_FRAME_LEN_MASK (0x3FFF0000U)
#define LAN743X_DESC_GET_FRAME_LEN(_desc_ctl) \
	(((_desc_ctl) & LAN743X_DESC_FRAME_LEN_MASK) >> 16U)
#define LAN743X_DESC_GET_BUF_LEN(_desc_ctl) \
	((_desc_ctl) & LAN743X_DESC_STS_BUFLEN_MASK)

#define LAN743X_EXT_DESC_CTL_LSO ((uint32_t)1U << 21U)
#define LAN743X_EXT_DESC_CTL_PAYLENGTH_POS 0U
#define LAN743X_EXT_DESC_CTL_PAYLENGTH_MASK (((uint32_t)1U << 20U) - 1U)

/** PHY **/
#define LAN743X_MII_ACCESS 0x120U
#define LAN743X_MII_DATA 0x124U
#define LAN743X_MII_PHY_ADDR_MASK 0x1FU
#define LAN743X_MII_PHY_ADDR_SHIFT 11U
#define LAN743X_MII_REG_ADDR_MASK 0x1FU
#define LAN743X_MII_REG_ADDR_SHIFT 6U
#define LAN743X_MII_READ 0x0U
#define LAN743X_MII_WRITE 0x2U
#define LAN743X_MII_BUSY 0x1U

#define LAN743X_PHY_BASIC_CTL_REG 0U
#define PHY_CTL_AUTO_NEG_MASK 0x1000U

#define LAN743X_PHY_BASIS_STS_REG 1U
#define PHY_STS_ACOMP_MASK 0x0020U

#define LAN743X_PHY_INTR_CSR 27
#define PHY_INTR_LINK_DOWN_MASK 0x0400U
#define PHY_INTR_LINK_UP_MASK 0x0100U
#define PHY_INTR_RX_ERROR_MASK 0x4000U

/** Interrupt registers **/
#define LAN743X_INTR_STS 0x780U
#define LAN743X_INTR_SET 0x784U /* This triggers a particular interrupt */
#define LAN743X_INTR_ENBL_SET 0x788U
#define LAN743X_INTR_STS_ANY (0x1U)
#define LAN743X_INTR_STS_PHY (0x20U)
#define LAN743X_INTR_STS_RX(_channel) ((uint32_t)1U << (24U + (_channel)))
#define LAN743X_INTR_STS_RX_ANY ((uint32_t)0xFU << 24U)
#define LAN743X_INTR_STS_TX(_channel) ((uint32_t)1U << (16U + (_channel)))
#define LAN743X_INTR_STS_TX_ANY ((uint32_t)0xFU << 16U)
#define LAN743X_INTR_STS_TEST ((uint32_t)1U << 9U)
#define LAN743X_INTR_ENBL_CLR 0x78CU

#define LAN743X_INTR_VEC_ENBL_SET 0x794U
#define LAN743X_INTR_VEC_ENBL_CLR 0x798U
#define LAN743X_INTR_VEC_ENBL_AUTO_CLR 0x79CU
#define LAN743X_INTR_VEC_RX_MAP 0x7A0U
#define LAN743X_INTR_VEC_TX_MAP 0x7A4U
#define LAN743X_INTR_VEC_OTHER_MAP 0x7A8U
#define LAN743X_INTR_VEC_MAP(_vsts, _ch) ((_vsts) << ((_ch) << 2U))
#define LAN743X_INTR_VEC_STS(_v) ((uint32_t)1U << (_v))
#define LAN743X_INTR_RX_VEC_STS(_qid) LAN743X_INTR_VEC_STS((_qid) + 1U)
#define LAN743X_INTR_TX_VEC_STS(_qid) LAN743X_INTR_VEC_STS((_qid) + 1U)

#define LAN743X_INT_MOD_MAPX 0x07B0U
#define LAN743X_INT_MOD_CFGX(x__) \
	(0x07C0U + ((x__) * (uint32_t)sizeof(uint32_t)))

#define LAN743X_MICROCHIP_VENDOR_ID 0x1055U
#define LAN743X_LAN7430_DEVICE_ID 0x7430U
#define LAN743X_LAN7431_DEVICE_ID 0x7431U

/* Set the Phy address */
#ifndef LAN743X_LAN7431_PHY_ADDRESS
#define LAN743X_LAN7431_PHY_ADDRESS (1)
#endif

#define LAN743X_MAX_RX_CHANNELS 4U

/* RX_CFG_C register — per-channel auto-clear configuration.
 * (DOS Section 10.10.22, offset 0xC64h per channel) */
#define LAN743X_DMA_RX_CFG_C(_ch) (0x0C64U | ((_ch) << 6U))
#define LAN743X_RX_DMA_INT_STS_AUTO_CLR ((uint32_t)1U << 3U)
#define LAN743X_RX_TOP_INT_EN_AUTO_CLR ((uint32_t)1U << 6U)

/* TX_CFG_C register — per-channel auto-clear configuration.
 * (DOS Section 10.10.32, offset 0xD64h per channel) */
#define LAN743X_DMA_TX_CFG_C(_ch) (0x0D64U | ((_ch) << 6U))
#define LAN743X_TX_DMA_INT_STS_AUTO_CLR ((uint32_t)1U << 3U)
#define LAN743X_TX_TOP_INT_EN_AUTO_CLR ((uint32_t)1U << 6U)

#endif
