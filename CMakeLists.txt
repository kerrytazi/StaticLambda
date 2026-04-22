cmake_minimum_required(VERSION 4.3)
project("StaticLambda")

option(STATIC_LAMBDA_EXAMPLE "Build example project" OFF)

add_library(StaticLambda)

target_sources(StaticLambda
	PRIVATE
		src/StaticLambda.cpp

		include/StaticLambda/StaticLambda.hpp
)

target_include_directories(StaticLambda
	PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(StaticLambda PRIVATE cxx_std_23)
set_target_properties(StaticLambda PROPERTIES CXX_SCAN_FOR_MODULES OFF)

if(STATIC_LAMBDA_EXAMPLE)
	add_executable(StaticLambdaExample)

	target_sources(StaticLambdaExample
		PRIVATE
			example/main.cpp
	)

	target_compile_features(StaticLambdaExample PRIVATE cxx_std_23)

	target_link_libraries(StaticLambdaExample StaticLambda)

	set_property(GLOBAL PROPERTY USE_FOLDERS ON)
	set_property(DIRECTORY "${CMAKE_CURRENT_LIST_DIR}" PROPERTY VS_STARTUP_PROJECT StaticLambdaExample)
endif()
