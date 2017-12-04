cmake_minimum_required(VERSION 3.0)

set(PROJECT_NAME lib_rpc)
set(CMAKE_CXX_STANDARD 14)

add_subdirectory(net)
add_subdirectory(generator)

set(BOOST_COMPONENTS system iostreams filesystem thread)
find_package(Boost COMPONENTS ${BOOST_COMPONENTS} REQUIRED)
find_package(Protobuf REQUIRED)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

file(GLOB SOURCES "include/rpc/*.h" "src/*.h" "src/*.cpp")

add_library(${PROJECT_NAME} STATIC ${SOURCES})
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "common/rpc" LINKER_LANGUAGE CXX)

target_include_directories(${PROJECT_NAME}

                           PUBLIC
                           $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
                           ${CMAKE_BINARY_DIR}

                           PRIVATE
                           ${CMAKE_CURRENT_LIST_DIR}/src)

target_link_libraries(${PROJECT_NAME}
    lib_net

    ${Protobuf_LIBRARIES}
    ${Boost_LIBRARIES}
)

add_subdirectory(protocols)

if (WITH_TESTS)
    add_subdirectory(tests)
endif()
