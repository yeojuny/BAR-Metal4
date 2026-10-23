find_package(Boost 1.35.0 REQUIRED COMPONENTS unit_test_framework)

add_executable(prd_test test.cpp)
target_link_libraries(prd_test
    pr-downloader
    Boost::unit_test_framework
)
add_test(NAME prdtest COMMAND prd_test)
