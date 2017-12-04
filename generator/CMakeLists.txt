set(PROJECT_NAME protoc-gen-clrn)

file(GLOB SOURCES "*.cpp")
file(GLOB HEADERS "*.h" "*.rc" "*.def")

find_package(Protobuf REQUIRED)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "tools")
target_link_libraries(${PROJECT_NAME}
    ${Boost_LIBRARIES}
	${Protobuf_LIBRARIES}
    ${Protobuf_PROTOC_LIBRARIES}
)
