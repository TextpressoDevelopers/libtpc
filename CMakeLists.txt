cmake_minimum_required(VERSION 3.7)
project(libtpc)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON) #...is required...
set(CMAKE_CXX_FLAGS "-DBOOST_NO_CXX11_SCOPED_ENUMS -w")

SET(CMAKE_INSTALL_PREFIX /usr/local)

set(SOURCE_FILES textpressocore.cpp textpressocore.h lucene-custom/CaseSensitiveAnalyzer.h
        lucene-custom/CaseSensitiveAnalyzer.cpp)
add_library(textpressocore SHARED ${SOURCE_FILES})
target_link_libraries(textpressocore lucene++)

add_executable(test_textpressocore ${SOURCE_FILES} tests/test_textpressocore.cpp)
target_link_libraries(test_textpressocore gtest lucene++ pthread boost_system)

install(TARGETS textpressocore RUNTIME DESTINATION bin LIBRARY DESTINATION lib)
install(FILES textpressocore.h DESTINATION include/textpresso)