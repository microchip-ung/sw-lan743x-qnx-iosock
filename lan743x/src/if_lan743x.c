/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 The FreeBSD Foundation, Inc.
 *
 * This driver was written by Gerald ND Aryeetey <gndaryee@uwaterloo.ca>
 * under sponsorship from the FreeBSD Foundation.
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
/*
 * Microchip LAN7430/LAN7431 PCIe to Gigabit Ethernet Controller driver.
 *
 * Product information:
 * LAN7430 www.microchip.com/wwwproducts/en/LAN7430
 *   - Integrated IEEE 802.3 compliant PHY
 * LAN7431 www.microchip.com/wwwproducts/en/LAN7431
 *   - RGMII Interface
 *
 * This driver uses the iflib interface and the default 'ukphy' PHY driver.
 *
 * UNIMPLEMENTED FEATURES
 * ----------------------
 * A number of features supported by LAN743X device are not yet implemented in
 * this driver:
 *
 * - Multiple (up to 4) RX queues support
 *   - Just needs to remove asserts and malloc multiple `rx_ring_data`
 *     structs based on ncpus.
 * - RX/TX Checksum Offloading support
 * - VLAN support
 * - Wake on LAN (WoL) support
 * - TX LSO support
 * - Recieve Side Scaling (RSS) support
 * - Debugging Capabilities:
 *   - Could include MAC statistics and
 *     error status registers in sysctl.
 */

#include "lan743x_iflib.h"
#include <sys/endian.h>
#include <machine/resource.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <net/if_media.h>
#include <dev/mii/miivar.h>
#include <dev/mii/mii.h>

#include "ifdi_if.h"
#include "miibus_if.h"

#include <qnx/qnx_modload.h>
#include <sys/sysctl.h>
#include <stdbool.h>

#include <sys/mman.h>
#include <net/if_dl.h>
#include "lan743x_stats.h"

#include "lan743x_ctx.h"
#include "lan743x_csr.h"
#include "lan743x_mii.h"
#include "lan743x_reg_defs.h"
#include "lan743x_assert.h"
#include "lan743x_intr.h"
#include "lan743x_rx.h"
#include "lan743x_tx.h"
#include "lan743x_pci.h"
#include "lan743x_util.h"
#include "lan743x_stats.h"
#include "lan743x_dma.h"
#include "lan743x_media.h"
#include <string.h>
#include <errno.h>
#include "lan743x_rfe.h"

int drvr_ver = (int)IOSOCK_VERSION_CUR;

SYSCTL_INT(_qnx_driver, OID_AUTO, lan743x_drvr, CTLFLAG_RD, &drvr_ver, 0,
    "Version");

struct _iosock_module_version iosock_module_version =
    IOSOCK_MODULE_VER_SYM_INIT;

static void lan743x_get_hw_ethaddr(struct lan743x_softc const *sc,
    struct ether_addr *dest);
static void lan743x_set_hw_ethaddr(struct lan743x_softc *sc,
    struct ether_addr const *src);
static int lan743x_mac_init(struct lan743x_softc *sc);
static int lan743x_hw_reset(struct lan743x_softc *sc);
static int lan743x_hw_init(struct lan743x_softc *sc);
static int lan743x_hw_teardown(struct lan743x_softc *sc);
static int lan743x_detach(if_ctx_t ctx);
static void lan743x_queues_free(if_ctx_t ctx);
static void lan743x_stop(if_ctx_t ctx);
static void lan743x_update_admin_status(if_ctx_t ctx);
static void lan743x_set_phy_address(struct lan743x_softc *sc);
static int lan743x_phy_reset(struct lan743x_softc *sc);
static void *lan743x_register(device_t dev);
static int lan743x_attach_pre(if_ctx_t ctx);
static int lan743x_attach_post(if_ctx_t ctx);
static void lan743x_init(if_ctx_t ctx);
static void lan743x_if_timer(if_ctx_t ctx, uint16_t txqid);

/*********************************************************************
 *  FreeBSD Device Interface Entry Points
 *********************************************************************/
// clang-format off
static device_method_t lan743x_methods[] = {
	/* Device interface */
	DEVMETHOD(device_register, lan743x_register),
	DEVMETHOD(device_probe, iflib_device_probe),
	DEVMETHOD(device_attach, iflib_device_attach),
	DEVMETHOD(device_detach, iflib_device_detach),
	DEVMETHOD(device_shutdown, iflib_device_shutdown),
	DEVMETHOD(device_suspend, iflib_device_suspend),
	DEVMETHOD(device_resume, iflib_device_resume),

	/* MII Interface */
	DEVMETHOD(miibus_readreg, lan743x_miibus_readreg),
	DEVMETHOD(miibus_writereg, lan743x_miibus_writereg),
	DEVMETHOD(miibus_statchg, lan743x_miibus_statchg),
	DEVMETHOD(miibus_linkchg, lan743x_miibus_linkchg),

	DEVMETHOD_END
};

