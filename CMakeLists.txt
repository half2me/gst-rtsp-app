cmake_minimum_required(VERSION 3.5)
project(gst-rtsp-app VERSION 1.0.0)
add_definitions(-DVERSION=\"${PROJECT_VERSION}\")

#set(CMAKE_BUILD_TYPE Release)
#set(CMAKE_VERBOSE_MAKEFILE on)

# GST: Find it using pkg-config
include(FindPkgConfig)
PKG_CHECK_MODULES(GST REQUIRED # for available packages see 'pkg-config --list-all | grep gst'
  gstreamer-1.0
  gstreamer-net-1.0
  gstreamer-rtsp-server-1.0
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${GST_CFLAGS}")

include_directories(PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${GST_INCLUDE_DIRS}
)
link_libraries(
  ${GST_LDFLAGS}
)

# App target
add_executable(${CMAKE_PROJECT_NAME} "")
target_sources(${CMAKE_PROJECT_NAME}
  PRIVATE
    src/ct1-rtsp-server.c
)

# Tools
set(APP_NAME "YuvPlayer")
add_executable(${APP_NAME} "")
target_sources(${APP_NAME}
  PRIVATE
    src/PlayCT1YUV422.c
    src/rtsp-server.c
)

set(APP_NAME "appsrc")
add_executable(${APP_NAME} "")
target_sources(${APP_NAME}
  PRIVATE
    src/appsrc.c
)