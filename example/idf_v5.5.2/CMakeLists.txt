cmake_minimum_required(VERSION 3.16)

file(TO_CMAKE_PATH "$ENV{IDF_PATH}/components/esp_lcd/rgb/esp_lcd_panel_rgb.c" ESP_LCD_RGB_SRC)
set_source_files_properties(${ESP_LCD_RGB_SRC} PROPERTIES COMPILE_FLAGS "-O0")

set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/components/bsp"
    "${CMAKE_CURRENT_LIST_DIR}/components/Middleware"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ESP32_S3_Matrix)
