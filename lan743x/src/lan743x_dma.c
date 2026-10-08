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

#include "lan743x_dma.h"
#include "lan743x_ring.h"
#include "lan743x_csr.h"
#include "lan743x_util.h"
#include "lan743x_reg_defs.h"
#include "lan743x_assert.h"
#include <sys/mman.h>
#include <sys/sysctl.h>
#include <sys/types.h>

enum lan743x_dmac_cmd { DMAC_RESET, DMAC_START, DMAC_STOP };
enum lan743x_fct_cmd { FCT_RESET, FCT_ENABLE, FCT_DISABLE };

/* Tx DMA moderation tuning parameters */
static const uint8_t tx_pf_thres = 16U;
static const uint8_t tx_pf_pri_thres = 8U;
static const uint8_t tx_writeback_thres = 14U;
static const uint16_t tx_abs_timer_us = 580U;
static const uint16_t tx_delay_timer_us = 31U;

/* Rx DMA moderation tuning parameters */
static const uint8_t rx_wb_thres = 7U;
static const uint8_t rx_pf_thres = 12U;
static const uint8_t rx_pf_pri_thres = 4U;
static const uint16_t rx_abs_timer_us = 523U;
static const uint16_t rx_rel_timer_us = 29U;

static int
lan743x_load_wb_var_physical_offset(struct lan743x_ring_data *rdata)
{
	off_t phys_offset = 0;
	int const offset_result = mem_offset64(&rdata->head_wb, NOFD,
	    sizeof(rdata->head_wb), &phys_offset, NULL);
	if (offset_result != 0) {
		return offset_result;
	}
	if (phys_offset < 0) {
		return EINVAL;
	}
	rdata->head_wb_bus_addr = (bus_addr_t)phys_offset;
	return 0;
}

static int
lan743x_fct_control(struct lan743x_softc const *sc, uint8_t reg,
    uint8_t channel, enum lan743x_fct_cmd cmd)
{
	switch (cmd) {
	case FCT_RESET:
		lan743x_csr_write_32(sc, reg, LAN743X_FCT_RESET(channel));
		return lan743x_wait_for_bits(sc, reg, 0,
		    LAN743X_FCT_RESET(channel));
	case FCT_ENABLE:
		lan743x_csr_write_32(sc, reg, LAN743X_FCT_ENBL(channel));
		return (0);
	case FCT_DISABLE:
	default:
		lan743x_csr_write_32(sc, reg, LAN743X_FCT_DSBL(channel));
		return lan743x_wait_for_bits(sc, reg, 0,
		    LAN743X_FCT_ENBL(channel));
	}
}

static int
lan743x_dmac_reset_control(struct lan743x_softc const *sc, uint8_t start,
    uint8_t channel)
{
	lan743x_csr_write_32(sc, LAN743X_DMAC_CMD,
	    LAN743X_DMAC_CMD_RESET(start, channel));
	return lan743x_wait_for_bits(sc, LAN743X_DMAC_CMD, 0,
	    LAN743X_DMAC_CMD_RESET(start, channel));
}

static int
lan743x_dmac_stop_control(struct lan743x_softc const *sc, uint8_t start,
    uint8_t channel)
{
	lan743x_csr_write_32(sc, LAN743X_DMAC_CMD,
	    LAN743X_DMAC_CMD_STOP(start, channel));
	return lan743x_wait_for_bits(sc, LAN743X_DMAC_CMD,
	    LAN743X_DMAC_CMD_STOP(start, channel),
	    LAN743X_DMAC_CMD_START(start, channel));
}

static int
lan743x_dmac_start_control(struct lan743x_softc const *sc, uint8_t start,
    uint8_t channel)
{
	/*
	 * NOTE: this simplifies the logic, since it will never
	 * try to start in STOP_PENDING, but it also increases work.
	 *
	 * TODO: remove recursion.
	 */
	int const error = lan743x_dmac_stop_control(sc, start, channel);
	if (error != EOK) {
		return error;
	}
	lan743x_csr_write_32(sc, LAN743X_DMAC_CMD,
	    LAN743X_DMAC_CMD_START(start, channel));
	return EOK;
}

