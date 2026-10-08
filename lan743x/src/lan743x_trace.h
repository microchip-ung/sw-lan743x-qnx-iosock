/*
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

#ifndef LAN743X_TRACE_H_INCLUDE_GUARD
#define LAN743X_TRACE_H_INCLUDE_GUARD

#if defined(LAN743X_TS_TRACE_ENABLE) && LAN743X_TS_TRACE_ENABLE

#include "lan743x_timestamp_print.h"

#define LAN743X_TS_TRACE(sc__) lan743x_print_timestamp_us(sc__);
#define LAN743X_TS_TRACE_MSG(sc__, msg__) \
	lan743x_print_timestamp_msg_us(sc__, msg__);

#else

#define LAN743X_TS_TRACE(sc__)		  // EMPTY
#define LAN743X_TS_TRACE_MSG(sc__, msg__) // EMPTY

#endif

#if defined(LAN743X_DBG_TRACE_ENABLE) && LAN743X_DBG_TRACE_ENABLE

#include "lan743x_ctx.h"
#include <sys/bus.h>

/* The first element of the variadic ... must be a format string.
 * Subsequent elements would be any optional arguments to be formatted in the
 * format string.
 *
 * Usage:
 * LAN743X_DBG_TRACE(sc, "printing sc->x: %d\n", sc->x); */
#define LAN743X_DBG_TRACE(sc__, ...) \
	(void)device_printf((sc__)->dev, __VA_ARGS__);

#else

#define LAN743X_DBG_TRACE(sc__, ...) // EMPTY

#endif

#endif // LAN743X_TRACE_H_INCLUDE_GUARD
