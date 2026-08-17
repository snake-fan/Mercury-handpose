// Copyright 2022, Collabora, Ltd.
// Copyright 2026, Beyley Cardellio
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  onnxruntime wrapper objects and functions.
 * @author Beyley Cardellio <ep1cm1n10n123@gmail.com>
 * @author Moshi Turner <moshiturner@protonmail.com>
 * @author Jakob Bornecrantz <jakob@collabora.com>
 * @ingroup aux_onnxruntime
 */

#include "onnx/onnx_wrapper.hpp"


namespace xrt::auxiliary::onnx {

OnnxWrapper::OnnxWrapper(enum u_logging_level log_level, std::string path, std::string env_name)
{
	this->log_level = log_level;

	this->api = OrtGetApiBase()->GetApi(ORT_API_VERSION);
	OrtSessionOptions *opts = nullptr;

	try {
		ORT_SAFE(*this, CreateSessionOptions(&opts));
		ORT_SAFE(*this, SetSessionGraphOptimizationLevel(opts, ORT_ENABLE_ALL));
		ORT_SAFE(*this, SetIntraOpNumThreads(opts, 1));
		ORT_SAFE(*this, CreateEnv(ORT_LOGGING_LEVEL_FATAL, env_name.c_str(), &this->env));
		ORT_SAFE(*this, CreateCpuMemoryInfo(OrtArenaAllocator, OrtMemTypeDefault, &this->meminfo));
		ORT_SAFE(*this, CreateSession(this->env, path.c_str(), opts, &this->session));
		if (this->session == nullptr) {
			throw std::runtime_error("Failed to create ONNX Runtime session: " + path);
		}
		this->api->ReleaseSessionOptions(opts);
	} catch (...) {
		if (opts != nullptr) this->api->ReleaseSessionOptions(opts);
		if (this->session != nullptr) this->api->ReleaseSession(this->session);
		if (this->meminfo != nullptr) this->api->ReleaseMemoryInfo(this->meminfo);
		if (this->env != nullptr) this->api->ReleaseEnv(this->env);
		this->session = nullptr;
		this->meminfo = nullptr;
		this->env = nullptr;
		throw;
	}
}

OnnxWrapper::~OnnxWrapper()
{
	if (this->api == nullptr) {
		return;
	}
	if (this->meminfo != nullptr) this->api->ReleaseMemoryInfo(this->meminfo);
	this->meminfo = nullptr;

	if (this->session != nullptr) this->api->ReleaseSession(this->session);
	this->session = nullptr;

	if (this->env != nullptr) this->api->ReleaseEnv(this->env);
	this->env = nullptr;

	this->api = nullptr;
}


}; // namespace xrt::auxiliary::onnx
