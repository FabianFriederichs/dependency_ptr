cmake_minimum_required(VERSION 3.14)
project(dependency_ptr VERSION 0.1.0 LANGUAGES CXX)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(header_only_library)
include(package_helpers)

set(dependency_ptr_sources "${CMAKE_CURRENT_SOURCE_DIR}/include/dependency_ptr.hpp")

add_header_only_library(
dependency_ptr                                          # target name
    SOURCES
        "${dependency_ptr_sources}"  # sources
    CXX_STD 17                                          # interface cpp standard              
)

target_include_directories(dependency_ptr
    INTERFACE
        $<INSTALL_INTERFACE:"include">    
        $<BUILD_INTERFACE:"${CMAKE_CURRENT_SOURCE_DIR}/include">
)

# creates package files and does all the install stuff
make_package(NAME dependency_ptr
    HEADER_ONLY
    TARGETS
        dependency_ptr
    PUBLIC_INCLUDE_DIRECTORIES
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
    RUNTIME_DESTINATION bin
    LIBRARY_DESTINATION lib
    ARCHIVE_DESTINATION lib
    INCLUDE_DESTINATION .
    CONFIG_DESTINATION lib/cmake
    VERSION_MAJOR ${PROJECT_VERSION_MAJOR}
    VERSION_MINOR ${PROJECT_VERSION_MINOR}
    VERSION_PATCH ${PROJECT_VERSION_PATCH}
    COMPATIBILITY SameMajorVersion
)