static driver_t lan743x_driver = {
	.name = "lan743x",
	.methods = lan743x_methods,
	.size = sizeof(struct lan743x_softc),
	.baseclasses = NULL,
	.refs = 0,
	.ops = NULL

};

static pci_vendor_info_t lan743x_vendor_info_array[] = {
	PVID(LAN743X_MICROCHIP_VENDOR_ID, LAN743X_LAN7430_DEVICE_ID,
	    "Microchip LAN7430 PCIe Gigabit Ethernet Controller"),
	PVID(LAN743X_MICROCHIP_VENDOR_ID, LAN743X_LAN7431_DEVICE_ID,
	    "Microchip LAN7431 PCIe Gigabit Ethernet Controller"),
	PVID_END
};

static devclass_t lan743x_devclass;
DRIVER_MODULE(lan743x, pci, lan743x_driver, lan743x_devclass, NULL, NULL);
IFLIB_PNP_INFO(pci, lan743x, lan743x_vendor_info_array)
MODULE_VERSION(lan743x, 1);

/* Module uninit stub: creates the set_sysuninit_set ELF section, which
 * the QNX module loader requires for proper cleanup during module unload.
 * Without this section, stale state (e.g. iflib's cpu_offset_mtx) persists
 * across unload/reload, causing a mutex panic on the next mount. */
static void lan743x_uninit(void *arg __unused) { }
SYSUNINIT(lan743x_uninit, SI_SUB_INIT_IF, SI_ORDER_FIRST, lan743x_uninit, NULL);

MODULE_DEPEND(lan743x, pci, 1, 1, 1);
MODULE_DEPEND(lan743x, ether, 1, 1, 1);
MODULE_DEPEND(lan743x, miibus, 1, 1, 1);
MODULE_DEPEND(lan743x, iflib, 1, 1, 1);

static struct if_txrx lan743x_txrx = {
	.ift_txd_encap = lan743x_isc_txd_encap,
	.ift_txd_flush = lan743x_isc_txd_flush,
	.ift_txd_credits_update = lan743x_isc_txd_credits_update,
	.ift_rxd_available = lan743x_isc_rxd_available,
	.ift_rxd_pkt_get = lan743x_isc_rxd_pkt_get,
	.ift_rxd_refill = lan743x_isc_rxd_refill,
	.ift_rxd_flush = lan743x_isc_rxd_flush,

	.ift_legacy_intr = lan743x_legacy_intr

};

static void *
lan743x_register(device_t dev)
{
	static device_method_t lan743x_iflib_methods[] = {
		DEVMETHOD(ifdi_attach_pre, lan743x_attach_pre),
		DEVMETHOD(ifdi_attach_post, lan743x_attach_post),
		DEVMETHOD(ifdi_detach, lan743x_detach),

		DEVMETHOD(ifdi_init, lan743x_init),
		DEVMETHOD(ifdi_stop, lan743x_stop),

		DEVMETHOD(ifdi_tx_queues_alloc, lan743x_tx_queues_alloc),
		DEVMETHOD(ifdi_rx_queues_alloc, lan743x_rx_queues_alloc),
		DEVMETHOD(ifdi_queues_free, lan743x_queues_free),

		DEVMETHOD(ifdi_msix_intr_assign, lan743x_msix_intr_assign),
		DEVMETHOD(ifdi_tx_queue_intr_enable, lan743x_tx_queue_intr_enable),
		DEVMETHOD(ifdi_rx_queue_intr_enable, lan743x_rx_queue_intr_enable),
		DEVMETHOD(ifdi_intr_enable, lan743x_intr_enable_all),
		DEVMETHOD(ifdi_intr_disable, lan743x_intr_disable_all),

		DEVMETHOD(ifdi_get_counter, lan743x_get_counter),
		DEVMETHOD(ifdi_update_admin_status, lan743x_update_admin_status),
		DEVMETHOD(ifdi_timer, lan743x_if_timer),

		DEVMETHOD(ifdi_multi_set, lan743x_rfe_multi_set),
		DEVMETHOD(ifdi_promisc_set, lan743x_rfe_promisc_set),

		DEVMETHOD_END
	};

	static driver_t lan743x_iflib_driver = {
		.name = "lan743x",
		.methods = lan743x_iflib_methods,
		.size = sizeof(struct lan743x_softc),
		.baseclasses = NULL,
		.refs = 0,
		.ops = NULL
	};

	static struct if_shared_ctx lan743x_sctx_init = {
		.isc_magic = IFLIB_MAGIC,

		.isc_q_align = PAGE_SIZE,
		.isc_admin_intrcnt = 1,
		.isc_flags =
		    IFLIB_DRIVER_MEDIA /* | IFLIB_HAS_RXCQ | IFLIB_HAS_TXCQ*/,

		.isc_vendor_info = lan743x_vendor_info_array,
		.isc_driver_version = "0.1.1",
		.isc_driver = &lan743x_iflib_driver,
		/* 1 queue per set for TX (ring queue) */
		.isc_ntxqs = 1,

		.isc_tx_maxsize = LAN743X_TX_MAXSIZE,
		/* .isc_tx_nsegments = LAN743X_DMA_MAXSEGS, */
		.isc_tx_maxsegsize = LAN743X_TX_MAXSEGSIZE,

		.isc_ntxd_min = { (int)LAN743X_DMA_RING_SIZE_MIN, 0,0,0,0,0,0,0 },
		.isc_ntxd_max = { (int)LAN743X_DMA_RING_SIZE, 0,0,0,0,0,0,0 },
		.isc_ntxd_default = { (int)LAN743X_DMA_RING_SIZE, 0,0,0,0,0,0,0 },

		/* 1 queue per set for RX (ring queue) */
		.isc_nrxqs = 1,

		.isc_rx_maxsize = MJUM9BYTES,
		.isc_rx_nsegments = (int)32U,
		.isc_rx_maxsegsize = MJUM9BYTES,

		.isc_nrxd_min = { (int)LAN743X_DMA_RING_SIZE_MIN, 0,0,0,0,0,0,0 },
		.isc_nrxd_max = { (int)LAN743X_DMA_RING_SIZE, 0,0,0,0,0,0,0 },
		.isc_nrxd_default = { (int)LAN743X_DMA_RING_SIZE, 0,0,0,0,0,0,0 },

		.isc_nfl = 1, /*one free list since there is only one queue */

		.isc_tso_maxsize = LAN743X_TSO_MAXSIZE,
		.isc_tso_maxsegsize = LAN743X_TSO_MAXSEGSIZE,

		.__spare0__ = 0,
		.isc_parse_devinfo = NULL,
		.isc_tx_reclaim_thresh = 0,
		.isc_name = NULL
	};

	return (&lan743x_sctx_init);
}

