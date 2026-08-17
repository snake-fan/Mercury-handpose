// Copyright 2026 Zhang
// SPDX-License-Identifier: BSL-1.0

#include "mercury/hand_tracker.hpp"

#include "hg_debug_instrumentation.hpp"
#include "hg_interface.h"
#include "tracking/t_hand_tracking.h"
#include "tracking/t_tracking.h"

#include <opencv2/core.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace mercury {
namespace {

constexpr const char *kDetectionModel = "grayscale_detection_160x160.onnx";
constexpr const char *kKeypointModel = "grayscale_keypoint_jan18.onnx";

constexpr std::array<std::size_t, kLandmarkCount> kLandmarkToJoint = {
	1, 2, 3, 4, 5, 7, 8, 9, 10, 12, 13, 14, 15, 17, 18, 19, 20, 22, 23, 24, 25,
};

template <std::size_t N>
std::array<double, N>
read_number_array(const cv::FileNode &node, const char *field)
{
	if (node.empty() || !node.isSeq() || node.size() != N) {
		throw std::runtime_error(std::string("Calibration field '") + field + "' must contain " +
		                         std::to_string(N) + " numbers");
	}
	std::array<double, N> output{};
	std::size_t index = 0;
	for (const auto &value : node) {
		output[index++] = (double)value;
	}
	return output;
}

double
required_number(const cv::FileNode &node, const char *field)
{
	const cv::FileNode value = node[field];
	if (value.empty() || (!value.isInt() && !value.isReal())) {
		throw std::runtime_error(std::string("Missing numeric calibration field '") + field + "'");
	}
	return (double)value;
}

CameraCalibration
read_camera(const cv::FileNode &node, std::size_t index)
{
	if (node.empty() || !node.isMap()) {
		throw std::runtime_error("Calibration camera " + std::to_string(index) + " is not an object");
	}

	CameraCalibration camera{};
	std::string model;
	node["model"] >> model;
	if (model == "fisheye_equidistant4") {
		camera.model = CameraModel::FisheyeEquidistant4;
		camera.distortion_count = 4;
	} else if (model == "pinhole_radtan5") {
		camera.model = CameraModel::PinholeRadTan5;
		camera.distortion_count = 5;
	} else {
		throw std::runtime_error("Unsupported camera model for camera " + std::to_string(index) + ": " + model);
	}

	const cv::FileNode intrinsics = node["intrinsics"];
	camera.fx = required_number(intrinsics, "fx");
	camera.fy = required_number(intrinsics, "fy");
	camera.cx = required_number(intrinsics, "cx");
	camera.cy = required_number(intrinsics, "cy");

	const cv::FileNode resolution = node["resolution"];
	camera.width = (std::uint32_t)required_number(resolution, "width");
	camera.height = (std::uint32_t)required_number(resolution, "height");

	const cv::FileNode distortion = node["distortion"];
	camera.distortion[0] = required_number(distortion, "k1");
	camera.distortion[1] = required_number(distortion, "k2");
	if (camera.model == CameraModel::FisheyeEquidistant4) {
		camera.distortion[2] = required_number(distortion, "k3");
		camera.distortion[3] = required_number(distortion, "k4");
	} else {
		camera.distortion[2] = required_number(distortion, "p1");
		camera.distortion[3] = required_number(distortion, "p2");
		camera.distortion[4] = required_number(distortion, "k3");
	}

	if (camera.width == 0 || camera.height == 0 || camera.fx <= 0.0 || camera.fy <= 0.0) {
		throw std::runtime_error("Camera " + std::to_string(index) + " has invalid resolution or focal length");
	}
	return camera;
}

enum t_camera_distortion_model
to_internal_model(CameraModel model)
{
	return model == CameraModel::FisheyeEquidistant4 ? T_DISTORTION_FISHEYE_KB4
	                                                   : T_DISTORTION_OPENCV_RADTAN_5;
}

enum t_camera_orientation
to_internal_orientation(CameraOrientation orientation)
{
	switch (orientation) {
	case CameraOrientation::Deg0: return CAMERA_ORIENTATION_0;
	case CameraOrientation::Deg90: return CAMERA_ORIENTATION_90;
	case CameraOrientation::Deg180: return CAMERA_ORIENTATION_180;
	case CameraOrientation::Deg270: return CAMERA_ORIENTATION_270;
	}
	return CAMERA_ORIENTATION_0;
}

t_stereo_camera_calibration *
to_internal_calibration(const StereoCalibration &calibration)
{
	if (calibration.cameras[0].model != calibration.cameras[1].model) {
		throw std::invalid_argument("Mercury requires both cameras to use the same distortion model");
	}

	t_stereo_camera_calibration *internal = nullptr;
	t_stereo_camera_calibration_alloc(&internal, to_internal_model(calibration.cameras[0].model));
	if (internal == nullptr) {
		throw std::bad_alloc();
	}

	for (std::size_t view = 0; view < 2; ++view) {
		const auto &source = calibration.cameras[view];
		auto &destination = internal->view[view];
		destination.image_size_pixels.w = (int)source.width;
		destination.image_size_pixels.h = (int)source.height;
		destination.intrinsics[0][0] = source.fx;
		destination.intrinsics[1][1] = source.fy;
		destination.intrinsics[0][2] = source.cx;
		destination.intrinsics[1][2] = source.cy;
		destination.intrinsics[2][2] = 1.0;
		std::copy(source.distortion.begin(), source.distortion.end(),
		          destination.distortion_parameters_as_array);
	}

	for (std::size_t row = 0; row < 3; ++row) {
		for (std::size_t column = 0; column < 3; ++column) {
			internal->camera_rotation[row][column] = calibration.rotation[row * 3 + column];
		}
		internal->camera_translation[row] = calibration.translation[row];
	}
	return internal;
}

xrt_frame
make_frame(const GrayImageView &image, std::int64_t timestamp_ns)
{
	xrt_frame frame{};
	frame.width = image.width;
	frame.height = image.height;
	frame.stride = image.stride_bytes;
	frame.size = image.stride_bytes * image.height;
	frame.data = const_cast<std::uint8_t *>(image.data);
	frame.format = XRT_FORMAT_L8;
	frame.stereo_format = XRT_STEREO_FORMAT_NONE;
	frame.timestamp = timestamp_ns;
	frame.source_timestamp = timestamp_ns;
	return frame;
}

HandPose
convert_hand(const xrt_hand_joint_set &source, bool is_right)
{
	HandPose result{};
	result.active = source.is_active;
	result.is_right = is_right;
	for (std::size_t index = 0; index < kJointCount; ++index) {
		const auto &input = source.values.hand_joint_set_default[index];
		auto &output = result.joints[index];
		output.position = {input.relation.pose.position.x, input.relation.pose.position.y,
		                   input.relation.pose.position.z};
		output.orientation = {input.relation.pose.orientation.x, input.relation.pose.orientation.y,
		                      input.relation.pose.orientation.z, input.relation.pose.orientation.w};
		output.radius_m = input.radius;
		const auto flags = input.relation.relation_flags;
		output.position_valid = (flags & XRT_SPACE_RELATION_POSITION_VALID_BIT) != 0;
		output.orientation_valid = (flags & XRT_SPACE_RELATION_ORIENTATION_VALID_BIT) != 0;
		output.tracked = (flags & (XRT_SPACE_RELATION_POSITION_TRACKED_BIT |
		                           XRT_SPACE_RELATION_ORIENTATION_TRACKED_BIT)) != 0;
	}
	for (std::size_t index = 0; index < kLandmarkCount; ++index) {
		result.landmarks_21[index] = result.joints[kLandmarkToJoint[index]].position;
	}
	return result;
}

void
validate_image(const GrayImageView &image, const char *name)
{
	if (image.data == nullptr) {
		throw std::invalid_argument(std::string(name) + " image data is null");
	}
	if (image.width == 0 || image.height == 0) {
		throw std::invalid_argument(std::string(name) + " image dimensions are zero");
	}
	if (image.stride_bytes < image.width) {
		throw std::invalid_argument(std::string(name) + " image stride is smaller than its width");
	}
}

} // namespace

