// Copyright 2026 Zhang
// SPDX-License-Identifier: BSL-1.0

#include "mercury/hand_tracker.hpp"

#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <vector>

int
main(int argc, char **argv)
{
	if (argc != 3) {
		std::cerr << "usage: pipeline_smoke_test CALIBRATION.json MODELS_DIR\n";
		return 2;
	}
	try {
		const auto calibration = mercury::load_calibration_json(argv[1]);
		mercury::TrackerOptions options{};
		options.warmup_frames = 0;
		mercury::Tracker tracker(calibration, argv[2], options);

		constexpr std::uint32_t width = 640;
		constexpr std::uint32_t height = 480;
		std::vector<std::uint8_t> left(width * height, 127);
		std::vector<std::uint8_t> right(width * height, 127);
		const mercury::GrayImageView left_view{left.data(), width, height, width};
		const mercury::GrayImageView right_view{right.data(), width, height, width};
		for (std::int64_t frame = 0; frame < 3; ++frame) {
			const auto timestamp = 1'000'000'000LL + frame * 16'666'667LL;
			const auto result = tracker.process(left_view, right_view, timestamp);
			if (result.timestamp_ns != timestamp) {
				std::cerr << "unexpected output timestamp\n";
				return 1;
			}
		}

		bool rejected_duplicate_timestamp = false;
		try {
			(void)tracker.process(left_view, right_view, 1'000'000'000LL + 2 * 16'666'667LL);
		} catch (const std::invalid_argument &) {
			rejected_duplicate_timestamp = true;
		}
		if (!rejected_duplicate_timestamp) {
			std::cerr << "tracker accepted a duplicate timestamp\n";
			return 1;
		}
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
	return 0;
}
