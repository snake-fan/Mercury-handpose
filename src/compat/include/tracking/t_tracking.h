// Copyright 2019-2023, Collabora, Ltd.
// Copyright 2026 Zhang
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "xrt/xrt_defines.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XRT_DISTORTION_MAX_DIM 14

enum t_camera_distortion_model
{
	T_DISTORTION_OPENCV_RADTAN_5,
	T_DISTORTION_OPENCV_RADTAN_8,
	T_DISTORTION_OPENCV_RADTAN_14,
	T_DISTORTION_FISHEYE_KB4,
	T_DISTORTION_WMR,
};

static inline const char *
t_stringify_camera_distortion_model(enum t_camera_distortion_model model)
{
	switch (model) {
	case T_DISTORTION_OPENCV_RADTAN_5: return "T_DISTORTION_OPENCV_RADTAN_5";
	case T_DISTORTION_OPENCV_RADTAN_8: return "T_DISTORTION_OPENCV_RADTAN_8";
	case T_DISTORTION_OPENCV_RADTAN_14: return "T_DISTORTION_OPENCV_RADTAN_14";
	case T_DISTORTION_FISHEYE_KB4: return "T_DISTORTION_FISHEYE_KB4";
	case T_DISTORTION_WMR: return "T_DISTORTION_WMR";
	}
	return "UNKNOWN";
}

static inline size_t
t_num_params_from_distortion_model(enum t_camera_distortion_model model)
{
	switch (model) {
	case T_DISTORTION_OPENCV_RADTAN_5: return 5;
	case T_DISTORTION_OPENCV_RADTAN_8: return 8;
	case T_DISTORTION_OPENCV_RADTAN_14: return 14;
	case T_DISTORTION_FISHEYE_KB4: return 4;
	case T_DISTORTION_WMR: return 11;
	}
	return 0;
}

struct t_camera_calibration_rt5_params
{
	double k1, k2, p1, p2, k3;
};

struct t_camera_calibration_rt8_params
{
	double k1, k2, p1, p2, k3, k4, k5, k6;
};

struct t_camera_calibration_rt14_params
{
	double k1, k2, p1, p2, k3, k4, k5, k6, s1, s2, s3, s4, tx, ty;
};

struct t_camera_calibration_kb4_params
{
	double k1, k2, k3, k4;
};

struct t_camera_calibration_wmr_params
{
	double k1, k2, p1, p2, k3, k4, k5, k6, codx, cody, rpmax;
};

struct t_camera_calibration
{
	struct xrt_size image_size_pixels;
	double intrinsics[3][3];
	union {
		struct t_camera_calibration_rt5_params rt5;
		struct t_camera_calibration_rt8_params rt8;
		struct t_camera_calibration_rt14_params rt14;
		struct t_camera_calibration_kb4_params kb4;
		struct t_camera_calibration_wmr_params wmr;
		double distortion_parameters_as_array[XRT_DISTORTION_MAX_DIM];
	};
	enum t_camera_distortion_model distortion_model;
};

struct t_stereo_camera_calibration
{
	struct xrt_reference reference;
	struct t_camera_calibration view[2];
	double camera_translation[3];
	double camera_rotation[3][3];
	double camera_essential[3][3];
	double camera_fundamental[3][3];
};

void
t_stereo_camera_calibration_alloc(struct t_stereo_camera_calibration **out_c,
                                  enum t_camera_distortion_model distortion_model);

void
t_stereo_camera_calibration_destroy(struct t_stereo_camera_calibration *calibration);

void
t_stereo_camera_calibration_dump(struct t_stereo_camera_calibration *calibration);

static inline void
t_stereo_camera_calibration_reference(struct t_stereo_camera_calibration **dst,
                                      struct t_stereo_camera_calibration *src)
{
	struct t_stereo_camera_calibration *old_dst = *dst;
	if (old_dst == src) {
		return;
	}
	if (src != NULL) {
		xrt_reference_inc(&src->reference);
	}
	*dst = src;
	if (old_dst != NULL && xrt_reference_dec_and_is_zero(&old_dst->reference)) {
		t_stereo_camera_calibration_destroy(old_dst);
	}
}

#ifdef __cplusplus
}
#endif
