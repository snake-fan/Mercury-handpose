// Copyright 2022, Collabora, Ltd.
// Copyright 2026 Zhang
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "xrt/xrt_defines.h"

#ifdef __cplusplus
extern "C" {
#endif

struct u_worker_thread_pool
{
	struct xrt_reference reference;
	uint32_t thread_count;
	void *implementation;
};

struct u_worker_group
{
	struct xrt_reference reference;
	struct u_worker_thread_pool *pool;
	void *implementation;
};

typedef void (*u_worker_group_func_t)(void *);

struct u_worker_thread_pool *
u_worker_thread_pool_create(uint32_t starting_worker_count, uint32_t thread_count, const char *prefix);
void u_worker_thread_pool_destroy(struct u_worker_thread_pool *pool);
struct u_worker_group *u_worker_group_create(struct u_worker_thread_pool *pool);
void u_worker_group_push(struct u_worker_group *group, u_worker_group_func_t function, void *data);
void u_worker_group_wait_all(struct u_worker_group *group);
void u_worker_group_destroy(struct u_worker_group *group);

static inline void
u_worker_thread_pool_reference(struct u_worker_thread_pool **dst, struct u_worker_thread_pool *src)
{
	struct u_worker_thread_pool *old_dst = *dst;
	if (old_dst == src) {
		return;
	}
	if (src != NULL) {
		xrt_reference_inc(&src->reference);
	}
	*dst = src;
	if (old_dst != NULL && xrt_reference_dec_and_is_zero(&old_dst->reference)) {
		u_worker_thread_pool_destroy(old_dst);
	}
}

static inline void
u_worker_group_reference(struct u_worker_group **dst, struct u_worker_group *src)
{
	struct u_worker_group *old_dst = *dst;
	if (old_dst == src) {
		return;
	}
	if (src != NULL) {
		xrt_reference_inc(&src->reference);
	}
	*dst = src;
	if (old_dst != NULL && xrt_reference_dec_and_is_zero(&old_dst->reference)) {
		u_worker_group_destroy(old_dst);
	}
}

#ifdef __cplusplus
}
#endif
