cmake_minimum_required(VERSION 2.8)
project(myhttpd)
include_directories(includes)
aux_source_directory(myhttpd SERVER)
aux_source_directory(ipcd IPCD)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
add_executable(server ${SERVER})
add_executable(daemon ${IPCD})
if (NOT APPLE)
    set(CMAKE_C_FLAGS ${CMAKE_C_FLAGS} "-pthread")
    TARGET_LINK_LIBRARIES(server ${Boost_LIBRARIES} rt)
    TARGET_LINK_LIBRARIES(daemon ${Boost_LIBRARIES} rt)
endif(NOT APPLE)