static int
lan743x_dmac_control(struct lan743x_softc const *sc, uint8_t start,
    uint8_t channel, enum lan743x_dmac_cmd cmd)
{
	switch (cmd) {
	case DMAC_RESET:
		return lan743x_dmac_reset_control(sc, start, channel);

	case DMAC_START:
		return lan743x_dmac_start_control(sc, start, channel);

	case DMAC_STOP:
		return lan743x_dmac_stop_control(sc, start, channel);

	default:
		return EINVAL;
	}
}

static int
lan743x_rx_dma_set_ring_address(struct lan743x_softc const *sc, uint8_t channel,
    struct lan743x_ring_data *rdata)
{
	if (rdata->ring_bus_addr == 0U) {
		(void)device_printf(sc->dev, "Invalid ring bus addr.\n");
		return EFAULT;
	}
	lan743x_csr_write_32(sc, LAN743X_DMA_RX_BASE_H(channel),
	    lan743x_csr_get_high32_addr(rdata->ring_bus_addr));
	lan743x_csr_write_32(sc, LAN743X_DMA_RX_BASE_L(channel),
	    lan743x_csr_get_low32_addr(rdata->ring_bus_addr));
	return EOK;
}

static void
lan743x_rx_dmac_cfg_absolute_timer(struct lan743x_softc const *sc,
    uint8_t channel, uint16_t value)
{
	uint32_t const reg =
	    LAN743X_RX_ABSTMR_WR |
	    ((uint32_t)channel << LAN743X_RX_ABSTMR_SEL_SHIFT) |
	    ((uint32_t)value & LAN743X_RX_ABSTMR_CNT_MASK);
	lan743x_csr_write_32(sc, LAN743X_RX_ABSTMR_CFG, reg);
}

static void
lan743x_rx_dmac_cfg_relative_timer(struct lan743x_softc const *sc,
    uint8_t channel, uint16_t value)
{
	/* MODE bit defaults to 0 after reset = inactivity timer mode,
	 * which is what we want. No need to explicitly set it. */
	uint32_t const reg =
	    LAN743X_RXRELTMR_WR |
	    ((uint32_t)channel << LAN743X_RXRELTMR_SEL_SHIFT) |
	    ((uint32_t)value & LAN743X_RXRELTMR_CNT_MASK);
	lan743x_csr_write_32(sc, LAN743X_RXRELTMR_CFG, reg);
}

static int
lan743x_rx_dma_configure_head_wb(struct lan743x_softc const *sc,
    uint8_t channel, struct lan743x_ring_data *rdata)
{
	LAN743X_TRY_ERR_PRINT(sc, lan743x_load_wb_var_physical_offset(rdata),
	    "Couldn't get physical address of Rx head wb location.\n");
	lan743x_csr_write_32(sc, LAN743X_DMA_RX_HEAD_WB_H(channel),
	    lan743x_csr_get_high32_addr(rdata->head_wb_bus_addr));
	lan743x_csr_write_32(sc, LAN743X_DMA_RX_HEAD_WB_L(channel),
	    lan743x_csr_get_low32_addr(rdata->head_wb_bus_addr));

	/* RX_CFG_A: head wb enable + DMA thresholds */
	uint32_t const ring_config =
	    LAN743X_DMA_HEAD_WB_ENBL |
	    ((uint32_t)rx_wb_thres << LAN743X_RX_WB_THRES_SHIFT) |
	    ((uint32_t)rx_pf_thres << LAN743X_RX_PF_THRES_SHIFT) |
	    ((uint32_t)rx_pf_pri_thres << LAN743X_RX_PF_PRI_THRES_SHIFT);
	lan743x_csr_write_32(sc, LAN743X_DMA_RX_CONFIG0(channel), ring_config);

	/* Program Rx DMA moderation timers for this channel */
	lan743x_rx_dmac_cfg_absolute_timer(sc, channel, rx_abs_timer_us);
	lan743x_rx_dmac_cfg_relative_timer(sc, channel, rx_rel_timer_us);

	return EOK;
}

