// Copyright 2026 Zhang
// SPDX-License-Identifier: BSL-1.0

#include "mercury/hand_tracker.hpp"

#include <cmath>
#include <exception>
#include <iostream>

int
main(int argc, char **argv)
{
	if (argc != 2) {
		std::cerr << "usage: calibration_test CALIBRATION.json\n";
		return 2;
	}
	try {
		const auto calibration = mercury::load_calibration_json(argv[1]);
		if (calibration.cameras[0].model != mercury::CameraModel::FisheyeEquidistant4 ||
		    calibration.cameras[0].width != 640 || calibration.cameras[1].height != 480 ||
		    std::abs(calibration.translation[0] + 0.064) >= 1e-9 ||
		    std::string(mercury::joint_name(mercury::Joint::IndexTip)) != "index_tip") {
			std::cerr << "calibration values did not round-trip as expected\n";
			return 1;
		}
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
	return 0;
}
