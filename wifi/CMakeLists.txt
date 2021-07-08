ADD_DEFINITIONS(-Wall -Werror -Wextra -Wno-unused-parameter)

file(GLOB wifi_srcs
    "${CMAKE_CURRENT_SOURCE_DIR}/libnl-tiny/*.c"
)

include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}
)

LINK_LIBRARIES(
)

add_library(wifi 
    ${wifi_srcs}
)

target_include_directories(wifi
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/libnl-tiny/include
)
