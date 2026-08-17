// Copyright 2026 Zhang
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "xrt/xrt_frame.h"

#ifdef __cplusplus
extern "C" {
#endif

struct u_sink_debug
{
	struct xrt_frame_sink *sink;
};

static inline void u_sink_debug_init(struct u_sink_debug *sink) { sink->sink = NULL; }
static inline bool u_sink_debug_is_active(struct u_sink_debug *sink) { return sink->sink != NULL; }
static inline void
u_sink_debug_push_frame(struct u_sink_debug *sink, struct xrt_frame *frame)
{
	if (sink->sink != NULL) {
		xrt_sink_push_frame(sink->sink, frame);
	}
}
static inline void u_sink_debug_destroy(struct u_sink_debug *sink) { sink->sink = NULL; }

#ifdef __cplusplus
}
#endif
