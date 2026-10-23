add_library(pr-base64 STATIC
    base64.cpp
)

target_include_directories(pr-base64
    INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)
