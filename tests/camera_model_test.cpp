// Copyright 2026 Zhang
// SPDX-License-Identifier: BSL-1.0

#include "tracking/t_camera_models.h"

#include <cmath>
#include <iostream>

int
main()
{
	t_camera_model_params model{};
	model.model = T_DISTORTION_FISHEYE_KB4;
	model.fx = 300.0f;
	model.fy = 302.0f;
	model.cx = 320.0f;
	model.cy = 240.0f;
	model.fisheye = {-0.01f, 0.001f, -0.0001f, 0.00001f};

	constexpr float source_x = 0.21f;
	constexpr float source_y = -0.12f;
	constexpr float source_z = 0.97f;
	float pixel_x = 0.0f;
	float pixel_y = 0.0f;
	if (!t_camera_models_project(&model, source_x, source_y, source_z, &pixel_x, &pixel_y)) {
		std::cerr << "KB4 projection failed\n";
		return 1;
	}

	float ray_x = 0.0f;
	float ray_y = 0.0f;
	float ray_z = 0.0f;
	if (!t_camera_models_unproject(&model, pixel_x, pixel_y, &ray_x, &ray_y, &ray_z)) {
		std::cerr << "KB4 unprojection failed\n";
		return 1;
	}

	const float source_length = std::sqrt(source_x * source_x + source_y * source_y + source_z * source_z);
	if (std::abs(ray_x - source_x / source_length) >= 1e-4f ||
	    std::abs(ray_y - source_y / source_length) >= 1e-4f ||
	    std::abs(ray_z - source_z / source_length) >= 1e-4f) {
		std::cerr << "KB4 project/unproject round trip exceeded tolerance\n";
		return 1;
	}
	return 0;
}
