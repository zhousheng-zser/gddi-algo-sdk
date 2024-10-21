find_package(concurrentqueue QUIET)

if(concurrentqueue_FOUND)
    message(STATUS "Found concurrentqueue: ${concurrentqueue_CONFIG} (found version \"${concurrentqueue_VERSION}\")")
else()
    ExternalProject_Add(
        concurrentqueue_external
        GIT_REPOSITORY https://github.com/cameron314/concurrentqueue.git
        GIT_TAG master
        PREFIX ${EXTERNAL_INSTALL_LOCATION}
        CMAKE_ARGS -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=${EXTERNAL_INSTALL_LOCATION}
            -DCMAKE_POSITION_INDEPENDENT_CODE=ON
            -DCMAKE_SYSTEM_PROCESSOR=${CMAKE_SYSTEM_PROCESSOR} -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
    )

    add_library(concurrentqueue INTERFACE IMPORTED)
    add_dependencies(concurrentqueue concurrentqueue_external)
endif()

include_directories(${EXTERNAL_INSTALL_LOCATION}/include/concurrentqueue)