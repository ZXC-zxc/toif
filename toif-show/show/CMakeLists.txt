# CMakeList.txt: show 的 CMake 项目，在此处包括源代码并定义
# 项目特定的逻辑。
#
cmake_minimum_required (VERSION 3.8)
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
    CACHE STRING "")
    message(" [INFO] VCPKG CMAKE_TOOLCHAIN_FILE = ${CMAKE_TOOLCHAIN_FILE}")
endif()

project ("show")
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/uzlib/src SRC_LIST)


find_package(SDL2 REQUIRED)
find_package(sdl2-ttf CONFIG REQUIRED)
find_package(sdl2-image CONFIG REQUIRED)

# 将源代码添加到此项目的可执行文件。
add_executable (show "show.c" "show.h" ${SRC_LIST})

target_link_libraries(show PRIVATE SDL2::SDL2 SDL2::SDL2main SDL2::SDL2_ttf SDL2::SDL2_image vcruntime.lib)

# TODO: 如有需要，请添加测试并安装目标。
