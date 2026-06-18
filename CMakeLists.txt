cmake_minimum_required(VERSION 3.16)
set(ADDON_NAME tcxQTMRT)

# Vendored Qualisys C++ SDK (has its own CMakeLists; build static, no examples/tests).
set(qualisys_cpp_sdk_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(qualisys_cpp_sdk_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(qualisys_cpp_sdk_OUTPUT_TYPE "STATIC" CACHE STRING "" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/qualisys_cpp_sdk)

# TrussC core headers require C++20 (concepts / requires). The vendored SDK
# only needs C++14, so 20 satisfies both.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB ADDON_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
file(GLOB ADDON_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h)

add_library(${ADDON_NAME} STATIC ${ADDON_SOURCES} ${ADDON_HEADERS})
target_include_directories(${ADDON_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/qualisys_cpp_sdk
)
target_link_libraries(${ADDON_NAME} PUBLIC
    TrussC
    qualisys_cpp_sdk        # propagates ws2_32/iphlpapi on Windows
)

add_library(tc::${ADDON_NAME} ALIAS ${ADDON_NAME})
