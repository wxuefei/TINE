add_library(dyad
  STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/dyad.c
)

add_library(argtable3
  STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/argtable3.c
)

set_target_properties(dyad argtable3
  PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO
    C_VISIBILITY_PRESET hidden
    INTERPROCEDURAL_OPTIMIZATION ON
    LINKER_LANGUAGE C
)

# yuck
add_library(c_opts INTERFACE)
target_compile_options(c_opts
  INTERFACE
    -Wall
    -Wextra
    -Wpedantic
    -Werror=shadow
    -fno-exceptions
    -fno-unwind-tables
    -fno-asynchronous-unwind-tables
    -fno-stack-protector
    -mno-shstk
    -fcf-protection=none
)
target_link_libraries(dyad PRIVATE c_opts)
target_link_libraries(argtable3 PRIVATE c_opts)

target_link_libraries(${PROJECT_NAME} PRIVATE dyad argtable3)
target_include_directories(${PROJECT_NAME}
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

add_subdirectory(linenoise-ng)

# vim: set expandtab ts=2 sw=2 :
