add_library(pr-jsoncpp
    src/lib_json/json_value.cpp
    src/lib_json/json_reader.cpp
    src/lib_json/json_writer.cpp)
target_include_directories(pr-jsoncpp PUBLIC include)
set_target_properties(pr-jsoncpp PROPERTIES POSITION_INDEPENDENT_CODE ON)