// clang-format on

/*********************************************************************/

static if_softc_ctx_t
lan743x_init_scctx(if_ctx_t ctx)
{
	if_softc_ctx_t scctx = iflib_get_softc_ctx(ctx);

	/* IFLIB required setup */
	scctx->isc_txrx = &lan743x_txrx;
	scctx->isc_tx_nsegments = (int)LAN743X_DMA_MAXSEGS;
	/* Ring desc queues */
	int const ntxd = scctx->isc_ntxd[0];
	if (ntxd < 0) {
		return NULL;
	}
	scctx->isc_txqsizes[0] = (uint32_t)sizeof(struct lan743x_ring_desc) *
	    (uint32_t)ntxd;
	int const nrxd = scctx->isc_nrxd[0];
	if (nrxd < 0) {
		return NULL;
	}
	scctx->isc_rxqsizes[0] = (uint32_t)sizeof(struct lan743x_ring_desc) *
	    (uint32_t)nrxd;
	/* XXX: Must have 1 txqset, but can have up to 4 rxqsets */
	scctx->isc_nrxqsets = 1;
	scctx->isc_ntxqsets = 1;
	scctx->isc_tx_csum_flags = CSUM_IP | CSUM_TCP | CSUM_UDP | CSUM_TSO;
	static const int caps = IFCAP_RXCSUM | IFCAP_TXCSUM | IFCAP_TSO4;
	scctx->isc_capabilities = caps;
	scctx->isc_capenable = caps;
	scctx->isc_tx_tso_segments_max = scctx->isc_tx_nsegments;
	scctx->isc_tx_tso_size_max = (int)LAN743X_TSO_MAXSIZE;
	uint32_t const tso_maxsegsize = LAN743X_TSO_MAXSEGSIZE;
	scctx->isc_tx_tso_segsize_max = (int)tso_maxsegsize;
	return scctx;
}

static int
lan743x_setup_mii_bar(if_ctx_t ctx, struct lan743x_softc *sc,
    if_softc_ctx_t scctx)
{
	(void)device_printf(sc->dev, "enter lan743x_setup_mii_bar\n");
	/* XXX: Would be nice(r) if locked methods were here */
	LAN743X_TRY_ERR_PRINT(sc,
	    mii_attach(sc->dev, &sc->miibus, iflib_get_ifp(ctx),
		lan743x_media_change, lan743x_media_status, BMSR_DEFCAPMASK,
		sc->phyaddr, MII_OFFSET_ANY, MIIF_DOPAUSE),
	    "Failed to attach MII interface\n");
	struct mii_data *miid = device_get_softc(sc->miibus);
	scctx->isc_media = &miid->mii_media;
	scctx->isc_msix_bar = pci_msix_table_bar(sc->dev);
	return EOK;
}

static int
lan743x_setup_pba_bar(struct lan743x_softc *sc, if_softc_ctx_t scctx)
{
	(void)device_printf(sc->dev, "enter lan743x_setup_pba_bar\n");
	/** Setup PBA BAR **/
	int rid = pci_msix_pba_bar(sc->dev);
	if (rid != scctx->isc_msix_bar) {
		sc->pba = bus_alloc_resource_any(sc->dev, SYS_RES_MEMORY, &rid,
		    RF_ACTIVE);
		if (sc->pba == NULL) {
			(void)device_printf(sc->dev,
			    "Failed to setup PBA BAR\n");
			return ENXIO;
		}
	}
	return EOK;
}

