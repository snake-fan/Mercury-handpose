// Copyright 2026 Zhang
// SPDX-License-Identifier: BSL-1.0

#include "mercury/hand_tracker.hpp"

#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {

struct Arguments
{
	std::string calibration;
	std::string models;
	std::string left;
	std::string right;
	std::string side_by_side;
	std::string output;
	bool show = false;
	std::int64_t max_frames = -1;
};

void
print_usage(const char *program)
{
	std::cerr << "Usage:\n  " << program
	          << " --calibration FILE --models DIR (--left SOURCE --right SOURCE | --sbs SOURCE)"
	             " [--output poses.ndjson] [--show] [--max-frames N]\n\n"
	             "SOURCE may be a video path or a numeric OpenCV camera index.\n";
}

Arguments
parse_arguments(int argc, char **argv)
{
	Arguments arguments{};
	for (int index = 1; index < argc; ++index) {
		const std::string name = argv[index];
		auto value = [&]() -> std::string {
			if (++index >= argc) throw std::invalid_argument("Missing value for " + name);
			return argv[index];
		};
		if (name == "--calibration") arguments.calibration = value();
		else if (name == "--models") arguments.models = value();
		else if (name == "--left") arguments.left = value();
		else if (name == "--right") arguments.right = value();
		else if (name == "--sbs") arguments.side_by_side = value();
		else if (name == "--output") arguments.output = value();
		else if (name == "--max-frames") arguments.max_frames = std::stoll(value());
		else if (name == "--show") arguments.show = true;
		else if (name == "--help" || name == "-h") {
			print_usage(argv[0]);
			std::exit(0);
		} else {
			throw std::invalid_argument("Unknown argument: " + name);
		}
	}
	const bool separate = !arguments.left.empty() && !arguments.right.empty();
	const bool combined = !arguments.side_by_side.empty();
	if (arguments.calibration.empty() || arguments.models.empty() || separate == combined) {
		throw std::invalid_argument("Calibration, models and exactly one stereo source mode are required");
	}
	return arguments;
}

bool
is_integer(const std::string &value)
{
	if (value.empty()) return false;
	std::size_t start = value.front() == '-' ? 1 : 0;
	if (start == value.size()) return false;
	for (std::size_t index = start; index < value.size(); ++index) {
		if (value[index] < '0' || value[index] > '9') return false;
	}
	return true;
}

cv::VideoCapture
open_capture(const std::string &source)
{
	cv::VideoCapture capture;
	if (is_integer(source)) capture.open(std::stoi(source));
	else capture.open(source);
	if (!capture.isOpened()) throw std::runtime_error("Unable to open source: " + source);
	return capture;
}

cv::Mat
to_gray(const cv::Mat &input)
{
	if (input.empty()) return {};
	if (input.channels() == 1) return input;
	cv::Mat gray;
	if (input.channels() == 4) cv::cvtColor(input, gray, cv::COLOR_BGRA2GRAY);
	else cv::cvtColor(input, gray, cv::COLOR_BGR2GRAY);
	return gray;
}

cv::Point2f
project_fisheye(const mercury::Vec3 &point,
	             const mercury::StereoCalibration &stereo,
	             std::size_t view)
{
	// Mercury output is +X right, +Y up, -Z forward. Convert to OpenCV camera coordinates.
	double x = point.x;
	double y = -point.y;
	double z = -point.z;
	if (view == 1) {
		const auto &r = stereo.rotation;
		const auto &t = stereo.translation;
		const double transformed_x = r[0] * x + r[1] * y + r[2] * z + t[0];
		const double transformed_y = r[3] * x + r[4] * y + r[5] * z + t[1];
		const double transformed_z = r[6] * x + r[7] * y + r[8] * z + t[2];
		x = transformed_x;
		y = transformed_y;
		z = transformed_z;
	}
	if (z <= 1e-6) return {-1.0f, -1.0f};

	const auto &camera = stereo.cameras[view];
	const double radial = std::hypot(x, y);
	double normalized_x = 0.0;
	double normalized_y = 0.0;
	if (camera.model == mercury::CameraModel::FisheyeEquidistant4) {
		if (radial > 1e-12) {
			const double theta = std::atan2(radial, z);
			const double theta2 = theta * theta;
			const auto &k = camera.distortion;
			const double distorted_theta =
			    theta * (1.0 + k[0] * theta2 + k[1] * theta2 * theta2 +
			             k[2] * theta2 * theta2 * theta2 + k[3] * theta2 * theta2 * theta2 * theta2);
			normalized_x = x * distorted_theta / radial;
			normalized_y = y * distorted_theta / radial;
		}
	} else {
		const double nx = x / z;
		const double ny = y / z;
		const double r2 = nx * nx + ny * ny;
		const auto &d = camera.distortion;
		const double radial_scale = 1.0 + d[0] * r2 + d[1] * r2 * r2 + d[4] * r2 * r2 * r2;
		normalized_x = nx * radial_scale + 2.0 * d[2] * nx * ny + d[3] * (r2 + 2.0 * nx * nx);
		normalized_y = ny * radial_scale + d[2] * (r2 + 2.0 * ny * ny) + 2.0 * d[3] * nx * ny;
	}
	return {(float)(camera.fx * normalized_x + camera.cx), (float)(camera.fy * normalized_y + camera.cy)};
}

