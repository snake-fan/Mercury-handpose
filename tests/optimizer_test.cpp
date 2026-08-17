// Copyright 2022, Collabora, Inc.
// Copyright 2026 Zhang
// SPDX-License-Identifier: BSL-1.0

#include "kine_common.hpp"
#include "lm_interface.hpp"
#include "math/m_vec2.h"
#include "util/u_logging.h"

#include <cmath>
#include <iostream>

using namespace xrt::tracking::hand::mercury;

int
main()
{
	one_frame_input input{};
	for (int view = 0; view < 2; ++view) {
		input.views[view].active = true;
		input.views[view].stereographic_radius = 0.5f;
		input.views[view].look_dir = XRT_QUAT_IDENTITY;
		for (int finger = 0; finger < 5; ++finger) {
			input.views[view].curls[finger].value = -0.5f;
			input.views[view].curls[finger].variance = 1.0f;
		}
		for (int joint = 0; joint < 21; ++joint) {
			xrt_vec2 direction = {std::sin((float)joint), std::cos((float)joint)};
			m_vec2_normalize(&direction);
			auto &point = input.views[view].keypoints_in_scaled_stereographic[joint];
			point.pos_2d = direction;
			point.depth_relative_to_midpxm = (joint / 21.0f) - 0.5f;
			point.confidence_depth = 1.0f;
			point.confidence_xy = 1.0f;
		}
	}

	lm::KinematicHandLM *hand = nullptr;
	xrt_pose left_in_right = XRT_POSE_IDENTITY;
	left_in_right.position.x = -0.064f;
	lm::optimizer_create(left_in_right, false, U_LOGGING_ERROR, &hand);

	xrt_hand_joint_set output{};
	float hand_size = 0.0f;
	float reprojection_error = 0.0f;
	lm::optimizer_run(hand, input, true, 2.0f, true, 0.09f, 0.5f, 0.5f, output, hand_size,
	                  reprojection_error);
	if (!std::isfinite(hand_size) || !std::isfinite(reprojection_error)) {
		std::cerr << "optimizer returned a non-finite value\n";
		return 1;
	}
	lm::optimizer_destroy(&hand);
	if (hand != nullptr) {
		std::cerr << "optimizer_destroy did not clear the handle\n";
		return 1;
	}
	return 0;
}
