cmake_minimum_required(VERSION 3.7)
project(libtpc)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES reader.cpp reader.h lucene-custom/CaseSensitiveAnalyzer.h
        lucene-custom/CaseSensitiveAnalyzer.cpp)
add_library(libtpc SHARED ${SOURCE_FILES})
target_link_libraries(libtpc lucene++)

add_executable(test_libtpc ${SOURCE_FILES} tests/reader_tests.cpp)
target_link_libraries(test_libtpc gtest lucene++ pthread boost_system)