StereoCalibration
load_calibration_json(const std::string &path)
{
	cv::FileStorage storage(path, cv::FileStorage::READ | cv::FileStorage::FORMAT_JSON);
	if (!storage.isOpened()) {
		throw std::runtime_error("Unable to open calibration JSON: " + path);
	}
	const cv::FileNode metadata = storage["metadata"];
	if (!metadata.empty() && (int)metadata["version"] != 2) {
		throw std::runtime_error("Only Monado calibration JSON version 2 is supported");
	}
	const cv::FileNode cameras = storage["cameras"];
	if (cameras.empty() || !cameras.isSeq() || cameras.size() != 2) {
		throw std::runtime_error("Calibration JSON must contain exactly two cameras");
	}

	StereoCalibration calibration{};
	calibration.cameras[0] = read_camera(cameras[0], 0);
	calibration.cameras[1] = read_camera(cameras[1], 1);
	if (calibration.cameras[0].model != calibration.cameras[1].model) {
		throw std::runtime_error("Both cameras must use the same distortion model");
	}

	const cv::FileNode stereo = storage["opencv_stereo_calibrate"];
	calibration.rotation = read_number_array<9>(stereo["rotation"], "rotation");
	calibration.translation = read_number_array<3>(stereo["translation"], "translation");
	return calibration;
}

