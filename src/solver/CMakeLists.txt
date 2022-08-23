cmake_minimum_required(VERSION 2.8.12)

project(recalage)

find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

SET(CMAKE_CXX_FLAGS "-std=c++0x")
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY "build")

add_executable(camCalibNode camCalibNode.cpp)
target_link_libraries(
	camCalibNode
	${OpenCV_LIBS}
)


