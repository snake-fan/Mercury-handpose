// Copyright 2026 Zhang
// SPDX-License-Identifier: BSL-1.0

#include "mercury/hand_tracker.hpp"

#include <chrono>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

struct TemporaryDirectory
{
	std::filesystem::path path;
	~TemporaryDirectory()
	{
		std::error_code error;
		std::filesystem::remove_all(path, error);
	}
};

} // namespace

int
main(int argc, char **argv)
{
	if (argc != 3) {
		std::cerr << "usage: initialization_failure_test CALIBRATION.json MODELS_DIR\n";
		return 2;
	}

	try {
		const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
		TemporaryDirectory temporary{
		    std::filesystem::temp_directory_path() /
		    ("mercury-handpose-init-failure-" + std::to_string(nonce))};
		std::filesystem::create_directory(temporary.path);
		std::filesystem::copy_file(
		    std::filesystem::path(argv[2]) / "grayscale_detection_160x160.onnx",
		    temporary.path / "grayscale_detection_160x160.onnx");
		std::ofstream(temporary.path / "grayscale_keypoint_jan18.onnx")
		    << "intentionally invalid ONNX model";

		const auto calibration = mercury::load_calibration_json(argv[1]);
		bool rejected = false;
		try {
			mercury::Tracker tracker(calibration, temporary.path.string());
		} catch (const std::exception &) {
			rejected = true;
		}
		if (!rejected) {
			std::cerr << "tracker unexpectedly accepted an invalid keypoint model\n";
			return 1;
		}
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
	return 0;
}
