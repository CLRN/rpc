set(PROJECT_NAME lib_test_proto)
file(GLOB_RECURSE PROTO_SOURCES "*.proto")

include(proto)
protobuf_generate_cpp(
    GENERATED_PROTO_HEADERS
    GENERATED_PROTO_SOURCES
    PROTOFILES ${PROTO_SOURCES}
    FOLDER ${CMAKE_CURRENT_LIST_DIR}
)

add_library(${PROJECT_NAME} STATIC ${PROTO_SOURCES} ${GENERATED_PROTO_HEADERS} ${GENERATED_PROTO_SOURCES})
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "common/rpc")

target_link_libraries(${PROJECT_NAME}
    lib_cmn_proto
)

add_dependencies(${PROJECT_NAME}
    protoc-gen-clrn
)
