// Copyright 2019-2025, Collabora, Ltd.
// Copyright 2026 Zhang
// SPDX-License-Identifier: BSL-1.0

#include "os/os_time.h"
#include "tracking/t_tracking.h"
#include "util/u_debug.h"
#include "util/u_frame.h"
#include "util/u_hand_tracking.h"
#include "util/u_logging.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>

namespace {

std::mutex g_log_mutex;

const char *
level_name(enum u_logging_level level)
{
	switch (level) {
	case U_LOGGING_TRACE: return "TRACE";
	case U_LOGGING_DEBUG: return "DEBUG";
	case U_LOGGING_INFO: return "INFO";
	case U_LOGGING_WARN: return "WARN";
	case U_LOGGING_ERROR: return "ERROR";
	case U_LOGGING_RAW: return "";
	}
	return "LOG";
}

struct owned_frame
{
	struct xrt_frame frame;
};

void
destroy_owned_frame(struct xrt_frame *frame)
{
	std::free(frame->data);
	delete reinterpret_cast<owned_frame *>(frame);
}

} // namespace

extern "C" void
u_log(const char *file, int line, const char *func, enum u_logging_level level, const char *format, ...)
{
	std::lock_guard<std::mutex> lock(g_log_mutex);
	std::fprintf(stderr, "%s %s:%d %s: ", level_name(level), file, line, func);
	va_list args;
	va_start(args, format);
	std::vfprintf(stderr, format, args);
	va_end(args);
	std::fputc('\n', stderr);
}

extern "C" bool
debug_string_to_bool(const char *value)
{
	if (value == nullptr) {
		return false;
	}
	return std::strcmp(value, "1") == 0 || std::strcmp(value, "true") == 0 || std::strcmp(value, "on") == 0 ||
	       std::strcmp(value, "yes") == 0;
}

extern "C" enum debug_tristate_option
debug_string_to_tristate(const char *value)
{
	if (value == nullptr || std::strcmp(value, "auto") == 0) {
		return DEBUG_TRISTATE_AUTO;
	}
	return debug_string_to_bool(value) ? DEBUG_TRISTATE_ON : DEBUG_TRISTATE_OFF;
}

extern "C" long
debug_string_to_num(const char *value, long fallback)
{
	if (value == nullptr) {
		return fallback;
	}
	char *end = nullptr;
	const long parsed = std::strtol(value, &end, 0);
	return end != value ? parsed : fallback;
}

extern "C" float
debug_string_to_float(const char *value, float fallback)
{
	if (value == nullptr) {
		return fallback;
	}
	char *end = nullptr;
	const float parsed = std::strtof(value, &end);
	return end != value ? parsed : fallback;
}

extern "C" enum u_logging_level
debug_string_to_log_level(const char *value, enum u_logging_level fallback)
{
	if (value == nullptr) {
		return fallback;
	}
	char lowered[16] = {};
	const size_t count = std::min(std::strlen(value), sizeof(lowered) - 1);
	for (size_t i = 0; i < count; ++i) {
		lowered[i] = (char)std::tolower((unsigned char)value[i]);
	}
	if (std::strcmp(lowered, "trace") == 0) return U_LOGGING_TRACE;
	if (std::strcmp(lowered, "debug") == 0) return U_LOGGING_DEBUG;
	if (std::strcmp(lowered, "info") == 0) return U_LOGGING_INFO;
	if (std::strcmp(lowered, "warn") == 0 || std::strcmp(lowered, "warning") == 0) return U_LOGGING_WARN;
	if (std::strcmp(lowered, "error") == 0) return U_LOGGING_ERROR;
	return fallback;
}

extern "C" const char *
debug_get_option(char *storage, size_t storage_size, const char *name, const char *fallback)
{
	const char *value = std::getenv(name);
	if (value == nullptr) {
		value = fallback;
	}
	if (value == nullptr || storage_size == 0) {
		return value;
	}
	std::strncpy(storage, value, storage_size - 1);
	storage[storage_size - 1] = '\0';
	return storage;
}