static int
lan743x_setup_regs(if_ctx_t ctx, struct lan743x_softc *sc, if_softc_ctx_t scctx)
{
	(void)device_printf(sc->dev, "enter lan743x_setup_regs\n");
	/* get the BAR */
	LAN743X_TRY_ERR_PRINT(sc, lan743x_alloc_regs(sc),
	    "Unable to allocate bus resource: registers.\n");
	LAN743X_TRY_ERR(lan743x_test_bar(sc));
	LAN743X_TRY_ERR_PRINT(sc, lan743x_hw_init(sc),
	    "LAN743X device init failed.\n");
	lan743x_set_phy_address(sc);
	LAN743X_TRY_ERR(lan743x_setup_mii_bar(ctx, sc, scctx));
	LAN743X_TRY_ERR(lan743x_setup_pba_bar(sc, scctx));
	return EOK;
}


static struct lan743x_softc *
lan743x_sc_ctor(if_ctx_t ctx)
{
	struct lan743x_softc *sc = iflib_get_softc(ctx);
	sc->ctx = ctx;
	sc->dev = iflib_get_dev(ctx);
	return sc;
}

static bool
is_valid_ether_addr(uint8_t const *ether_addr)
{
	if (ether_addr == NULL) {
		return false;
	}
	return !(ETHER_IS_ZERO(ether_addr));
}

static void
copy_ether_addr(void const *src, struct ether_addr *dst)
{
	bcopy(src, dst->octet, ETHER_ADDR_LEN);
}

static uint8_t const *
get_if_mac_addr(if_t const ifp)
{
	if (ifp->if_addr == NULL) {
		return NULL;
	}
	return (uint8_t const *)IF_LLADDR(ifp);
}

static struct ether_addr
lan743x_get_new_mac_addr(struct lan743x_softc *sc)
{
	// MAC address source priority:
	// 1. ifconfig
	// 2. value in h/w registers on POR
	// 3. random

	// therefore, the logic will be:
	// a. If 'ifaddr' is already configured (nonzero), then use that.
	// b. If 'hwaddr' is already configured (nonzero), then use that.
	// c. Otherwise, generate random.

	struct ether_addr ret;
	if_t ifp = iflib_get_ifp(sc->ctx);
	uint8_t const *ifaddr = get_if_mac_addr(ifp);

	if (is_valid_ether_addr(ifaddr)) {
		copy_ether_addr(ifaddr, &ret);
		return ret;
	}

	struct ether_addr hwaddr;
	lan743x_get_hw_ethaddr(sc, &hwaddr);
	if (is_valid_ether_addr(hwaddr.octet)) {
		copy_ether_addr(hwaddr.octet, &ret);
		return ret;
	}

	// none available from 'if' or hardware; need to generate
	ether_gen_addr(ifp, &ret);
	return ret;
}

static void
lan743x_setup_mac_addr(struct lan743x_softc *sc)
{
	struct ether_addr new_addr = lan743x_get_new_mac_addr(sc);
	lan743x_set_hw_ethaddr(sc, &new_addr);
	iflib_set_mac(sc->ctx, new_addr.octet);
}

static int
lan743x_attach_pre_impl(if_ctx_t ctx)
{
	struct lan743x_softc *sc = lan743x_sc_ctor(ctx);
	(void)device_printf(sc->dev, "enter lan743x_attach_pre_impl\n");
	lan743x_init_cycle_cnt(&sc->ts_ctx);
	if_softc_ctx_t scctx = lan743x_init_scctx(ctx);
	if (scctx == NULL) {
		(void)device_printf(sc->dev, "Error in lan743x_init_scctx\n");
		return EINVAL;
	}
	LAN743X_TRY_ERR(lan743x_setup_regs(ctx, sc, scctx));
	lan743x_setup_mac_addr(sc);
	lan743x_init_intr_config (sc);

	return EOK;
}

static int
lan743x_attach_pre(if_ctx_t ctx)
{
	int result = lan743x_attach_pre_impl(ctx);
	if (result != EOK) {
		/* Ignore error in this case; it's better to return the first
		 * error. */
		(void)lan743x_detach(ctx);
	}
	return result;
}

