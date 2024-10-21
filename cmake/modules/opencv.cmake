find_package(OpenCV REQUIRED)
message(STATUS "Found OpenCV: ${OpenCV_CONFIG} (found version \"${OpenCV_VERSION}\")")
set(LinkLibraries "${LinkLibraries};${OpenCV_LIBRARIES}")