std::string
validate_model_directory(const std::string &model_directory)
{
	const std::filesystem::path directory(model_directory);
	if (!std::filesystem::is_directory(directory)) {
		return "Model directory does not exist: " + model_directory;
	}
	for (const char *name : {kDetectionModel, kKeypointModel}) {
		const auto path = directory / name;
		if (!std::filesystem::is_regular_file(path)) {
			return "Required model is missing: " + path.string();
		}
	}
	return {};
}

class Tracker::Impl
{
public:
	Impl(const StereoCalibration &calibration, const std::string &models, const TrackerOptions &options)
	{
		const std::string model_error = validate_model_directory(models);
		if (!model_error.empty()) {
			throw std::invalid_argument(model_error);
		}
		for (std::size_t view = 0; view < 2; ++view) {
			const auto &camera = calibration.cameras[view];
			if (camera.width == 0 || camera.height == 0 || camera.fx <= 0.0 || camera.fy <= 0.0) {
				throw std::invalid_argument("Calibration contains an invalid camera");
			}
		}
		if (calibration.cameras[0].width != calibration.cameras[1].width ||
		    calibration.cameras[0].height != calibration.cameras[1].height) {
			throw std::invalid_argument("Mercury requires equal calibration resolutions for both cameras");
		}
		calibration_width_ = calibration.cameras[0].width;
		calibration_height_ = calibration.cameras[0].height;

		t_stereo_camera_calibration *internal = to_internal_calibration(calibration);
		t_hand_tracking_create_info create_info{};
		for (std::size_t view = 0; view < 2; ++view) {
			create_info.cams_info.views[view].camera_orientation =
			    to_internal_orientation(options.camera_orientations[view]);
			const auto &boundary = options.image_boundaries[view];
			if (boundary.enabled) {
				create_info.cams_info.views[view].boundary_type = HT_IMAGE_BOUNDARY_CIRCLE;
				create_info.cams_info.views[view].boundary.circle.normalized_center =
				    {boundary.center_x, boundary.center_y};
				create_info.cams_info.views[view].boundary.circle.normalized_radius =
				    boundary.radius_over_image_width;
			}
		}

		tracker_ = t_hand_tracking_sync_mercury_create(internal, create_info, models.c_str());
		t_stereo_camera_calibration_reference(&internal, nullptr);
		if (tracker_ == nullptr) {
			throw std::runtime_error("Mercury failed to initialize the ONNX models");
		}

		auto *tuning = xrt::tracking::hand::mercury::
		    t_hand_tracking_sync_mercury_get_tuneable_values_pointer(tracker_);
		if (tuning != nullptr) {
			tuning->num_frames_before_display = options.warmup_frames;
			tuning->min_detection_confidence.val = options.minimum_detection_confidence;
			tuning->max_hand_dist.val = options.maximum_hand_distance_m;
			tuning->optimize_hand_size = options.optimize_hand_size;
		}
	}