static void
lan743x_add_sysctls(struct lan743x_softc *sc)
{
	struct sysctl_ctx_list *ctx_list = device_get_sysctl_ctx(sc->dev);
	struct sysctl_oid_list *child = SYSCTL_CHILDREN(
	    device_get_sysctl_tree(sc->dev));

	SYSCTL_ADD_PROC(ctx_list, child, OID_AUTO, "rx_abs_timer_us",
	    CTLTYPE_U16 | CTLFLAG_RW | CTLFLAG_NEEDGIANT, sc, 0,
	    lan743x_set_rx_abs_timer, "IU",
	    "Rx DMA absolute timer in microseconds (0=disabled)");

	SYSCTL_ADD_PROC(ctx_list, child, OID_AUTO, "rx_rel_timer_us",
	    CTLTYPE_U16 | CTLFLAG_RW | CTLFLAG_NEEDGIANT, sc, 0,
	    lan743x_set_rx_rel_timer, "IU",
	    "Rx DMA relative (inactivity) timer in microseconds (0=disabled)");
}

static int
lan743x_attach_post(if_ctx_t ctx)
{
	struct lan743x_softc *sc = iflib_get_softc(ctx);

	lan743x_stats_init(sc);
	lan743x_add_sysctls(sc);
	return (0);
}

static int
lan743x_bus_release_resource(struct lan743x_softc *sc)
{
	int error = EOK;
	if (sc->pba != NULL) {
		error = bus_release_resource(sc->dev, SYS_RES_MEMORY,
		    rman_get_rid(sc->pba), sc->pba);
	}
	sc->pba = NULL;
	return error;
}

static int
lan743x_device_release_child(struct lan743x_softc *sc)
{
	int error = EOK;
	if (sc->miibus != NULL) {
		error = device_delete_child(sc->dev, sc->miibus);
	}
	return error;
}

static int
lan743x_detach(if_ctx_t ctx)
{
	struct lan743x_softc *sc = iflib_get_softc(ctx);

	(void)device_printf(sc->dev, "lan743x_detach()\n");
	/* XXX: Should report errors but still detach everything. */
	int const teardown_result = lan743x_hw_teardown(sc);
	if (teardown_result != EOK) {
		(void)device_printf(sc->dev, "Error during h/w teardown: %i\n",
		    teardown_result);
	}
	/* Release IRQs */
	iflib_irq_free(ctx, &sc->rx_irq);
	iflib_irq_free(ctx, &sc->admin_irq);
	int const dev_release_child_result = lan743x_device_release_child(sc);
	if (dev_release_child_result != EOK) {
		(void)device_printf(sc->dev,
		    "Error releasing device child: %i\n",
		    dev_release_child_result);
	}
	int const release_resource_result = lan743x_bus_release_resource(sc);
	if (release_resource_result != EOK) {
		(void)device_printf(sc->dev,
		    "Error releasing bus resource: %i\n",
		    release_resource_result);
	}
	int const release_regs_result = lan743x_release_regs(sc);
	if (release_regs_result != EOK) {
		(void)device_printf(sc->dev, "Error releasing regs: %i\n",
		    release_regs_result);
	}
	LAN743X_TRY_ERR(teardown_result);
	LAN743X_TRY_ERR(dev_release_child_result);
	LAN743X_TRY_ERR(release_resource_result);
	LAN743X_TRY_ERR(release_regs_result);
	return EOK;
}

static void
lan743x_queues_free(if_ctx_t ctx)
{
	struct lan743x_softc *sc = iflib_get_softc(ctx);

	(void)memset(&sc->rx_ring_data, 0, sizeof(struct lan743x_ring_data));
	(void)memset(&sc->tx_ring_data, 0, sizeof(struct lan743x_ring_data));
}

static void
lan743x_clr_mac_enable(struct lan743x_softc *sc, uint16_t const reg_addr,
    uint32_t const prev_reg_val)
{
	// clear the enable bit (also don't write 1 to the W1C disabled status bit to avoid maybe clearing that inadvertently)
	uint32_t reg_wr_val = prev_reg_val;
	reg_wr_val &= ~LAN743X_MAC_ENBL;
	reg_wr_val &= ~LAN743X_MAC_DSBL;
	lan743x_csr_write_32(sc, reg_addr, reg_wr_val);
}

/* 
 * Disables the mac and waits for the disabled status bit to be set.
 * 
 * Pass the address of the MACRX or MAC_TX register to reg_addr.
 */
static int
lan743x_disable_mac(struct lan743x_softc *sc, uint16_t const reg_addr)
{
	uint32_t const initial_reg_value = lan743x_csr_read_32(sc, reg_addr);
	lan743x_clr_mac_enable(sc, reg_addr, initial_reg_value);
	return lan743x_wait_for_bits(sc, reg_addr, LAN743X_MAC_DSBL, 0);
}

static int
lan743x_disable_macs(struct lan743x_softc *sc)
{
	int const rx_result = lan743x_disable_mac(sc, LAN743X_MAC_RX);
	int const tx_result = lan743x_disable_mac(sc, LAN743X_MAC_TX);
	LAN743X_TRY_ERR(rx_result);
	return tx_result;
}

static void
lan743x_stop(if_ctx_t ctx)
{
	struct lan743x_softc *sc = iflib_get_softc(ctx);

	(void)device_printf(sc->dev, "running stop ...\n");

	/* Stop DMA channels and disable FIFO controllers */
	lan743x_dmac_stop(sc);

	/* Disable MAC RX then MAC TX (per spec Section 12.1).
	 * Ignore wait errors since the MAC may already be disabled
	 * (defensive stop called by iflib before init). */
	(void)lan743x_disable_macs(sc);
}

