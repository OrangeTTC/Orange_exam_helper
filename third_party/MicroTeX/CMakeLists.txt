# Copyright 2026 Orange. SPDX-License-Identifier: Apache-2.0
# 本项目的 Qt6 集成配置；上游代码保持其 MIT 许可，字体保留各自许可。
file(GLOB_RECURSE microtex_sources CONFIGURE_DEPENDS
    src/atom/*.cpp src/box/*.cpp src/core/*.cpp src/fonts/*.cpp src/utils/*.cpp src/res/*.cpp)
add_library(orange_microtex STATIC ${microtex_sources} src/latex.cpp src/render.cpp
    src/platform/qt/graphic_qt.cpp)
target_include_directories(orange_microtex SYSTEM PUBLIC src)
target_compile_definitions(orange_microtex PUBLIC BUILD_QT)
target_link_libraries(orange_microtex PRIVATE orange_tinyxml2 Qt6::Gui)
set_target_properties(orange_microtex PROPERTIES AUTOMOC OFF)
if(MSVC)
    target_compile_options(orange_microtex PRIVATE /utf-8)
endif()

# 字体嵌入可执行文件，无须安装 TeX、联网或依赖用户当前工作目录。
file(GLOB_RECURSE font_files RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" res/fonts/*.ttf)
set(font_qrc "<RCC><qresource prefix=\"/orange-tex\">\n")
foreach(font_file IN LISTS font_files)
    string(REGEX REPLACE "^res/" "" font_alias "${font_file}")
    string(APPEND font_qrc "<file alias=\"${font_alias}\">${CMAKE_CURRENT_SOURCE_DIR}/${font_file}</file>\n")
endforeach()
string(APPEND font_qrc "</qresource></RCC>")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/orange_math_fonts.qrc" "${font_qrc}")
qt_add_resources(math_font_sources "${CMAKE_CURRENT_BINARY_DIR}/orange_math_fonts.qrc")
target_sources(orange_microtex PRIVATE ${math_font_sources})