static void
lan743x_rx_dma_ring_config1(struct lan743x_softc *sc, uint8_t channel)
{
	uint32_t ring_config = lan743x_csr_read_32(sc,
	    LAN743X_DMA_RX_CONFIG1(channel));
	/*  ring size */
	ring_config &= ~LAN743X_DMA_RING_LEN_MASK;
	ring_config |= (LAN743X_DMA_RING_SIZE & LAN743X_DMA_RING_LEN_MASK);
	/* packet padding  (PAD_2 is better for IP header alignment ...) */
	ring_config &= ~LAN743X_DMA_RING_PAD_MASK;
	ring_config |= (LAN743X_DMA_RING_PAD_0 & LAN743X_DMA_RING_PAD_MASK);
	lan743x_csr_write_32(sc, LAN743X_DMA_RX_CONFIG1(channel), ring_config);
}

static int
lan743x_rx_dma_start(struct lan743x_softc *sc, uint8_t channel)
{
	LAN743X_TRY_ERR_PRINT(sc,
	    lan743x_fct_control(sc, LAN743X_FCT_RX_CTL, channel, FCT_RESET),
	    "Failed to reset RX FCT.\n");
	LAN743X_TRY_ERR_PRINT(sc,
	    lan743x_fct_control(sc, LAN743X_FCT_RX_CTL, channel, FCT_ENABLE),
	    "Failed to enable RX FCT.\n");
	LAN743X_TRY_ERR_PRINT(sc,
	    lan743x_dmac_control(sc, LAN743X_DMAC_RX_START, channel,
		DMAC_START),
	    "Failed to start RX DMAC.\n");
	return EOK;
}

static int
lan743x_dma_rx_set_ring_regs(struct lan743x_softc *sc, uint8_t channel)
{
	struct lan743x_ring_data *const rdata = &sc->rx_ring_data;
	LAN743X_TRY_ERR(lan743x_rx_dma_set_ring_address(sc, channel, rdata));
	LAN743X_TRY_ERR(lan743x_rx_dma_configure_head_wb(sc, channel, rdata));
	lan743x_rx_dma_ring_config1(sc, channel);
	uint32_t const reg_val = lan743x_csr_read_32(sc,
	    LAN743X_DMA_RX_HEAD(channel));
	if (reg_val > (uint32_t)UINT16_MAX) {
		(void)device_printf(sc->dev, "Rx head idx reg too large: %d.\n",
		    reg_val);
		return EINVAL;
	}
	rdata->last_head = (uint16_t)reg_val;
	return EOK;
}

static int
lan743x_dma_rx_ring_init(struct lan743x_softc *sc, uint8_t channel)
{
	LAN743X_TRY_ERR_PRINT(sc,
	    lan743x_dmac_control(sc, LAN743X_DMAC_RX_START, 0, DMAC_RESET),
	    "Failed resetting DMAC");
	LAN743X_ASSERT(LAN743X_DMAC_STATE_IS_INITIAL(sc, LAN743X_DMAC_RX_START,
			   channel),
	    ("Trying to init channels when not in init state\n"));

	/* Zero software ring state before configuring hardware. The softc
	 * persists across module reload, so these fields may carry stale
	 * values. A non-zero head_wb would cause rxd_available to report
	 * descriptors as ready before the hardware has processed them. */
	sc->rx_ring_data.head_wb = 0;
	sc->rx_ring_data.last_head = 0;
	sc->rx_ring_data.last_tail = 0;

	LAN743X_TRY_ERR(lan743x_dma_rx_set_ring_regs(sc, channel));
	LAN743X_TRY_ERR(lan743x_rx_dma_start(sc, channel));
	return EOK;
}

