// Copyright 2019, Collabora, Ltd.
// Copyright 2026 Zhang
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "util/u_var.h"

#include <stdint.h>

struct u_frame_times_widget
{
	uint64_t previous_timestamp_ns;
	float fps;
	struct u_var_timing *debug_var;
};

static inline void
u_frame_times_widget_init(struct u_frame_times_widget *widget, float, float)
{
	widget->previous_timestamp_ns = 0;
	widget->fps = 0.0f;
	widget->debug_var = NULL;
}

static inline void
u_frame_times_widget_push_sample(struct u_frame_times_widget *widget, uint64_t timestamp_ns)
{
	if (widget->previous_timestamp_ns != 0 && timestamp_ns > widget->previous_timestamp_ns) {
		widget->fps = 1000000000.0f / (float)(timestamp_ns - widget->previous_timestamp_ns);
	}
	widget->previous_timestamp_ns = timestamp_ns;
}

static inline void u_frame_times_widget_teardown(struct u_frame_times_widget *) {}
