# Writer (codename fyodor) on any Native backend. `make` remains the
# local entry point; this file is what the platform SDKs build.
cmake_minimum_required(VERSION 3.20)
project(fyodor LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(FYODOR_VERSION "0.1.0" CACHE STRING "Application version")
if(NOT NATIVE_ROOT)
    get_filename_component(NATIVE_ROOT "${CMAKE_SOURCE_DIR}/../native" ABSOLUTE)
endif()
set(BUILD_TESTING OFF CACHE BOOL "Native standalone tests" FORCE)
add_subdirectory(src/ui native-dependency)

set(libraries impexp edit fmt gfx doc util)
add_library(fyodor_options INTERFACE)
target_include_directories(fyodor_options INTERFACE include)
foreach(name ${libraries})
    target_include_directories(fyodor_options INTERFACE "lib/${name}/include")
endforeach()
target_compile_options(fyodor_options INTERFACE
    $<$<CXX_COMPILER_ID:GNU>:-Wall;-Wextra;-pedantic>)
target_compile_definitions(fyodor_options INTERFACE
    FYODOR_VERSION="${FYODOR_VERSION}")

set(core_sources)
foreach(name ${libraries})
    file(GLOB sources CONFIGURE_DEPENDS "lib/${name}/*.cpp")
    list(APPEND core_sources ${sources})
endforeach()
add_library(fyodor_core STATIC ${core_sources})
target_link_libraries(fyodor_core PUBLIC fyodor_options)

file(GLOB ui_sources CONFIGURE_DEPENDS src/ui/*.cpp src/app/*.cpp)
add_library(fyodor_ui STATIC ${ui_sources})
target_include_directories(fyodor_ui PUBLIC src/ui)
target_link_libraries(fyodor_ui PUBLIC fyodor_core native)

add_executable(fyodor WIN32 src/main.cpp)
target_link_libraries(fyodor PRIVATE fyodor_ui)

add_executable(test_desktop tests/ui/test_desktop.cpp)
target_include_directories(test_desktop PRIVATE tests/unit)
target_compile_definitions(test_desktop PRIVATE
    FIXTURES="${CMAKE_SOURCE_DIR}/tests/fixtures")
target_link_libraries(test_desktop PRIVATE fyodor_ui)