static int
lan743x_dma_tx_dma_reset(struct lan743x_softc *sc, uint8_t channel)
{
	LAN743X_TRY_ERR_PRINT(sc,
	    lan743x_fct_control(sc, LAN743X_FCT_TX_CTL, channel, FCT_RESET),
	    "Failed to reset TX FCT.\n");
	LAN743X_TRY_ERR_PRINT(sc,
	    lan743x_fct_control(sc, LAN743X_FCT_TX_CTL, channel, FCT_ENABLE),
	    "Failed to enable TX FCT.\n");
	LAN743X_TRY_ERR_PRINT(sc,
	    lan743x_dmac_control(sc, LAN743X_DMAC_TX_START, channel,
		DMAC_RESET),
	    "Failed to reset TX DMAC.\n");
	LAN743X_ASSERT(LAN743X_DMAC_STATE_IS_INITIAL(sc, LAN743X_DMAC_TX_START,
			   channel),
	    ("Trying to init channels in not init state\n"));
	return EOK;
}

static int
lan743x_dma_tx_set_ring_address(struct lan743x_softc *sc, uint8_t channel,
    struct lan743x_ring_data *const rdata)
{
	/* write ring address */
	if (rdata->ring_bus_addr == 0U) {
		(void)device_printf(sc->dev, "Invalid ring bus addr.\n");
		return EFAULT;
	}
	lan743x_csr_write_32(sc, LAN743X_DMA_TX_BASE_H(channel),
	    lan743x_csr_get_high32_addr(rdata->ring_bus_addr));
	lan743x_csr_write_32(sc, LAN743X_DMA_TX_BASE_L(channel),
	    lan743x_csr_get_low32_addr(rdata->ring_bus_addr));
	return EOK;
}

static void
lan743x_dma_tx_ring_config1(struct lan743x_softc *sc, uint8_t channel)
{
	/* write ring size */
	uint32_t ring_config = lan743x_csr_read_32(sc,
	    LAN743X_DMA_TX_CONFIG1(channel));
	ring_config &= ~LAN743X_DMA_RING_LEN_MASK;
	ring_config |= (LAN743X_DMA_RING_SIZE & LAN743X_DMA_RING_LEN_MASK);
	lan743x_csr_write_32(sc, LAN743X_DMA_TX_CONFIG1(channel), ring_config);
}

static void
lan743x_tx_dmac_cfg_absolute_timer(struct lan743x_softc const *sc,
    uint16_t value)
{
	uint32_t const reg = LAN743X_TX_ABSTMR_WR |
	    ((uint32_t)value & LAN743X_TX_ABSTMR_CNT_MASK);
	lan743x_csr_write_32(sc, LAN743X_TX_ABSTMR_CFG, reg);
}

static void
lan743x_tx_dmac_cfg_tx_delay(struct lan743x_softc const *sc,
    uint16_t value)
{
	uint32_t const reg = LAN743X_TXTMR_WR |
	    ((uint32_t)value & LAN743X_TXTMR_CNT_MASK);
	lan743x_csr_write_32(sc, LAN743X_TXTMR_CFG, reg);
}

static void
lan743x_dma_tx_ring_config0(struct lan743x_softc *sc, uint8_t channel)
{
	uint32_t const ring_config =
	    LAN743X_DMA_HEAD_WB_LS_ENBL |
	    LAN743X_DMA_HEAD_WB_ENBL |
	    LAN743X_TX_HP_WB_ON_TXTMR |
	    /* TX_PF_THRES must be non-zero for LSO; see Jira STB09_DEV-628 */
	    ((uint32_t)tx_pf_thres << LAN743X_TX_PF_THRES_SHIFT) |
	    ((uint32_t)tx_pf_pri_thres << LAN743X_TX_PF_PRI_THRES_SHIFT) |
	    ((uint32_t)tx_writeback_thres & LAN743X_TX_HP_WB_THRES_MASK);
	sc->tx_cfg_a_cache = ring_config;
	lan743x_csr_write_32(sc, LAN743X_DMA_TX_CONFIG0(channel), ring_config);

	lan743x_tx_dmac_cfg_absolute_timer(sc, tx_abs_timer_us);
	lan743x_tx_dmac_cfg_tx_delay(sc, tx_delay_timer_us);
}

