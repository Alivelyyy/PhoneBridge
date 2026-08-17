cmake_minimum_required(VERSION 3.16)
project(PhoneBridge VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find required packages
find_package(PkgConfig REQUIRED)

pkg_check_modules(LIBUSB REQUIRED libusb-1.0)
pkg_check_modules(AVCODEC REQUIRED libavcodec)
pkg_check_modules(AVFORMAT REQUIRED libavformat)
pkg_check_modules(AVUTIL REQUIRED libavutil)
pkg_check_modules(SWSCALE REQUIRED libswscale)
pkg_check_modules(SWRESAMPLE REQUIRED libswresample)
pkg_check_modules(GTK4 REQUIRED gtk4)
pkg_check_modules(PIPEWIRE REQUIRED libpipewire-0.3)

# Source files
set(SOURCES
    src/main.cpp
    src/usb/usb_device.cpp
    src/protocol/protocol.cpp
    src/video/video_decoder.cpp
    src/audio/audio_handler.cpp
    src/virtual_camera/virtual_camera.cpp
    src/virtual_audio/virtual_audio.cpp
    src/config/config.cpp
    src/gui/gui.cpp
)

# Header files
set(HEADERS
    include/phonebridge/usb_device.h
    include/phonebridge/protocol.h
    include/phonebridge/video_decoder.h
    include/phonebridge/audio_handler.h
    include/phonebridge/virtual_camera.h
    include/phonebridge/virtual_audio.h
    include/phonebridge/config.h
    include/phonebridge/gui.h
)

# Create executable
add_executable(phonebridge ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(phonebridge PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${LIBUSB_INCLUDE_DIRS}
    ${AVCODEC_INCLUDE_DIRS}
    ${AVFORMAT_INCLUDE_DIRS}
    ${AVUTIL_INCLUDE_DIRS}
    ${SWSCALE_INCLUDE_DIRS}
    ${SWRESAMPLE_INCLUDE_DIRS}
    ${GTK4_INCLUDE_DIRS}
    ${PIPEWIRE_INCLUDE_DIRS}
)

# Link libraries (order matters: dependencies after dependents)
target_link_libraries(phonebridge PRIVATE
    -Wl,--start-group
    ${LIBUSB_LIBRARIES}
    ${AVFORMAT_LIBRARIES}
    ${AVCODEC_LIBRARIES}
    ${SWSCALE_LIBRARIES}
    ${AVUTIL_LIBRARIES}
    ${SWRESAMPLE_LIBRARIES}
    ${GTK4_LIBRARIES}
    ${PIPEWIRE_LIBRARIES}
    -Wl,--end-group
)

# Compile options
target_compile_options(phonebridge PRIVATE
    -Wall -Wextra -Wpedantic
)

# Install
install(TARGETS phonebridge DESTINATION bin)

# Tests
option(BUILD_TESTS "Build unit tests" ON)

if(BUILD_TESTS)
    enable_testing()

    # Test sources (only modules that don't require USB/GUI/hardware)
    set(TEST_SOURCES
        tests/test_main.cpp
        tests/test_protocol.cpp
        tests/test_config.cpp
        tests/test_audio_handler.cpp
        tests/test_video_decoder.cpp
    )

    add_executable(phonebridge_tests ${TEST_SOURCES}
        src/protocol/protocol.cpp
        src/config/config.cpp
        src/audio/audio_handler.cpp
        src/video/video_decoder.cpp
    )

    target_include_directories(phonebridge_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/tests
        ${AVCODEC_INCLUDE_DIRS}
        ${AVFORMAT_INCLUDE_DIRS}
        ${AVUTIL_INCLUDE_DIRS}
        ${SWSCALE_INCLUDE_DIRS}
        ${SWRESAMPLE_INCLUDE_DIRS}
    )

    target_link_libraries(phonebridge_tests PRIVATE
        ${AVCODEC_LIBRARIES}
        ${AVFORMAT_LIBRARIES}
        ${AVUTIL_LIBRARIES}
        ${SWSCALE_LIBRARIES}
        ${SWRESAMPLE_LIBRARIES}
    )

    target_compile_options(phonebridge_tests PRIVATE
        -Wall -Wextra -Wpedantic
    )

    add_test(NAME phonebridge_tests COMMAND phonebridge_tests)
endif()
