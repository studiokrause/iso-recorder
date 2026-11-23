cmake_minimum_required(VERSION 3.24)
project(IsoRecorder VERSION 3.5.0 LANGUAGES CXX)
set(APP_VERSION_SHORT "3.5")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

# GStreamer (MSVC x64) - z instalacji systemowej; bez pkg-config, sciezki jawnie.
set(GST_ROOT "$ENV{GSTREAMER_1_0_ROOT_MSVC_X86_64}" CACHE PATH "Katalog GStreamera MSVC x86_64")
file(TO_CMAKE_PATH "${GST_ROOT}" GST_ROOT)
string(REGEX REPLACE "/$" "" GST_ROOT "${GST_ROOT}")
if(NOT EXISTS "${GST_ROOT}/include/gstreamer-1.0/gst/gst.h")
    message(FATAL_ERROR "Nie znaleziono GStreamera (pakiet development) w '${GST_ROOT}'. Ustaw GST_ROOT lub GSTREAMER_1_0_ROOT_MSVC_X86_64.")
endif()

file(GLOB_RECURSE SRC CONFIGURE_DEPENDS src/*.cpp src/*.h)
# Ikona: res/app.rc daje ikone pliku EXE (Eksplorator, pasek zadan), zasob Qt - ikone okien programu.
qt_add_executable(IsoRecorder WIN32 ${SRC} res/app.rc)
qt_add_resources(IsoRecorder "appicon" PREFIX "/" BASE res FILES res/icon.png res/splash.png)

target_include_directories(IsoRecorder PRIVATE
    src
    "${GST_ROOT}/include/gstreamer-1.0"
    "${GST_ROOT}/include/glib-2.0"
    "${GST_ROOT}/lib/glib-2.0/include")
target_link_directories(IsoRecorder PRIVATE "${GST_ROOT}/lib")
target_link_libraries(IsoRecorder PRIVATE
    Qt6::Widgets Qt6::Network
    gstreamer-1.0 gstbase-1.0 gstapp-1.0 gstvideo-1.0 gstaudio-1.0
    gobject-2.0 glib-2.0
    dwmapi user32 winmm)
target_compile_definitions(IsoRecorder PRIVATE
    UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN
    APP_VERSION="${APP_VERSION_SHORT}")
target_compile_options(IsoRecorder PRIVATE /W4 /permissive- /utf-8 /MP)

# Narzedzie testowe: maly serwer RTSP (tests\net_test.ps1). Nie trafia do paczki.
if(EXISTS "${GST_ROOT}/include/gstreamer-1.0/gst/rtsp-server/rtsp-server.h")
    add_executable(rtsp_test_server tests/tools/rtsp_test_server.cpp)
    target_include_directories(rtsp_test_server PRIVATE
        "${GST_ROOT}/include/gstreamer-1.0"
        "${GST_ROOT}/include/glib-2.0"
        "${GST_ROOT}/lib/glib-2.0/include")
    target_link_directories(rtsp_test_server PRIVATE "${GST_ROOT}/lib")
    target_link_libraries(rtsp_test_server PRIVATE gstrtspserver-1.0 gstreamer-1.0 gobject-2.0 glib-2.0)
    target_compile_options(rtsp_test_server PRIVATE /utf-8)
endif()
