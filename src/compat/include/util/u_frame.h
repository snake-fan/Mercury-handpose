// Copyright 2026 Zhang
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "xrt/xrt_frame.h"

#ifdef __cplusplus
extern "C" {
#endif

void u_frame_create_one_off(enum xrt_format format, uint32_t width, uint32_t height, struct xrt_frame **out_frame);

#ifdef __cplusplus
}
#endif
