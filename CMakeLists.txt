cmake_minimum_required(VERSION 3.16)
project(skreeu1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# SKREEU1_DEVICE=ON：交叉编译给 Snapmaker U1（aarch64，/dev/fb0 + evdev），
# 不依赖 SDL；OFF 时是桌面模拟器（SDL 窗口 + 鼠标）。
option(SKREEU1_DEVICE "Build for the U1 panel (fbdev + evdev instead of SDL)" OFF)

# 只用于 SKREEU1_DEVICE：链接目标机上真正在跑的 .so（从机器上拷下来的 sysroot）
set(SKREEU1_SYSROOT ${CMAKE_CURRENT_SOURCE_DIR}/sysroot CACHE PATH "Target rootfs libraries")

if(NOT LVGL_DIR)
    set(LVGL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lvgl)
endif()

find_package(Threads REQUIRED)

if(SKREEU1_DEVICE)
    # 头文件用宿主机的 dev 包（纯文本，架构无关），库用目标机的 .so，
    # 这样链接出来的 DT_NEEDED 与机器上实际的 soname 完全一致。
    # 路径写死：CMAKE_FIND_ROOT_PATH 会把 find_path 的 PATHS 也重定到 /usr 下。
    set(FT_INCLUDE_DIRS /usr/include/freetype2)
    set(FT_LIBRARIES ${SKREEU1_SYSROOT}/lib/libfreetype.so.6)

    set(PNG_INCLUDE_DIR /usr/include)
    set(PNG_LIBRARY ${SKREEU1_SYSROOT}/lib/libpng16.so.16)

    # 容器里把 curl 头文件单独放到 /opt/u1include，避免 -I 到宿主 multiarch 目录
    set(CURL_INCLUDE_DIR /opt/u1include)
    set(CURL_LIBRARY ${SKREEU1_SYSROOT}/lib/libcurl.so.4)

    foreach(_p ${FT_INCLUDE_DIRS}/ft2build.h ${PNG_INCLUDE_DIR}/png.h
               ${CURL_INCLUDE_DIR}/curl/curl.h ${FT_LIBRARIES} ${PNG_LIBRARY} ${CURL_LIBRARY})
        if(NOT EXISTS ${_p})
            message(FATAL_ERROR "device build: missing ${_p}")
        endif()
    endforeach()
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FT REQUIRED freetype2)
    pkg_check_modules(SDL2 REQUIRED sdl2)
    find_package(PNG REQUIRED)
    find_package(CURL REQUIRED)
endif()

set(LV_BUILD_CONF_PATH ${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h CACHE PATH "" FORCE)
add_subdirectory(${LVGL_DIR} ${CMAKE_BINARY_DIR}/lvgl)

# LVGL's FreeType driver needs the headers/libs on the lvgl target itself
target_include_directories(lvgl PRIVATE ${FT_INCLUDE_DIRS})
target_link_libraries(lvgl PRIVATE ${FT_LIBRARIES})

if(SKREEU1_DEVICE)
    target_compile_definitions(lvgl PUBLIC SKREEU1_DEVICE=1)
else()
    target_include_directories(lvgl PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(lvgl PRIVATE ${SDL2_LIBRARIES})
endif()

add_executable(skreeu1
    src/main.cpp
    src/theme.cpp
    src/ui.cpp
    src/moonraker.cpp
    src/wifi.cpp
)

target_compile_definitions(skreeu1 PRIVATE
    LV_CONF_PATH="${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h"
)

target_include_directories(skreeu1 PRIVATE
    src
    third_party
    ${FT_INCLUDE_DIRS}
)

target_link_libraries(skreeu1 PRIVATE lvgl ${FT_LIBRARIES} Threads::Threads m)

if(SKREEU1_DEVICE)
    target_compile_definitions(skreeu1 PRIVATE SKREEU1_DEVICE=1)
    # LVGL 的 fbdev / evdev 驱动头文件没有汇总进 lvgl.h
    target_include_directories(skreeu1 PRIVATE
        ${LVGL_DIR}/src/drivers/display/fb
        ${LVGL_DIR}/src/drivers/evdev
        ${PNG_INCLUDE_DIR}
        ${CURL_INCLUDE_DIR}
    )
    target_link_libraries(skreeu1 PRIVATE ${PNG_LIBRARY} ${CURL_LIBRARY})
    # 目标机是 Buildroot glibc 2.38 + gcc12，静态链 libstdc++ 避免版本漂移。
    # libcurl/libfreetype 的传递依赖（libssl3、libbz2……）在目标机上运行时解析，
    # 链接期不需要它们的 .so。
    target_link_options(skreeu1 PRIVATE
        -static-libstdc++ -static-libgcc
        -Wl,--allow-shlib-undefined)
else()
    target_include_directories(skreeu1 PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(skreeu1 PRIVATE ${SDL2_LIBRARIES} PNG::PNG CURL::libcurl)
endif()

target_compile_options(skreeu1 PRIVATE -O2 -Wall -Wno-unused-parameter)
