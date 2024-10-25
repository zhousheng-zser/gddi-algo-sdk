cmake_minimum_required(VERSION 3.14.0)
project(gddi-algo-sdk VERSION 1.0.3 LANGUAGES CXX C)

include(FetchContent)
include(ExternalProject)

if(MSVC)
    set(CMAKE_CXX_STANDARD 20)
else()
    set(CMAKE_CXX_STANDARD 17)
endif()

set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_FIND_ROOT_PATH}/include)

set(EXTERNAL_INSTALL_LOCATION ${CMAKE_BINARY_DIR}/external)
include_directories(${EXTERNAL_INSTALL_LOCATION}/include)
link_directories(${EXTERNAL_INSTALL_LOCATION}/lib)
list(APPEND CMAKE_FIND_ROOT_PATH ${EXTERNAL_INSTALL_LOCATION})
set(ENV{PKG_CONFIG_PATH} ${EXTERNAL_INSTALL_LOCATION}/share/pkgconfig:$ENV{PKG_CONFIG_PATH})
set(ENV{PKG_CONFIG_PATH} ${EXTERNAL_INSTALL_LOCATION}/lib/pkgconfig:$ENV{PKG_CONFIG_PATH})

# 引入CMake片段代码新方法
file(GLOB CMakeFiles "cmake/modules/*.cmake")

foreach(cmake_module_file IN LISTS CMakeFiles)
    get_filename_component(CMakeName ${cmake_module_file} NAME_WLE)
    get_filename_component(CMakePath ${cmake_module_file} DIRECTORY)
    message("LoadCMake: ${CMakeName}")
    include(${cmake_module_file})
endforeach(cmake_module_file)

add_compile_definitions(BOOST_ALLOW_DEPRECATED_HEADERS)

file(GLOB_RECURSE SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c??")
add_library(gddalgo SHARED ${SRC_FILES})
target_link_libraries(gddalgo ${LinkLibraries})
set_target_properties(gddalgo PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION 1)

file(GLOB SAMPLE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/samples/*.c??")
foreach(file IN LISTS SAMPLE_FILES)
    get_filename_component(ProgramName ${file} NAME_WE)
    add_executable(${ProgramName} ${file})
    target_link_libraries(${ProgramName} gddalgo ${LinkLibraries} pthread dl)
endforeach(file)

set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/release")
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/ DESTINATION ${CMAKE_INSTALL_PREFIX}/include)
    install(TARGETS gddalgo RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/samples/ DESTINATION ${CMAKE_INSTALL_PREFIX}/samples)
endif()