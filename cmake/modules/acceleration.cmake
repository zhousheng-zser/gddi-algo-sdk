set(LinkLibraries "${LinkLibraries};gddeploy_app;gddeploy_api;gddeploy_core;gddeploy_register;gddeploy_common;bmrt")
set(SDK_DOWNLOAD_URL "https://github.com/gddi-cn/archive/raw/refs/heads/main/bm1684/bm1684-v1.0.30.tar.gz")
set(SDK_URL_HASH "18fbbd882166565dff5b9358cad6f24253b09d9a84526f2a0dafef70970a30f5")

include(FetchContent)
FetchContent_Declare(
    inference-sdk
    URL ${SDK_DOWNLOAD_URL}
    URL_HASH SHA256=${SDK_URL_HASH}
)

FetchContent_MakeAvailable(inference-sdk)
include_directories(${inference-sdk_SOURCE_DIR}/include)
link_directories(${inference-sdk_SOURCE_DIR}/lib)