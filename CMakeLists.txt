cmake_minimum_required(VERSION 3.5)
project(gst-rtsp-app VERSION 1.0.0)
add_definitions(-DVERSION=\"${PROJECT_VERSION}\")
include(FindPkgConfig)

#set(CMAKE_BUILD_TYPE Release)
#set(CMAKE_VERBOSE_MAKEFILE on)
set(CMAKE_CXX_STANDARD 11)

# GST
PKG_CHECK_MODULES(
        GST REQUIRED # for available packages see 'pkg-config --list-all | grep gst'
        gstreamer-1.0
        gstreamer-net-1.0
        gstreamer-rtsp-server-1.0
)

# RapidJSON
# Needs subproject for installer
#PKG_CHECK_MODULES(
#        RAPIDJSON REQUIRED RapidJSON
#)

include_directories(
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
#        ${RAPIDJSON_INCLUDE_DIRS}
        ${GST_INCLUDE_DIRS}
)

link_libraries(
        ${GST_LDFLAGS}
)

# App target
add_executable(
        ${CMAKE_PROJECT_NAME} ""
)

target_sources(
        ${CMAKE_PROJECT_NAME} PRIVATE
        src/main.cpp
        src/server.cpp
        src/topology.cpp
        src/logger.cpp
		src/json.cpp
)

set(
	EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/bin
)