static int
lan743x_hw_teardown(struct lan743x_softc *sc)
{
	/* Lite Reset: resets DMA, interrupts, MAC, RFE, FCT — puts all
	 * functional blocks in a known state. Does not affect the PCIe
	 * interface or Ethernet PHY (per spec Section 18.1.7).
	 * Prerequisites (DMA stopped, MACs disabled) are met because
	 * iflib always calls stop before detach. */
	int const reset_result = lan743x_hw_reset(sc);

	/* Reset PHY: LRST does not affect the PHY, so reset it
	 * separately to put it in a known state for the next attach
	 * and prevent stale PHY interrupt events from accumulating. */
	(void)lan743x_phy_reset(sc);

	return reset_result;
}

static void
lan743x_init(if_ctx_t ctx)
{
	struct lan743x_softc *sc = iflib_get_softc(ctx);
	struct mii_data *miid = device_get_softc(sc->miibus);

	(void)device_printf(sc->dev, "running init ...\n");

	/* Perform Soft Lite Reset (LRST) to get hardware into a known state.
	 * LRST resets DMA, interrupts, MAC, RFE, FCT -- everything functional
	 * except PCIe interface and Ethernet PHY (per spec Section 18.1.7).
	 * Prerequisites: DMA stopped and interrupts disabled. These are met
	 * because iflib calls ifdi_intr_disable then ifdi_stop before
	 * ifdi_init. */
	int const hw_reset_result = lan743x_hw_reset(sc);
	if (hw_reset_result != EOK) {
		(void)device_printf(sc->dev, "hw_reset failed: %i\n",
		    hw_reset_result);
		return;
	}

	/* Re-init MAC: enable auto duplex/speed detection, enable MAC
	 * TX/RX, setup MII/RGMII interface. */
	int const mac_init_result = lan743x_mac_init(sc);
	if (mac_init_result != EOK) {
		(void)device_printf(sc->dev, "mac_init failed: %i\n",
		    mac_init_result);
		return;
	}

	/* Re-enable PHY interrupts so the link-change state machine gets
	 * fresh notifications. LRST does not reset the Ethernet PHY itself
	 * (per spec Section 18.1.7), so a full PHY reset is not needed here.
	 * Writing the PHY interrupt CSR also clears any stale status. */
	int const phy_intr_result = lan743x_phy_intr_enable(sc);
	if (phy_intr_result != EOK) {
		(void)device_printf(sc->dev, "phy_intr_enable failed: %i\n",
		    phy_intr_result);
	}

	/* Re-apply interrupt vector mappings and RX moderation config
	 * that LRST wiped (LRST resets the interrupt controller). */
	lan743x_init_intr_config(sc);

	/* Re-init DMA rings (DMAC was reset by LRST) */
	int const dma_init_result = lan743x_dma_init(sc);
	if (dma_init_result != EOK) {
		(void)device_printf(sc->dev, "DMA init failed: %i\n",
		    dma_init_result);
	}
	LAN743X_ASSERT(dma_init_result == EOK,
	    ("LAN743X DMA init failed; err code: %i", dma_init_result));

	/* Re-init receive filtering engine (MAC address filters, etc.) */
	lan743x_rfe_init(sc);

	/* Poll current PHY link status and notify iflib. Since LRST does
	 * not reset the PHY, and we did not restart auto-negotiation, the
	 * existing link state is preserved. Using mii_pollstat instead of
	 * mii_mediachg avoids restarting AN (which would cause a ~3-4s
	 * link drop + re-negotiation cycle). */
	mii_pollstat(miid);
}

static int
lan743x_hw_init(struct lan743x_softc *sc)
{
	(void)device_printf(sc->dev, "enter lan743x_hw_init\n");
	LAN743X_TRY_ERR_PRINT(sc, lan743x_hw_reset(sc), "hw_reset failed");
	LAN743X_TRY_ERR_PRINT(sc, lan743x_mac_init(sc), "mac_init failed");
	// LAN743X_TRY_ERR_PRINT(sc, lan743x_phy_reset(sc), "phy_reset failed");
	lan743x_phy_reset(sc);
	/* TODO: probably move this call to lan743x_intr.c where all of the
	 * other interrupts are enabled/disabled */
	LAN743X_TRY_ERR_PRINT(sc, lan743x_phy_intr_enable(sc),
	    "phy_intr failed");
	LAN743X_TRY_ERR_PRINT(sc, lan743x_dmac_reset(sc), "dmac_reset failed");
	return EOK;
}

