
project(blocking_collection_tests VERSION 0.0.1 LANGUAGES CXX)
find_package(Threads REQUIRED)
add_executable(blocking_collection_test unit_tests.cpp)

target_link_libraries(blocking_collection_test 
    PRIVATE 
        blocking_collection
        Threads::Threads
)
set_target_properties(blocking_collection_test
    PROPERTIES
        CXX_STANDARD 17
)
