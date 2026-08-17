// Copyright 2026 Zhang
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct u_sink_debug;

struct u_var_draggable_f32
{
	float val;
	float step;
	float min;
	float max;
};

struct u_var_timing
{
	int unused;
};

static inline void u_var_add_root(void *, const char *, bool) {}
static inline void u_var_remove_root(void *) {}
static inline void u_var_add_bool(void *, bool *, const char *) {}
static inline void u_var_add_f32(void *, float *, const char *) {}
static inline void u_var_add_ro_f32(void *, float *, const char *) {}
static inline void u_var_add_i32(void *, int *, const char *) {}
static inline void u_var_add_u64(void *, size_t *, const char *) {}
static inline void u_var_add_draggable_f32(void *, struct u_var_draggable_f32 *, const char *) {}
static inline void u_var_add_f32_timing(void *, struct u_var_timing *, const char *) {}
static inline void u_var_add_sink_debug(void *, struct u_sink_debug *, const char *) {}

#ifdef __cplusplus
}
#endif