static int
lan743x_dma_tx_set_wb_address(struct lan743x_softc *sc, uint8_t channel,
    struct lan743x_ring_data *const rdata)
{
	/* write head pointer writeback address */
	LAN743X_TRY_ERR_PRINT(sc, lan743x_load_wb_var_physical_offset(rdata),
	    "Couldn't get physical address of Tx head wb location.\n");
	lan743x_csr_write_32(sc, LAN743X_DMA_TX_HEAD_WB_H(channel),
	    lan743x_csr_get_high32_addr(rdata->head_wb_bus_addr));
	lan743x_csr_write_32(sc, LAN743X_DMA_TX_HEAD_WB_L(channel),
	    lan743x_csr_get_low32_addr(rdata->head_wb_bus_addr));
	return EOK;
}

static void
lan743x_dma_tx_set_ring_ptrs(struct lan743x_softc *sc, uint8_t channel,
    struct lan743x_ring_data *const rdata)
{
	uint32_t const reg_val = lan743x_csr_read_32(sc,
	    LAN743X_DMA_TX_HEAD(channel));
	LAN743X_ASSERT(reg_val < ((uint32_t)1U << 16U),
	    ("Tx head idx reg too large: %d.\n", reg_val));
	rdata->last_head = (uint16_t)reg_val;
	LAN743X_ASSERT(rdata->last_head == 0U,
	    ("LAN743X_DMA_TX_HEAD was not reset.\n"));
	rdata->last_tail = 0U;
	lan743x_csr_write_32(sc, LAN743X_DMA_TX_TAIL(channel),
	    rdata->last_tail);
}

static int
lan743x_dma_tx_set_ring_regs(struct lan743x_softc *sc, uint8_t channel)
{
	struct lan743x_ring_data *const rdata = &sc->tx_ring_data;
	LAN743X_TRY_ERR(lan743x_dma_tx_set_ring_address(sc, channel, rdata));
	lan743x_dma_tx_ring_config1(sc, channel);
	lan743x_dma_tx_ring_config0(sc, channel);
	LAN743X_TRY_ERR(lan743x_dma_tx_set_wb_address(sc, channel, rdata));
	lan743x_dma_tx_set_ring_ptrs(sc, channel, rdata);
	return EOK;
}

static int
lan743x_dma_tx_ring_init(struct lan743x_softc *sc, uint8_t channel)
{
	LAN743X_TRY_ERR(lan743x_dma_tx_dma_reset(sc, channel));

	/* Zero software ring state (see comment in rx_ring_init). */
	sc->tx_ring_data.head_wb = 0;
	sc->tx_ring_data.last_head = 0;
	sc->tx_ring_data.last_tail = 0;

	LAN743X_TRY_ERR(lan743x_dma_tx_set_ring_regs(sc, channel));
	LAN743X_TRY_ERR_PRINT(sc,
	    lan743x_dmac_control(sc, LAN743X_DMAC_TX_START, channel,
		DMAC_START),
	    "Failed to start TX DMAC.\n");
	return EOK;
}

int
lan743x_dma_init(struct lan743x_softc *sc)
{
	if_softc_ctx_t scctx = iflib_get_softc_ctx(sc->ctx);

	if (scctx->isc_nrxqsets > 4) {
		return EINVAL;
	}

	if (scctx->isc_ntxqsets > 1) {
		return EINVAL;
	}

	for (uint8_t ch = 0U; ch < (uint8_t)scctx->isc_nrxqsets; ch++) {
		LAN743X_TRY_ERR(lan743x_dma_rx_ring_init(sc, ch));
	}

	for (uint8_t ch = 0U; ch < (uint8_t)scctx->isc_ntxqsets; ch++) {
		LAN743X_TRY_ERR(lan743x_dma_tx_ring_init(sc, ch));
	}

	return EOK;
}