static int
lan743x_hw_reset(struct lan743x_softc *sc)
{
	uint32_t val;

	val = lan743x_csr_read_32(sc, LAN743X_PMT_CTL);
	(void)device_printf(sc->dev, "LAN743X_PMT_CTL before reset: 0x%08X\n",
	    val);

	val = lan743x_csr_read_32(sc, LAN743X_HW_CFG);
	(void)device_printf(sc->dev, "HW_CFG before reset: 0x%08X\n", val);

	val = lan743x_csr_read_32(sc, LAN743X_INTR_STS);
	(void)device_printf(sc->dev, "INTR_STS before reset: 0x%08X\n", val);

	lan743x_csr_set_bits_32(sc, LAN743X_HW_CFG, LAN743X_LITE_RESET);

	/* Datasheet: no CSR access for >= 5us after LRST */
	DELAY(10);

	val = lan743x_csr_read_32(sc, LAN743X_PMT_CTL);
	(void)device_printf(sc->dev, "PMT_CTL after LRST+10us: 0x%08X\n", val);

	val = lan743x_csr_read_32(sc, LAN743X_HW_CFG);
	(void)device_printf(sc->dev, "HW_CFG after write+10us: 0x%08X\n", val);

	int const result =
	    lan743x_wait_for_bits(sc, LAN743X_HW_CFG, 0, LAN743X_LITE_RESET);

	if (result != EOK) {
		val = lan743x_csr_read_32(sc, LAN743X_PMT_CTL);
		(void)device_printf(sc->dev,
		    "PMT_CTL at timeout: 0x%08X\n", val);
		val = lan743x_csr_read_32(sc, LAN743X_HW_CFG);
		(void)device_printf(sc->dev,
		    "HW_CFG at timeout: 0x%08X\n", val);
		val = lan743x_csr_read_32(sc, 0);
		(void)device_printf(sc->dev,
		    "ID_REV at timeout: 0x%08X\n", val);

		/*
		 * Test: stop reading for 200ms, then check once.
		 * If polling reads keep the CSR interface busy and
		 * block the reset, this gap should let it complete.
		 */
		(void)device_printf(sc->dev,
		    "Trying 200ms read-free gap...\n");
		DELAY(200000);
		val = lan743x_csr_read_32(sc, LAN743X_PMT_CTL);
		(void)device_printf(sc->dev, "PMT_CTL after gap: 0x%08X\n",
		    val);
		val = lan743x_csr_read_32(sc, LAN743X_HW_CFG);
		(void)device_printf(sc->dev,
		    "HW_CFG after gap: 0x%08X\n", val);
		if ((val & LAN743X_LITE_RESET) == 0U) {
			(void)device_printf(sc->dev,
			    "Reset completed after gap!\n");
			return EOK;
		}
	}

	return result;
}

static int
lan743x_mac_ifc_setup(struct lan743x_softc *sc)
{
	mii_contype_t conn_typ = MII_CONTYPE_RGMII_RXID;
	uint32_t val = 0;

	lan743x_csr_set_bits_32(sc, LAN743X_MAC_CR, LAN743X_MAC_CR_MII_EN);
	lan743x_csr_clr_bits_32(sc, LAN743X_HW_CFG, LAN743X_CLK125_EN);

	if (pci_get_device(sc->dev) != LAN743X_LAN7431_DEVICE_ID)
		return EOK;

	if (mii_contype_is_rgmii(conn_typ)) {
		(void)device_printf(sc->dev, "Setup RGMII interface\n");
		lan743x_csr_clr_bits_32(sc, LAN743X_MAC_CR,
		    LAN743X_MAC_CR_MII_EN);
		lan743x_csr_set_bits_32(sc, LAN743X_HW_CFG, LAN743X_CLK125_EN);

		val = lan743x_csr_read_32(sc, LAN743X_MAC_RGMII_ID);
		val &= ~(LAN743X_MAC_RGMII_ID_RXC_DELAY_EN |
		    LAN743X_MAC_RGMII_ID_TXC_DELAY_EN);
		switch (conn_typ) {
		case MII_CONTYPE_RGMII_TXID:
			(void)device_printf(sc->dev, "RGMII_TXID\n");
			val |= LAN743X_MAC_RGMII_ID_RXC_DELAY_EN;
			break;
		case MII_CONTYPE_RGMII_RXID:
			(void)device_printf(sc->dev, "RGMII_RXID\n");
			val |= LAN743X_MAC_RGMII_ID_TXC_DELAY_EN;
			break;
		case MII_CONTYPE_RGMII:
			(void)device_printf(sc->dev, "RGMII\n");
			val |= (LAN743X_MAC_RGMII_ID_RXC_DELAY_EN |
			    LAN743X_MAC_RGMII_ID_TXC_DELAY_EN);
			break;
		default:
			(void)device_printf(sc->dev, "RGMII_ID\n");
			break;
		}
		lan743x_csr_write_32(sc, LAN743X_MAC_RGMII_ID, val);
	}

	return EOK;
}

static void
lan743x_enable_macs(struct lan743x_softc *sc)
{
	// the set bits function will clear the W1C TXD bit if it had been set previously
	lan743x_csr_set_bits_32(sc, LAN743X_MAC_TX, LAN743X_MAC_ENBL);
	lan743x_csr_set_bits_32(sc, LAN743X_MAC_RX, LAN743X_MAC_ENBL);
}

