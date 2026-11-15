cmake_minimum_required(VERSION 3.20)
project(fyodor_native_dependency LANGUAGES CXX)
# The form uses Native's explicit tab order and separate form-control metric.
file(READ "${NATIVE_ROOT}/include/native/wnd.h" fyodor_native_wnd_header)
file(READ "${NATIVE_ROOT}/include/native/theme.h" fyodor_native_theme_header)
if(NOT fyodor_native_wnd_header MATCHES "set_tab_order" OR
   NOT fyodor_native_theme_header MATCHES "get_form_control_height")
    message(FATAL_ERROR "Fyodor needs Native 426a42f or newer. Run make update-native, or update NATIVE_ROOT.")
endif()
# Serialize compatibility changes when two backend configurations start together.
file(LOCK "${CMAKE_CURRENT_LIST_DIR}/../../build/native-dialogs.lock"
    GUARD PROCESS TIMEOUT 60)
execute_process(COMMAND sh "${CMAKE_CURRENT_LIST_DIR}/../../scripts/build/patch_native.sh"
    "${NATIVE_ROOT}" COMMAND_ERROR_IS_FATAL ANY)
# Native permits consumers to choose their own root build directory.
add_subdirectory("${NATIVE_ROOT}" native)
if(NOT TARGET native_rc)
    add_subdirectory("${NATIVE_ROOT}/tools/rc" native-rc)
endif()

# Artwork regeneration is explicit; ordinary builds use the checked-in PNGs.
add_executable(export_icons EXCLUDE_FROM_ALL ../resources/export_icons.cpp)
target_link_libraries(export_icons PRIVATE native)

# Native's public headers are external to fyodor. Treat them as system
# headers in both the dependency and its consumers.
get_target_property(fyodor_native_includes native INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories(native SYSTEM PUBLIC ${fyodor_native_includes})
