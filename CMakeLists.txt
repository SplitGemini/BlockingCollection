cmake_minimum_required(VERSION 3.1)

project(blocking_collection VERSION 0.0.1 LANGUAGES CXX)

set(BLOCKING_COLLECTION_MAIN_PROJECT OFF)
if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(BLOCKING_COLLECTION_MAIN_PROJECT ON)
endif()

add_library(blocking_collection INTERFACE)
add_library(code_machina::blocking_collection ALIAS blocking_collection)
target_include_directories(blocking_collection 
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

if(BLOCKING_COLLECTION_MAIN_PROJECT)
    add_subdirectory(tests)
endif()