void
draw_hand(cv::Mat &image,
	      const mercury::HandPose &hand,
	      const mercury::StereoCalibration &calibration,
	      std::size_t view,
	      const cv::Scalar &color)
{
	if (!hand.active) return;
	std::array<cv::Point2f, mercury::kLandmarkCount> points{};
	for (std::size_t index = 0; index < points.size(); ++index) {
		points[index] = project_fisheye(hand.landmarks_21[index], calibration, view);
		if (points[index].x >= 0.0f && points[index].y >= 0.0f) {
			cv::circle(image, points[index], 3, color, cv::FILLED, cv::LINE_AA);
		}
	}
	static constexpr std::array<std::array<int, 2>, 20> bones = {{
	    {0, 1}, {1, 2}, {2, 3}, {3, 4}, {0, 5}, {5, 6}, {6, 7}, {7, 8}, {0, 9}, {9, 10},
	    {10, 11}, {11, 12}, {0, 13}, {13, 14}, {14, 15}, {15, 16}, {0, 17}, {17, 18}, {18, 19}, {19, 20},
	}};
	for (const auto &bone : bones) {
		const auto &a = points[bone[0]];
		const auto &b = points[bone[1]];
		if (a.x >= 0.0f && a.y >= 0.0f && b.x >= 0.0f && b.y >= 0.0f) {
			cv::line(image, a, b, color, 2, cv::LINE_AA);
		}
	}
}

void
write_hand(std::ostream &stream, const mercury::HandPose &hand)
{
	stream << "{\"active\":" << (hand.active ? "true" : "false") << ",\"landmarks21\":[";
	for (std::size_t index = 0; index < hand.landmarks_21.size(); ++index) {
		if (index != 0) stream << ',';
		const auto &point = hand.landmarks_21[index];
		stream << '[' << point.x << ',' << point.y << ',' << point.z << ']';
	}
	stream << "]}";
}

void
write_result(std::ostream &stream, std::int64_t frame_index, const mercury::FrameResult &result)
{
	stream << std::fixed << std::setprecision(6) << "{\"frame\":" << frame_index << ",\"timestamp_ns\":"
	       << result.timestamp_ns << ",\"left\":";
	write_hand(stream, result.left);
	stream << ",\"right\":";
	write_hand(stream, result.right);
	stream << "}\n";
}

} // namespace

int
main(int argc, char **argv)
{
	try {
		const Arguments arguments = parse_arguments(argc, argv);
		const auto calibration = mercury::load_calibration_json(arguments.calibration);
		mercury::Tracker tracker(calibration, arguments.models);

		cv::VideoCapture combined;
		cv::VideoCapture left_capture;
		cv::VideoCapture right_capture;
		if (!arguments.side_by_side.empty()) combined = open_capture(arguments.side_by_side);
		else {
			left_capture = open_capture(arguments.left);
			right_capture = open_capture(arguments.right);
		}

		std::ofstream output_file;
		std::ostream *output = &std::cout;
		if (!arguments.output.empty()) {
			output_file.open(arguments.output);
			if (!output_file) throw std::runtime_error("Unable to open output: " + arguments.output);
			output = &output_file;
		}

		for (std::int64_t frame_index = 0;
		     arguments.max_frames < 0 || frame_index < arguments.max_frames;
		     ++frame_index) {
			cv::Mat left_color;
			cv::Mat right_color;
			if (combined.isOpened()) {
				cv::Mat frame;
				if (!combined.read(frame)) break;
				if (frame.cols % 2 != 0) throw std::runtime_error("Side-by-side frame width must be even");
				const int half_width = frame.cols / 2;
				left_color = frame(cv::Rect(0, 0, half_width, frame.rows)).clone();
				right_color = frame(cv::Rect(half_width, 0, half_width, frame.rows)).clone();
			} else {
				if (!left_capture.read(left_color) || !right_capture.read(right_color)) break;
			}

			cv::Mat left_gray = to_gray(left_color);
			cv::Mat right_gray = to_gray(right_color);
			const auto now = std::chrono::steady_clock::now().time_since_epoch();
			const auto timestamp_ns =
			    std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
			const mercury::GrayImageView left_view{left_gray.data, (std::uint32_t)left_gray.cols,
			                                                (std::uint32_t)left_gray.rows, left_gray.step};
			const mercury::GrayImageView right_view{right_gray.data, (std::uint32_t)right_gray.cols,
			                                                 (std::uint32_t)right_gray.rows, right_gray.step};
			const auto result = tracker.process(left_view, right_view, timestamp_ns);
			write_result(*output, frame_index, result);

			if (arguments.show) {
				if (left_color.channels() == 1) cv::cvtColor(left_color, left_color, cv::COLOR_GRAY2BGR);
				if (right_color.channels() == 1) cv::cvtColor(right_color, right_color, cv::COLOR_GRAY2BGR);
				draw_hand(left_color, result.left, calibration, 0, {0, 255, 0});
				draw_hand(left_color, result.right, calibration, 0, {0, 128, 255});
				draw_hand(right_color, result.left, calibration, 1, {0, 255, 0});
				draw_hand(right_color, result.right, calibration, 1, {0, 128, 255});
				cv::Mat display;
				cv::hconcat(left_color, right_color, display);
				cv::imshow("Mercury stereo hand pose", display);
				const int key = cv::waitKey(1);
				if (key == 27 || key == 'q' || key == 'Q') break;
			}
		}
	} catch (const std::exception &error) {
		std::cerr << "error: " << error.what() << '\n';
		print_usage(argv[0]);
		return 1;
	}
	return 0;
}
