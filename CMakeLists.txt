# Copyright 2026 Zhang
# SPDX-License-Identifier: BSL-1.0
cmake_minimum_required(VERSION 3.22)

project(
	MercuryHandpose
	VERSION 0.1.0
	DESCRIPTION "Standalone stereo fisheye hand-pose tracker extracted from Monado Mercury"
	LANGUAGES CXX
)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(MERCURY_BUILD_EXAMPLES "Build the stereo video/camera demo" ON)
option(MERCURY_BUILD_TESTS "Build unit and smoke tests" ON)
set(MERCURY_MODELS_DIR "" CACHE PATH "Optional model directory used by the pipeline smoke test")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(Threads REQUIRED)
find_package(Eigen3 REQUIRED NO_MODULE)
if(Eigen3_VERSION VERSION_LESS 3.3)
	message(FATAL_ERROR "Mercury Handpose requires Eigen 3.3 or newer")
endif()
find_package(OpenCV REQUIRED COMPONENTS core imgproc)
find_package(ONNXRuntime REQUIRED)

add_library(
	mercury_handpose
	src/hand_tracker.cpp
	src/compat/compat.cpp
	src/compat/worker.cpp
	src/compat/onnx_wrapper.cpp
	src/compat/t_camera_models.cpp
	src/compat/m_base.cpp
	src/mercury/hg_model.cpp
	src/mercury/hg_image_distorter.cpp
	src/mercury/hg_sync.cpp
	src/mercury/kine_lm/lm_main.cpp
	src/mercury/kine_lm/lm_hand_init_guesser.cpp
)
add_library(Mercury::handpose ALIAS mercury_handpose)
set_target_properties(mercury_handpose PROPERTIES EXPORT_NAME handpose)

target_include_directories(
	mercury_handpose
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/src/mercury
		${CMAKE_CURRENT_SOURCE_DIR}/src/mercury/kine_lm
		${CMAKE_CURRENT_SOURCE_DIR}/src/compat/include
		${CMAKE_CURRENT_SOURCE_DIR}/third_party/tinyceres/include
)

target_link_libraries(
	mercury_handpose
	PRIVATE
		Eigen3::Eigen
		ONNXRuntime::ONNXRuntime
		Threads::Threads
		${OpenCV_LIBS}
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
	target_compile_options(
		mercury_handpose
		PRIVATE
			$<$<CONFIG:Debug>:-O2>
			$<$<NOT:$<CONFIG:Debug>>:-O3>
			-Wall
			-Wextra
			-Wno-unused-parameter
	)
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	set_source_files_properties(
		src/mercury/kine_lm/lm_main.cpp
		src/mercury/kine_lm/lm_hand_init_guesser.cpp
		PROPERTIES COMPILE_OPTIONS "-ftemplate-backtrace-limit=20"
	)
endif()

if(MERCURY_BUILD_EXAMPLES)
	find_package(OpenCV REQUIRED COMPONENTS core imgproc highgui videoio imgcodecs)
	add_executable(mercury_stereo_demo examples/stereo_demo.cpp)
	target_link_libraries(mercury_stereo_demo PRIVATE Mercury::handpose ${OpenCV_LIBS})
	target_compile_features(mercury_stereo_demo PRIVATE cxx_std_20)
endif()

if(MERCURY_BUILD_TESTS)
	include(CTest)
	add_executable(mercury_camera_model_test tests/camera_model_test.cpp)
	target_include_directories(
		mercury_camera_model_test
		PRIVATE
			${CMAKE_CURRENT_SOURCE_DIR}/src/compat/include
	)
	target_link_libraries(mercury_camera_model_test PRIVATE Mercury::handpose)
	add_test(NAME camera_model_roundtrip COMMAND mercury_camera_model_test)

	add_executable(mercury_optimizer_test tests/optimizer_test.cpp)
	target_include_directories(
		mercury_optimizer_test
		PRIVATE
			${CMAKE_CURRENT_SOURCE_DIR}/src/mercury
			${CMAKE_CURRENT_SOURCE_DIR}/src/mercury/kine_lm
			${CMAKE_CURRENT_SOURCE_DIR}/src/compat/include
	)
	target_link_libraries(mercury_optimizer_test PRIVATE Mercury::handpose)
	add_test(NAME optimizer_finite_output COMMAND mercury_optimizer_test)

	add_executable(mercury_calibration_test tests/calibration_test.cpp)
	target_link_libraries(mercury_calibration_test PRIVATE Mercury::handpose)
	add_test(
		NAME calibration_json_v2
		COMMAND mercury_calibration_test ${CMAKE_CURRENT_SOURCE_DIR}/config/calibration.example.json
	)

	if(MERCURY_MODELS_DIR)
		add_executable(mercury_pipeline_smoke_test tests/pipeline_smoke_test.cpp)
		target_link_libraries(mercury_pipeline_smoke_test PRIVATE Mercury::handpose)
		add_test(
			NAME pipeline_blank_stereo
			COMMAND mercury_pipeline_smoke_test
				${CMAKE_CURRENT_SOURCE_DIR}/config/calibration.example.json
				${MERCURY_MODELS_DIR}
		)

		add_executable(mercury_initialization_failure_test tests/initialization_failure_test.cpp)
		target_link_libraries(mercury_initialization_failure_test PRIVATE Mercury::handpose)
		add_test(
			NAME invalid_model_cleanup
			COMMAND mercury_initialization_failure_test
				${CMAKE_CURRENT_SOURCE_DIR}/config/calibration.example.json
				${MERCURY_MODELS_DIR}
		)
	endif()
endif()

install(
	TARGETS mercury_handpose
	EXPORT MercuryHandposeTargets
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(
	EXPORT MercuryHandposeTargets
	FILE MercuryHandposeTargets.cmake
	NAMESPACE Mercury::
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MercuryHandpose
)

configure_package_config_file(
	cmake/MercuryHandposeConfig.cmake.in
	${CMAKE_CURRENT_BINARY_DIR}/MercuryHandposeConfig.cmake
	INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MercuryHandpose
)
write_basic_package_version_file(
	${CMAKE_CURRENT_BINARY_DIR}/MercuryHandposeConfigVersion.cmake
	VERSION ${PROJECT_VERSION}
	COMPATIBILITY SameMajorVersion
)
install(
	FILES
		${CMAKE_CURRENT_BINARY_DIR}/MercuryHandposeConfig.cmake
		${CMAKE_CURRENT_BINARY_DIR}/MercuryHandposeConfigVersion.cmake
		cmake/FindONNXRuntime.cmake
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MercuryHandpose
)
install(
	FILES LICENSE README.md THIRD_PARTY_NOTICES.md UPSTREAM.md
	DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/MercuryHandpose
)
install(
	DIRECTORY LICENSES/
	DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/MercuryHandpose/LICENSES
)