	~Impl() { t_ht_sync_destroy(&tracker_); }

	FrameResult
	process(const GrayImageView &left, const GrayImageView &right, std::int64_t timestamp_ns)
	{
		validate_image(left, "left");
		validate_image(right, "right");
		if (left.width != right.width || left.height != right.height) {
			throw std::invalid_argument("Left and right images must have identical dimensions");
		}
		if (static_cast<std::uint64_t>(left.width) * calibration_height_ !=
		    static_cast<std::uint64_t>(left.height) * calibration_width_) {
			throw std::invalid_argument("Input images and calibration must have the same aspect ratio");
		}
		if (timestamp_ns < 0) {
			throw std::invalid_argument("Timestamp must be non-negative");
		}

		std::lock_guard<std::mutex> lock(mutex_);
		if (last_timestamp_ns_ >= 0 && timestamp_ns <= last_timestamp_ns_) {
			throw std::invalid_argument("Timestamps must be strictly increasing for each tracker");
		}
		last_timestamp_ns_ = timestamp_ns;
		xrt_frame left_frame = make_frame(left, timestamp_ns);
		xrt_frame right_frame = make_frame(right, timestamp_ns);
		xrt_hand_joint_set left_hand{};
		xrt_hand_joint_set right_hand{};
		std::int64_t output_timestamp = 0;
		t_ht_sync_process(tracker_, &left_frame, &right_frame, &left_hand, &right_hand, &output_timestamp);

		FrameResult result{};
		result.timestamp_ns = output_timestamp;
		result.left = convert_hand(left_hand, false);
		result.right = convert_hand(right_hand, true);
		return result;
	}

private:
	t_hand_tracking_sync *tracker_ = nullptr;
	std::mutex mutex_;
	std::uint32_t calibration_width_ = 0;
	std::uint32_t calibration_height_ = 0;
	std::int64_t last_timestamp_ns_ = -1;
};

Tracker::Tracker(const StereoCalibration &calibration,
                 const std::string &model_directory,
                 const TrackerOptions &options)
    : impl_(std::make_unique<Impl>(calibration, model_directory, options))
{}

Tracker::~Tracker() = default;
Tracker::Tracker(Tracker &&) noexcept = default;
Tracker &Tracker::operator=(Tracker &&) noexcept = default;

FrameResult
Tracker::process(const GrayImageView &left, const GrayImageView &right, std::int64_t timestamp_ns)
{
	if (!impl_) {
		throw std::logic_error("Cannot use a moved-from Mercury tracker");
	}
	return impl_->process(left, right, timestamp_ns);
}

const char *
joint_name(Joint joint) noexcept
{
	static constexpr std::array<const char *, kJointCount> names = {
	    "palm",          "wrist",        "thumb_metacarpal", "thumb_proximal", "thumb_distal",
	    "thumb_tip",     "index_metacarpal", "index_proximal", "index_intermediate", "index_distal",
	    "index_tip",     "middle_metacarpal", "middle_proximal", "middle_intermediate", "middle_distal",
	    "middle_tip",    "ring_metacarpal", "ring_proximal", "ring_intermediate", "ring_distal",
	    "ring_tip",      "little_metacarpal", "little_proximal", "little_intermediate", "little_distal",
	    "little_tip",
	};
	const auto index = static_cast<std::size_t>(joint);
	return index < names.size() ? names[index] : "unknown";
}

} // namespace mercury
