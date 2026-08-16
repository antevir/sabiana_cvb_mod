cmake_minimum_required(VERSION 3.16)
project(hmi_pc_sim C CXX)

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(LVGL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.pio/libdeps/hmi/lvgl")
if(NOT EXISTS "${LVGL_DIR}/lvgl.h")
  message(FATAL_ERROR "LVGL not found at ${LVGL_DIR}. Build the firmware env once so PlatformIO downloads lvgl.")
endif()

file(GLOB_RECURSE LVGL_SOURCES "${LVGL_DIR}/src/*.c")
list(FILTER LVGL_SOURCES EXCLUDE REGEX ".*/libs/thorvg/.*")
list(FILTER LVGL_SOURCES EXCLUDE REGEX ".*/others/vg_lite_tvg/.*")

add_library(lvgl STATIC ${LVGL_SOURCES})
target_include_directories(lvgl PUBLIC
  "${LVGL_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_compile_definitions(lvgl PUBLIC
  LV_CONF_INCLUDE_SIMPLE
  LV_LVGL_H_INCLUDE_SIMPLE
)

add_executable(hmi_pc_sim
  src/main.cpp
  ../src/assets/img_blue_bg.c
  ../src/assets/img_fan.c
  ../src/assets/img_heat.c
  ../src/assets/img_wifi.c
  ../src/assets/img_no_wifi.c
  ../src/assets/amarath_40.c
)

target_include_directories(hmi_pc_sim PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/stubs"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src"
)
target_compile_definitions(hmi_pc_sim PRIVATE
  LV_CONF_INCLUDE_SIMPLE
  LV_LVGL_H_INCLUDE_SIMPLE
)
target_link_libraries(hmi_pc_sim PRIVATE lvgl)

if(MSVC)
  target_compile_definitions(lvgl PRIVATE _CRT_SECURE_NO_WARNINGS)
  target_compile_definitions(hmi_pc_sim PRIVATE _CRT_SECURE_NO_WARNINGS)
  target_compile_options(lvgl PRIVATE /W3 /wd4244 /wd4267)
  target_compile_options(hmi_pc_sim PRIVATE /W3 /wd4244)
endif()