static int
lan743x_mac_init(struct lan743x_softc *sc)
{
	(void)device_printf(sc->dev, "enter lan743x_mac_init\n");
	/**
	 * enable automatic duplex detection and
	 * automatic speed detection
	 */
	lan743x_csr_set_bits_32(sc, LAN743X_MAC_CR,
	    LAN743X_MAC_ADD_ENBL | LAN743X_MAC_ASD_ENBL);

	lan743x_enable_macs(sc);
	lan743x_mac_ifc_setup(sc);

	return EOK;
}

static void
lan743x_get_hw_ethaddr(struct lan743x_softc const *sc, struct ether_addr *dest)
{
	lan743x_csr_read_block(sc, LAN743X_MAC_ADDR_BASE_L, &dest->octet[0], 4);
	lan743x_csr_read_block(sc, LAN743X_MAC_ADDR_BASE_H, &dest->octet[4], 2);
}

static void
lan743x_set_hw_ethaddr(struct lan743x_softc *sc, struct ether_addr const *src)
{
	lan743x_csr_write_block(sc, LAN743X_MAC_ADDR_BASE_L, &src->octet[0], 4);
	lan743x_csr_write_block(sc, LAN743X_MAC_ADDR_BASE_H, &src->octet[4], 2);
}

static uint32_t
lan743x_get_enabled_intr_sts(struct lan743x_softc const *sc)
{
	uint32_t intr_sts = lan743x_csr_read_32(sc, LAN743X_INTR_STS);
	uint32_t intr_en = lan743x_csr_read_32(sc, LAN743X_INTR_ENBL_SET);
	return intr_sts & intr_en;
}

static void
lan743x_handle_phy_intr(struct lan743x_softc *sc)
{
	(void)device_printf(sc->dev, "lan743x_handle_phy_intr()\n");

	struct mii_data *miid = device_get_softc(sc->miibus);
	// read the status reg to clear the phy interrupt status
	(void)lan743x_miibus_readreg(sc->dev, sc->phyaddr,
	    LAN743X_PHY_INTR_CSR);
	// the below triggers lan743x_miibus_linkchg
	mii_pollstat(miid);
}

/*
 * Admin task handler — called in thread context from iflib when the
 * admin interrupt vector fires.
 */
static void
lan743x_update_admin_status(if_ctx_t ctx)
{
	struct lan743x_softc *sc = iflib_get_softc(ctx);

	(void)device_printf(sc->dev, "lan743x_update_admin_status()\n");

	uint32_t const intr_sts = lan743x_get_enabled_intr_sts(sc);
	bool const phy_intr_status = intr_sts & LAN743X_INTR_STS_PHY;

	if (phy_intr_status) {
		lan743x_handle_phy_intr(sc);
	}
}

static void
lan743x_if_timer(if_ctx_t ctx, uint16_t txqid)
{
	struct lan743x_softc *sc = iflib_get_softc(ctx);

	if (txqid != 0)
		return;

	lan743x_stats_update(sc);
}

static void
load_phy_addr_sysctl_override(struct lan743x_softc *sc)
{
	struct sysctl_ctx_list *ctx_list = device_get_sysctl_ctx(sc->dev);
	struct sysctl_oid_list *child = SYSCTL_CHILDREN(
	    device_get_sysctl_tree(sc->dev));

	SYSCTL_ADD_INT(ctx_list, child, OID_AUTO, "phy_addr",
	    CTLFLAG_RDTUN, &sc->phyaddr, 0,
	    "PHY address (-1 = MII_PHY_ANY)");
}

static void
lan743x_set_phy_address(struct lan743x_softc *sc)
{
	(void)device_printf(sc->dev, "enter lan743x_set_phy_address\n");
	switch (pci_get_device(sc->dev)) {
	case LAN743X_LAN7430_DEVICE_ID:
		sc->phyaddr = 1;
		break;
	case LAN743X_LAN7431_DEVICE_ID:
	default:
		sc->phyaddr = LAN743X_LAN7431_PHY_ADDRESS;
		break;
	}

	load_phy_addr_sysctl_override(sc);

	(void)device_printf(sc->dev, "using phy addr: %d\n", sc->phyaddr);
}

static int
lan743x_phy_reset(struct lan743x_softc *sc)
{
	(void)device_printf(sc->dev, "enter lan743x_phy_reset\n");
	lan743x_csr_set_bits_8(sc, LAN743X_PMT_CTL, LAN743X_PHY_RESET);
	if (lan743x_wait_for_bits(sc, LAN743X_PMT_CTL, 0, LAN743X_PHY_RESET) ==
	    EBUSY) {
		return EBUSY;
	}
	return (
	    lan743x_wait_for_bits(sc, LAN743X_PMT_CTL, LAN743X_PHY_READY, 0));
}