extern "C" bool debug_get_bool_option(const char *name, bool fallback)
{
	const char *value = std::getenv(name);
	return value == nullptr ? fallback : debug_string_to_bool(value);
}

extern "C" enum debug_tristate_option debug_get_tristate_option(const char *name)
{
	return debug_string_to_tristate(std::getenv(name));
}

extern "C" long debug_get_num_option(const char *name, long fallback)
{
	return debug_string_to_num(std::getenv(name), fallback);
}

extern "C" float debug_get_float_option(const char *name, float fallback)
{
	return debug_string_to_float(std::getenv(name), fallback);
}

extern "C" enum u_logging_level debug_get_log_option(const char *name, enum u_logging_level fallback)
{
	return debug_string_to_log_level(std::getenv(name), fallback);
}

extern "C" int64_t
os_monotonic_get_ns(void)
{
	return (int64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(
	           std::chrono::steady_clock::now().time_since_epoch())
	    .count();
}

extern "C" void
t_stereo_camera_calibration_alloc(struct t_stereo_camera_calibration **out,
                                  enum t_camera_distortion_model distortion_model)
{
	auto *calibration = (t_stereo_camera_calibration *)std::calloc(1, sizeof(t_stereo_camera_calibration));
	calibration->reference.count = 1;
	calibration->view[0].distortion_model = distortion_model;
	calibration->view[1].distortion_model = distortion_model;
	calibration->camera_rotation[0][0] = 1.0;
	calibration->camera_rotation[1][1] = 1.0;
	calibration->camera_rotation[2][2] = 1.0;
	*out = calibration;
}

extern "C" void t_stereo_camera_calibration_destroy(struct t_stereo_camera_calibration *calibration)
{
	std::free(calibration);
}

extern "C" void t_stereo_camera_calibration_dump(struct t_stereo_camera_calibration *calibration)
{
	U_LOG_D("Stereo calibration: %dx%d, baseline=(%.6f, %.6f, %.6f) m",
	        calibration->view[0].image_size_pixels.w,
	        calibration->view[0].image_size_pixels.h,
	        calibration->camera_translation[0],
	        calibration->camera_translation[1],
	        calibration->camera_translation[2]);
}

extern "C" void
u_frame_create_one_off(enum xrt_format format, uint32_t width, uint32_t height, struct xrt_frame **out_frame)
{
	auto *owned = new owned_frame{};
	auto &frame = owned->frame;
	frame.reference.count = 1;
	frame.destroy = destroy_owned_frame;
	frame.width = width;
	frame.height = height;
	frame.format = format;
	frame.stereo_format = XRT_STEREO_FORMAT_NONE;
	const size_t pixel_size = format == XRT_FORMAT_R8G8B8 ? 3 : 1;
	frame.stride = (size_t)width * pixel_size;
	frame.size = frame.stride * height;
	frame.data = (uint8_t *)std::calloc(1, frame.size);
	*out_frame = &frame;
}

extern "C" void
u_hand_joints_apply_joint_width(struct xrt_hand_joint_set *set)
{
	auto *joints = set->values.hand_joint_set_default;
	static const float finger_joint_size[5] = {0.022f, 0.021f, 0.022f, 0.021f, 0.020f};
	static const float finger_scale[4] = {1.0f, 1.0f, 0.83f, 0.75f};
	static const float thumb_size[4] = {0.016f, 0.014f, 0.012f, 0.012f};
	for (int i = XRT_HAND_JOINT_THUMB_METACARPAL; i <= XRT_HAND_JOINT_THUMB_TIP; ++i) {
		joints[i].radius = thumb_size[i - XRT_HAND_JOINT_THUMB_METACARPAL];
	}
	for (int finger = 0; finger < 4; ++finger) {
		for (int joint = 0; joint < 5; ++joint) {
			const int index = finger * 5 + joint + XRT_HAND_JOINT_INDEX_METACARPAL;
			joints[index].radius = finger_joint_size[joint] * finger_scale[finger] * 0.5f;
		}
	}
	joints[XRT_HAND_JOINT_PALM].radius = 0.016f;
	joints[XRT_HAND_JOINT_WRIST].radius = 0.020f;
}
