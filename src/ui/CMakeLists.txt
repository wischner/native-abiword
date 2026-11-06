cmake_minimum_required(VERSION 3.20)
project(fyodor_native_dependency LANGUAGES CXX)
# Native permits consumers to choose their own root build directory.
add_subdirectory("${NATIVE_ROOT}" native)

# Native's public headers are external to fyodor. Treat them as system
# headers in both the dependency and its consumers.
get_target_property(fyodor_native_includes native INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories(native SYSTEM PUBLIC ${fyodor_native_includes})
