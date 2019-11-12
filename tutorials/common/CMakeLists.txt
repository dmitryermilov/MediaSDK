include_directories (${CMAKE_CURRENT_SOURCE_DIR}/../common)
set( sources
    ${CMAKE_CURRENT_SOURCE_DIR}/cmd_options.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/common_utils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/common_utils_linux.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/common_vaapi.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/spl/bitstream_reader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/spl/avc_bitstream.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/spl/avc_nal_spl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/spl/avc_spl.cpp
    )

make_library( tutorials_common none static )
