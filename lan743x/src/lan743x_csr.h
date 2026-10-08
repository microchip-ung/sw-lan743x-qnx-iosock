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

#ifndef LAN743X_CSR_INCLUDE_GUARD
#define LAN743X_CSR_INCLUDE_GUARD

#ifndef LAN743X_UNIT_TEST
#include "lan743x_ctx.h"
#include <sys/bus.h>
#endif

#include "lan743x_assert.h"
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef uint32_t lan743x_reg_t;

#ifdef LAN743X_UNIT_TEST
#define LAN743X_CSR_FUNCTION_SCOPE
#define LAN743X_CSR_INLINE_IMPL(func__) ;
#else
#define LAN743X_CSR_FUNCTION_SCOPE static inline
#define LAN743X_CSR_INLINE_IMPL(func__) func__
#endif

// clang-format off

LAN743X_CSR_FUNCTION_SCOPE uint8_t
lan743x_csr_read_8(struct lan743x_softc const *sc, lan743x_reg_t reg) LAN743X_CSR_INLINE_IMPL(
{
	return bus_read_1(sc->regs, reg);
}
)

LAN743X_CSR_FUNCTION_SCOPE void
lan743x_csr_write_8(struct lan743x_softc const *sc, lan743x_reg_t reg, uint8_t val) LAN743X_CSR_INLINE_IMPL(
{
	bus_write_1(sc->regs, reg, val);
}
)

LAN743X_CSR_FUNCTION_SCOPE void
lan743x_csr_set_bits_8(struct lan743x_softc const *sc, lan743x_reg_t reg, uint8_t mask) LAN743X_CSR_INLINE_IMPL(
{
	lan743x_csr_write_8(sc, reg, lan743x_csr_read_8(sc, reg) | mask);
}
)

LAN743X_CSR_FUNCTION_SCOPE uint32_t
lan743x_csr_read_32(struct lan743x_softc const *sc, lan743x_reg_t reg) LAN743X_CSR_INLINE_IMPL(
{
	return bus_read_4(sc->regs, reg);
}
)

LAN743X_CSR_FUNCTION_SCOPE void
lan743x_csr_write_32(struct lan743x_softc const *sc, lan743x_reg_t reg, uint32_t val) LAN743X_CSR_INLINE_IMPL(
{
	bus_write_4(sc->regs, reg, val);
}
)

LAN743X_CSR_FUNCTION_SCOPE void
lan743x_csr_set_bits_32(struct lan743x_softc const *sc, lan743x_reg_t reg, uint32_t mask) LAN743X_CSR_INLINE_IMPL(
{
	lan743x_csr_write_32(sc, reg, lan743x_csr_read_32(sc, reg) | mask);
}
)

LAN743X_CSR_FUNCTION_SCOPE void
lan743x_csr_clr_bits_32(struct lan743x_softc const *sc, lan743x_reg_t reg, uint32_t mask) LAN743X_CSR_INLINE_IMPL(
{
	lan743x_csr_write_32(sc, reg, lan743x_csr_read_32(sc, reg) & ~mask);
}
)

LAN743X_CSR_FUNCTION_SCOPE uint16_t
lan743x_csr_read_16(struct lan743x_softc const *sc, lan743x_reg_t reg) LAN743X_CSR_INLINE_IMPL(
{
	return bus_read_2(sc->regs, reg);
}
)

LAN743X_CSR_FUNCTION_SCOPE void
lan743x_csr_read_block(struct lan743x_softc const *sc, lan743x_reg_t reg, void *dest,
    uint32_t cnt) LAN743X_CSR_INLINE_IMPL(
{
	bus_read_region_1(sc->regs, reg, (uint8_t *)dest, cnt);
}
)

LAN743X_CSR_FUNCTION_SCOPE void
lan743x_csr_write_block(struct lan743x_softc const *sc, lan743x_reg_t reg, void const *src,
    uint32_t cnt) LAN743X_CSR_INLINE_IMPL(
{
	bus_write_region_1(sc->regs, reg, (uint8_t const *)src, cnt);
}
)

static inline uint32_t
lan743x_csr_get_low32_addr(uint64_t const addr)
{
	return (uint32_t)(addr & 0xFFFFFFFFU);
}

// clang-format on

static inline uint32_t
lan743x_csr_get_high32_addr(uint64_t const addr)
{
	uint64_t const hi_mask = (uint64_t)0xFFFFFFFFU << 32U;
	uint64_t const masked = addr & hi_mask;
	return (uint32_t)(masked >> 32U);
}

static inline uint32_t
lan743x_csr_make_mask(uint8_t start_bit, uint8_t field_size)
{
	LAN743X_ASSERT(field_size < 32U,
	    ("lan743x_csr_make_mask; invalid field_size value: %d\n",
		field_size));
	uint32_t const mask = ((uint32_t)1U << field_size) - (uint32_t)1U;
	LAN743X_ASSERT(start_bit < 32U,
	    ("lan743x_csr_make_mask; invalid start_bit value: %d\n",
		start_bit));
	return mask << start_bit;
}

static inline void
lan743x_csr_write_field_32(struct lan743x_softc const *sc, lan743x_reg_t reg,
    uint32_t val, uint8_t start_bit, uint8_t field_size)
{
	uint32_t const initial_val = lan743x_csr_read_32(sc, reg);
	uint32_t const masked_val = initial_val &
	    ~lan743x_csr_make_mask(start_bit, field_size);
	LAN743X_ASSERT(start_bit < 32U,
	    ("lan743x_csr_write_field_32; invalid start_bit value: %d\n",
		start_bit));
	uint32_t const write_val = masked_val | (val << start_bit);
	lan743x_csr_write_32(sc, reg, write_val);
}

static inline uint32_t
lan743x_csr_read_field_32(struct lan743x_softc const *sc, lan743x_reg_t reg,
    uint8_t start_bit, uint8_t field_size)
{
	uint32_t const initial_val = lan743x_csr_read_32(sc, reg);
	uint32_t const masked_val = initial_val &
	    lan743x_csr_make_mask(start_bit, field_size);
	LAN743X_ASSERT(start_bit < 32U,
	    ("lan743x_csr_read_field_32; invalid start_bit value: %d\n",
		start_bit));
	return masked_val >> start_bit;
}

static inline uint16_t
lan743x_csr_read_field_16(struct lan743x_softc const *sc, lan743x_reg_t reg,
    uint8_t start_bit, uint8_t field_size)
{
	LAN743X_ASSERT(field_size < 16U,
	    ("lan743x_csr_read_field_16; invalid field_size value: %d\n",
		field_size));
	return (uint16_t)(lan743x_csr_read_field_32(sc, reg, start_bit,
			      field_size) &
	    0xFFFFU);
}

#if defined(__cplusplus)
}
#endif

#endif
