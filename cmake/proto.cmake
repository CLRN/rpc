if(__PROTOBUF_INCLUDED)
  return()
endif()
set(__PROTOBUF_INCLUDED TRUE)

include(CMakeParseArguments)

if (WIN32)
    set(PROTO_FULL_PATH "${CMAKE_BINARY_DIR}/\;${Protobuf_INCLUDE_DIR}/")
else()
    set(PROTO_FULL_PATH "${CMAKE_BINARY_DIR}/:${Protobuf_INCLUDE_DIR}/")
endif()

# получить имя файла без расширения
function(get_filename_we _sourceFile _fileNameWE)
    # cmake built-in function get_filename_component returns extension from first occurrence of . in file name
    # this function computes the extension from last occurrence of . in file name
    string (FIND "${_sourceFile}" "." _index REVERSE)
    if (_index GREATER -1)
        string (SUBSTRING "${_sourceFile}" 0 ${_index} _sourceExt)
    else()
        set (_sourceExt "${_sourceFile}")
    endif()
    
    set (${_fileNameWE} "${_sourceExt}" PARENT_SCOPE)
endfunction()

function(protobuf_generate_cpp GENERATED_HDR GENERATED_SRC)
    set(options "")
    set(oneValueArgs INCLUDE_PATH TARGET FOLDER PROTO_INCLUDES CPP_OUT_FOLDER)
    set(multiValueArgs PROTOFILES)

    cmake_parse_arguments(PROTOBUF_FUNC "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    set(WORKING_FOLDER ${CMAKE_BINARY_DIR})
    if (NOT ${CMAKE_CFG_INTDIR} STREQUAL ".")
        set(WORKING_FOLDER "${WORKING_FOLDER}/${CMAKE_CFG_INTDIR}")
    endif()

    if (WIN32)
        set(ERROR_FORMAT "msvs")
    else()
        set(ERROR_FORMAT "gcc")
    endif()
    
    set(_outPythonDir "${CMAKE_BINARY_DIR}/py/")
    if (PYTHON_OUT_DIRECTORY)
        set(_outPythonDir ${PYTHON_OUT_DIRECTORY})
    endif()
    if(EXISTS ${_outPythonDir})
        set(PYTHON_OUTPUT "--python_out=${_outPythonDir}")
    endif()

    set(_outCppDir "${CMAKE_BINARY_DIR}")
    if (PROTOBUF_FUNC_CPP_OUT_FOLDER)
        set(_outCppDir ${PROTOBUF_FUNC_CPP_OUT_FOLDER})
    endif()

    if (WIN32)
        set(PROTO_INCLUDE_PATH "${PROTOBUF_FUNC_FOLDER}\;${PROTO_FULL_PATH}")
    else()
        set(PROTO_INCLUDE_PATH "${PROTOBUF_FUNC_FOLDER}:${PROTO_FULL_PATH}")
    endif()

    foreach(_protoFile ${PROTOBUF_FUNC_PROTOFILES})
        get_filename_component(_absoluteFilePath ${_protoFile} ABSOLUTE)
        get_filename_component(_protoFilePath ${_absoluteFilePath} PATH)
        get_filename_component(_protoNameTmp ${_absoluteFilePath} NAME)
        get_filename_we(${_protoNameTmp} _protoName)
                
        set(_generatedHdr "${CMAKE_BINARY_DIR}/${_protoName}.pb.h")
        set(_generatedSrc "${CMAKE_BINARY_DIR}/${_protoName}.pb.cc")
        list(APPEND _generatedHeaders ${_generatedHdr})
        list(APPEND _generatedSources ${_generatedSrc})    

        add_custom_target(copy_${_protoName})
        add_custom_command(TARGET copy_${_protoName} PRE_BUILD
                           COMMAND ${CMAKE_COMMAND} -E
                           copy ${_protoFile} ${CMAKE_BINARY_DIR})

        add_custom_command(
            PRE_BUILD
            OUTPUT ${_generatedSrc} ${_generatedHdr}
            COMMAND "${Protobuf_PROTOC_EXECUTABLE}"
            ARGS ${_protoFile} --cpp_out="${_outCppDir}/" "${PYTHON_OUTPUT}" --clrn_out="${_outCppDir}/" --proto_path="${PROTO_INCLUDE_PATH}" --error_format=${ERROR_FORMAT} --plugin=$<TARGET_FILE:protoc-gen-clrn>
            WORKING_DIRECTORY "${WORKING_FOLDER}"
            DEPENDS ${_protoFile} copy_${_protoName} protoc-gen-clrn
            COMMENT "Proto file: ${_protoName}, exec ${Protobuf_PROTOC_EXECUTABLE} in ${WORKING_FOLDER}: ${_protoFile} --cpp_out=${_outCppDir} ${PYTHON_OUTPUT} --clrn_out=${_outCppDir} --proto_path=${PROTO_INCLUDE_PATH} --error_format=${ERROR_FORMAT} --plugin=$<TARGET_FILE:protoc-gen-clrn>"
        )
    endforeach()
    
    set_source_files_properties(${_generatedHeaders} ${_generatedSources} PROPERTIES GENERATED TRUE)
    if(MSVC)
        # disable warning C4512: assignment operator could not be generated
        # disable warning C4244: '=' : conversion from 'google::protobuf::uint32' to 'google::protobuf::uint8', possible loss of data
        # disable warning C4125: decimal digit terminates octal escape sequence
        # disable warning C4996: Function call with parameters that may be unsafe
        # disable warning C4100: unreferenced formal parameter
        # disable warning C4127: conditional expression is constant
        # disable warning C4267: conversion from 'size_t' to 'int', possible loss of data
        # disable warning C4018: '<' : signed/unsigned mismatch
        set_source_files_properties(${_generatedSources} APPEND PROPERTY COMPILE_FLAGS " /wd4512 /wd4244 /wd4125 /wd4996 /wd4100 /wd4127 /wd4267 /wd4018" )
    endif()    
    
    set(${GENERATED_HDR} ${_generatedHeaders} PARENT_SCOPE)
    set(${GENERATED_SRC} ${_generatedSources} PARENT_SCOPE)
    
endfunction()