find_package(ZLIB REQUIRED)

add_library(pr-minizip
    unzip.c
    zip.c
    ioapi.c)
target_link_libraries(pr-minizip PUBLIC ZLIB::ZLIB)
set_target_properties(pr-minizip PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_definitions(pr-minizip PRIVATE -DNOCRYPT -DNOUNCRYPT)
if(WIN32)
    target_compile_definitions(pr-minizip PRIVATE -DUNICODE -D_UNICODE)
endif()
target_include_directories(pr-minizip
  INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
)
