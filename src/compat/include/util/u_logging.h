// Copyright 2020-2025, Collabora, Ltd.
// Copyright 2026 Zhang
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "xrt/xrt_compiler.h"

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

enum u_logging_level
{
	U_LOGGING_TRACE,
	U_LOGGING_DEBUG,
	U_LOGGING_INFO,
	U_LOGGING_WARN,
	U_LOGGING_ERROR,
	U_LOGGING_RAW,
};

void
u_log(const char *file, int line, const char *func, enum u_logging_level level, const char *format, ...)
    XRT_PRINTF_FORMAT(5, 6);

#define U_LOG(level, ...) u_log(__FILE__, __LINE__, __func__, level, __VA_ARGS__)
#define U_LOG_IFL(level, cond_level, ...)                                                                              \
	do {                                                                                                           \
		if ((cond_level) <= (level)) {                                                                           \
			U_LOG(level, __VA_ARGS__);                                                                         \
		}                                                                                                      \
	} while (false)

#define U_LOG_T(...) U_LOG(U_LOGGING_TRACE, __VA_ARGS__)
#define U_LOG_D(...) U_LOG(U_LOGGING_DEBUG, __VA_ARGS__)
#define U_LOG_I(...) U_LOG(U_LOGGING_INFO, __VA_ARGS__)
#define U_LOG_W(...) U_LOG(U_LOGGING_WARN, __VA_ARGS__)
#define U_LOG_E(...) U_LOG(U_LOGGING_ERROR, __VA_ARGS__)

#define U_LOG_IFL_T(level, ...) U_LOG_IFL(U_LOGGING_TRACE, level, __VA_ARGS__)
#define U_LOG_IFL_D(level, ...) U_LOG_IFL(U_LOGGING_DEBUG, level, __VA_ARGS__)
#define U_LOG_IFL_I(level, ...) U_LOG_IFL(U_LOGGING_INFO, level, __VA_ARGS__)
#define U_LOG_IFL_W(level, ...) U_LOG_IFL(U_LOGGING_WARN, level, __VA_ARGS__)
#define U_LOG_IFL_E(level, ...) U_LOG_IFL(U_LOGGING_ERROR, level, __VA_ARGS__)

#ifdef __cplusplus
}
#endif
