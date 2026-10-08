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

#ifndef LAN743X_UTIL_INCLUDE_GUARD
#define LAN743X_UTIL_INCLUDE_GUARD

#include <stdint.h>
#include "lan743x_csr.h"
#include "lan743x_ctx.h"
#include <sys/systm.h>
#include <errno.h>
#include <stdbool.h>

static inline int
lan743x_wait_for_bits(struct lan743x_softc const *sc, uint32_t reg,
    uint32_t set_bits, uint32_t clear_bits)
{
	static const uint32_t timeout_us = 500000U;
	static const uint16_t polling_interval_us = 500U;
	static const uint16_t timeout_iterations = timeout_us /
	    polling_interval_us;

	for (uint16_t i = 0; i < timeout_iterations; i++) {
		/*
		 * XXX: Datasheets states delay should be > 5 microseconds
		 * for device reset.
		 */
		DELAY((int)polling_interval_us);
		uint32_t const val = lan743x_csr_read_32(sc, reg);
		bool const is_bits_set = ((val & set_bits) == set_bits);
		bool const is_bits_clear = ((val & clear_bits) == 0U);
		if (is_bits_set && is_bits_clear) {
			return EOK;
		}
	}

	return EBUSY;
}

static inline bool
lan743x_u32_to_bool(uint32_t val)
{
	return val != 0U;
}

static inline bool
lan743x_u16_to_bool(uint16_t val)
{
	return val != 0U;
}

static inline bool
lan743x_u8_to_bool(uint8_t val)
{
	return val != 0U;
}

#define LAN743X_TRY_ERR(expr__)             \
	{                                   \
		int const err__ = (expr__); \
		if (err__ != EOK) {         \
			return err__;       \
		}                           \
	}

/* A format string to print must be passed as the next argument after expr__. */
#define LAN743X_TRY_ERR_PRINT(sc__, expr__, ...)                             \
	{                                                                    \
		int const err__ = (expr__);                                  \
		if (err__ != EOK) {                                          \
			(void)device_printf((sc__)->dev, __VA_ARGS__);       \
			(void)device_printf((sc__)->dev, "Error code: %i\n", \
			    err__);                                          \
			return err__;                                        \
		}                                                            \
	}

#endif
