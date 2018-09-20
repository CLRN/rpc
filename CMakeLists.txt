cmake_minimum_required(VERSION 3.0)

set(PROJECT_NAME lib_rpc)
set(CMAKE_CXX_STANDARD 14)

add_subdirectory(net)
add_subdirectory(generator)

set(BOOST_COMPONENTS system iostreams filesystem thread)
find_package(Boost COMPONENTS ${BOOST_COMPONENTS} REQUIRED)
find_package(Protobuf REQUIRED)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

file(GLOB INCLUDES "include/rpc/*.h" "src/Dummy.cpp")
add_library(rpc_includes STATIC ${INCLUDES})
set_target_properties(rpc_includes PROPERTIES FOLDER "common/rpc" LINKER_LANGUAGE CXX)

target_include_directories(rpc_includes

        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
        ${CMAKE_BINARY_DIR}
        ${PROTOBUF_INCLUDE_DIR})

file(GLOB SOURCES "src/*")

add_library(${PROJECT_NAME} STATIC ${INCLUDES} ${SOURCES})
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "common/rpc")

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)

target_link_libraries(${PROJECT_NAME}
    lib_net
    lib_cmn_proto
    rpc_includes

    ${Protobuf_LIBRARIES}
    ${Boost_LIBRARIES}
)

if (NOT APPLE AND UNIX)
    target_link_libraries(${PROJECT_NAME} rt)
endif()

set(RPC_ROOT ${CMAKE_CURRENT_LIST_DIR})
add_subdirectory(protocols)

if (WITH_TESTS)
    add_subdirectory(tests)
endif()
