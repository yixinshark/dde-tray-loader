# SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: CC0-1.0

set(PLUGIN_NAME "disk-mount")

project(${PLUGIN_NAME})

# Sources files
file(GLOB SRCS "*.h" "*.cpp")

find_package(PkgConfig REQUIRED)
find_package(Qt5Widgets REQUIRED)
find_package(Qt5Svg REQUIRED)
find_package(Qt5DBus REQUIRED)
find_package(DtkWidget REQUIRED)

add_definitions("${QT_DEFINITIONS} -DQT_PLUGIN")
add_library(${PLUGIN_NAME} SHARED ${SRCS} resources.qrc)
set_target_properties(${PLUGIN_NAME} PROPERTIES LIBRARY_OUTPUT_DIRECTORY ../)
target_include_directories(${PLUGIN_NAME} PUBLIC ${DtkWidget_INCLUDE_DIRS} ../../interfaces)
target_link_libraries(${PLUGIN_NAME} PRIVATE
    ${DtkWidget_LIBRARIES}
    ${Qt5Widgets_LIBRARIES}
    ${Qt5Svg_LIBRARIES}
    ${Qt5DBus_LIBRARIES}
)

install(TARGETS ${PLUGIN_NAME} LIBRARY DESTINATION lib/dde-dock/plugins)
