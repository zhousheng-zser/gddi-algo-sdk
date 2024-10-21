set(LinkLibraries "${LinkLibraries};gddeploy_app;gddeploy_api;gddeploy_core;gddeploy_register;gddeploy_common;bmrt")
set(SDK_DOWNLOAD_URL "http://cacher.devops.io/api/cacher/files/40fb56d71052ddb5850bda7094176fcf8fc49e62183b2203eda6245fc752a485")
set(SDK_URL_HASH "40fb56d71052ddb5850bda7094176fcf8fc49e62183b2203eda6245fc752a485")

include(FetchContent)
FetchContent_Declare(
    inference-sdk
    URL ${SDK_DOWNLOAD_URL}
    URL_HASH SHA256=${SDK_URL_HASH}
)

FetchContent_MakeAvailable(inference-sdk)
include_directories(${inference-sdk_SOURCE_DIR}/include)
link_directories(${inference-sdk_SOURCE_DIR}/lib)