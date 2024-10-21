# mpp
pkg_check_modules(rockchip QUIET IMPORTED_TARGET rockchip_mpp)
if (rockchip_FOUND)
    message(STATUS "Found rockchip: ${rockchip_CONFIG} (found version \"${rockchip_VERSION}\")")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    ExternalProject_Add(
        rockchip_external
        GIT_REPOSITORY http://git.mirror.gddi.io/mirror/mpp.git
        GIT_TAG 1.0.5
        PREFIX ${EXTERNAL_INSTALL_LOCATION}
        CMAKE_ARGS -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=${EXTERNAL_INSTALL_LOCATION}
            -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DARM=ON -DARMEABI_V7A_HARDFP=ON
            -DCMAKE_SYSTEM_PROCESSOR=${CMAKE_SYSTEM_PROCESSOR} -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER} -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        BUILD_BYPRODUCTS ${EXTERNAL_INSTALL_LOCATION}/lib/librockchip_mpp.so ${EXTERNAL_INSTALL_LOCATION}/lib/librockchip_vpu.so
    )
else()
    ExternalProject_Add(
        rockchip_external
        GIT_REPOSITORY http://git.mirror.gddi.io/mirror/mpp.git
        GIT_TAG 1.0.5
        PREFIX ${EXTERNAL_INSTALL_LOCATION}
        CMAKE_ARGS -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=${EXTERNAL_INSTALL_LOCATION}
            -DCMAKE_POSITION_INDEPENDENT_CODE=ON
            -DCMAKE_SYSTEM_PROCESSOR=${CMAKE_SYSTEM_PROCESSOR} -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER} -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        BUILD_BYPRODUCTS ${EXTERNAL_INSTALL_LOCATION}/lib/librockchip_mpp.so ${EXTERNAL_INSTALL_LOCATION}/lib/librockchip_vpu.so
    )
endif()

add_library(rockchip_mpp SHARED IMPORTED)
add_dependencies(rockchip_mpp rockchip_external)
set_target_properties(rockchip_mpp PROPERTIES IMPORTED_LOCATION "${EXTERNAL_INSTALL_LOCATION}/lib/librockchip_mpp.so")
include_directories("${EXTERNAL_INSTALL_LOCATION}/include/rockchip")

# rga
find_library(RGA NAMES rga HINTS "${EXTERNAL_INSTALL_LOCATION}/lib")
if (RGA)
    message(STATUS "Found RGA: ${RGA}")
else()
    set(FILE_PATH "${CMAKE_BINARY_DIR}/rga-1.9.1.zip")
    if(NOT EXISTS "${FILE_PATH}")
        set(FILE_URL "https://github.com/airockchip/librga/archive/refs/tags/v1.9.1.zip")
        file(DOWNLOAD "${FILE_URL}" "${FILE_PATH}")
    endif()

    execute_process(COMMAND ${CMAKE_COMMAND} -E tar xzf "${FILE_PATH}" WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")

    set(RGA_DIR "${CMAKE_BINARY_DIR}/librga-1.9.1")
    if (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "aarch64")
        file(COPY "${RGA_DIR}/libs/Linux/gcc-aarch64/librga.so" DESTINATION "${EXTERNAL_INSTALL_LOCATION}/lib")
    else()
        file(COPY "${RGA_DIR}/libs/Linux/gcc-armhf/librga.so" DESTINATION "${EXTERNAL_INSTALL_LOCATION}/lib")
    endif()

    file(GLOB INCLUDE_FILES "${RGA_DIR}/include/*")
    foreach(INCLUDE_FILE ${INCLUDE_FILES})
        file(COPY "${INCLUDE_FILE}" DESTINATION "${EXTERNAL_INSTALL_LOCATION}/include/rga")
    endforeach()

    file(REMOVE_RECURSE "${CMAKE_BINARY_DIR}/librga-1.9.1")
    file(REMOVE "${FILE_PATH}")
endif()

include_directories("${EXTERNAL_INSTALL_LOCATION}/include/rga")
set(LinkLibraries "${LinkLibraries};rga")