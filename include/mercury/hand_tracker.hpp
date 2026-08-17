// Copyright 2026 Zhang
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace mercury {

struct Vec3
{
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
};

struct Quaternion
{
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
	float w = 1.0f;
};

enum class Joint : std::uint8_t
{
	Palm = 0,
	Wrist,
	ThumbMetacarpal,
	ThumbProximal,
	ThumbDistal,
	ThumbTip,
	IndexMetacarpal,
	IndexProximal,
	IndexIntermediate,
	IndexDistal,
	IndexTip,
	MiddleMetacarpal,
	MiddleProximal,
	MiddleIntermediate,
	MiddleDistal,
	MiddleTip,
	RingMetacarpal,
	RingProximal,
	RingIntermediate,
	RingDistal,
	RingTip,
	LittleMetacarpal,
	LittleProximal,
	LittleIntermediate,
	LittleDistal,
	LittleTip,
};

inline constexpr std::size_t kJointCount = 26;
inline constexpr std::size_t kLandmarkCount = 21;

struct JointPose
{
	Vec3 position;
	Quaternion orientation;
	float radius_m = 0.0f;
	bool position_valid = false;
	bool orientation_valid = false;
	bool tracked = false;
};

struct HandPose
{
	bool active = false;
	bool is_right = false;
	std::array<JointPose, kJointCount> joints{};

	// Mercury neural-network order: wrist, 4 thumb points, then 4 points per finger.
	std::array<Vec3, kLandmarkCount> landmarks_21{};
};

struct FrameResult
{
	std::int64_t timestamp_ns = 0;
	HandPose left;
	HandPose right;
};

struct GrayImageView
{
	const std::uint8_t *data = nullptr;
	std::uint32_t width = 0;
	std::uint32_t height = 0;
	std::size_t stride_bytes = 0;
};

enum class CameraModel
{
	FisheyeEquidistant4,
	PinholeRadTan5,
};

struct CameraCalibration
{
	CameraModel model = CameraModel::FisheyeEquidistant4;
	std::uint32_t width = 0;
	std::uint32_t height = 0;
	double fx = 0.0;
	double fy = 0.0;
	double cx = 0.0;
	double cy = 0.0;
	std::array<double, 14> distortion{};
	std::size_t distortion_count = 4;
};

struct StereoCalibration
{
	std::array<CameraCalibration, 2> cameras{};

	// OpenCV convention: point_right = rotation * point_left + translation.
	// Translation is measured in metres.
	std::array<double, 9> rotation{1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0};
	std::array<double, 3> translation{};
};

enum class CameraOrientation : int
{
	Deg0 = 0,
	Deg90 = 90,
	Deg180 = 180,
	Deg270 = 270,
};

struct CircularBoundary
{
	bool enabled = false;
	float center_x = 0.5f;
	float center_y = 0.5f;
	float radius_over_image_width = 0.5f;
};

struct TrackerOptions
{
	std::array<CameraOrientation, 2> camera_orientations{CameraOrientation::Deg0, CameraOrientation::Deg0};
	std::array<CircularBoundary, 2> image_boundaries{};
	std::size_t warmup_frames = 10;
	float minimum_detection_confidence = 0.3f;
	float maximum_hand_distance_m = 1.7f;
	bool optimize_hand_size = true;
};

// Loads Monado calibration JSON v2. Both cameras must use the same model.
StereoCalibration
load_calibration_json(const std::string &path);

// Returns an empty string when both required model files are present.
std::string
validate_model_directory(const std::string &model_directory);

class Tracker
{
public:
	Tracker(const StereoCalibration &calibration,
	        const std::string &model_directory,
	        const TrackerOptions &options = {});
	~Tracker();

	Tracker(const Tracker &) = delete;
	Tracker &operator=(const Tracker &) = delete;
	Tracker(Tracker &&) noexcept;
	Tracker &operator=(Tracker &&) noexcept;

	// The tracker is stateful. Calls are serialized internally.
	// Input must be synchronized 8-bit grayscale images of identical dimensions
	// and the same aspect ratio as the calibration images.
	FrameResult
	process(const GrayImageView &left, const GrayImageView &right, std::int64_t timestamp_ns);

private:
	class Impl;
	std::unique_ptr<Impl> impl_;
};

const char *
joint_name(Joint joint) noexcept;

} // namespace mercury