static int
lan743x_dmac_stop_impl(struct lan743x_softc const *sc)
{
	if_softc_ctx_t scctx = iflib_get_softc_ctx(sc->ctx);

	/* XXX: Could potentially timeout */
	for (int i = 0; i < scctx->isc_nrxqsets; i++) {
		LAN743X_TRY_ERR(lan743x_dmac_control(sc, LAN743X_DMAC_RX_START,
		    0, DMAC_STOP));
		LAN743X_TRY_ERR(lan743x_fct_control(sc, LAN743X_FCT_RX_CTL, 0,
		    FCT_DISABLE));
	}
	for (int i = 0; i < scctx->isc_ntxqsets; i++) {
		LAN743X_TRY_ERR(lan743x_dmac_control(sc, LAN743X_DMAC_TX_START,
		    0, DMAC_STOP));
		LAN743X_TRY_ERR(lan743x_fct_control(sc, LAN743X_FCT_TX_CTL, 0,
		    FCT_DISABLE));
	}
	return EOK;
}

void
lan743x_dmac_stop(struct lan743x_softc *sc)
{
	if (lan743x_dmac_stop_impl(sc) != EOK) {
		(void)device_printf(sc->dev, "lan743x_dmac_stop() failed");
	}
}

int
lan743x_dmac_reset(struct lan743x_softc *sc)
{
	(void)device_printf(sc->dev, "enter lan743x_dmac_reset\n");
	lan743x_csr_write_32(sc, LAN743X_DMAC_CMD, LAN743X_DMAC_RESET);
	return lan743x_wait_for_bits(sc, LAN743X_DMAC_CMD, 0,
	    LAN743X_DMAC_RESET);
}

static int
lan743x_set_rx_abs_timer_impl(struct sysctl_oid *oidp, void *arg1,
    intmax_t arg2, struct sysctl_req *req)
{
	struct lan743x_softc *sc = (struct lan743x_softc *)arg1;
	/* Read current hw value (channel 0 is selected by default after
	 * the last write, since we only use channel 0) */
	uint16_t timer_val = (uint16_t)(lan743x_csr_read_32(sc,
	    LAN743X_RX_ABSTMR_CFG) & LAN743X_RX_ABSTMR_CNT_MASK);
	uint16_t const initial_val = timer_val;
	LAN743X_TRY_ERR_PRINT(sc, sysctl_handle_16(oidp, &timer_val, 0, req),
	    "error in lan743x_set_rx_abs_timer sysctl_handle_16()\n");
	if (timer_val == initial_val) {
		return EOK;
	}
	lan743x_rx_dmac_cfg_absolute_timer(sc, 0U, timer_val);
	return EOK;
}

int
lan743x_set_rx_abs_timer(SYSCTL_HANDLER_ARGS)
{
	return lan743x_set_rx_abs_timer_impl(oidp, arg1, arg2, req);
}

static int
lan743x_set_rx_rel_timer_impl(struct sysctl_oid *oidp, void *arg1,
    intmax_t arg2, struct sysctl_req *req)
{
	struct lan743x_softc *sc = (struct lan743x_softc *)arg1;
	uint16_t timer_val = (uint16_t)(lan743x_csr_read_32(sc,
	    LAN743X_RXRELTMR_CFG) & LAN743X_RXRELTMR_CNT_MASK);
	uint16_t const initial_val = timer_val;
	LAN743X_TRY_ERR_PRINT(sc, sysctl_handle_16(oidp, &timer_val, 0, req),
	    "error in lan743x_set_rx_rel_timer sysctl_handle_16()\n");
	if (timer_val == initial_val) {
		return EOK;
	}
	lan743x_rx_dmac_cfg_relative_timer(sc, 0U, timer_val);
	return EOK;
}

int
lan743x_set_rx_rel_timer(SYSCTL_HANDLER_ARGS)
{
	return lan743x_set_rx_rel_timer_impl(oidp, arg1, arg2, req);
}
