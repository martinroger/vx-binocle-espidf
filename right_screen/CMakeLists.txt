# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# include_directories(${CMAKE_CURRENT_LIST_DIR}/main/conf)
# Flags for everyone (C and C++)
add_compile_options(
    -Wno-missing-field-initializers
    -Wno-unused-variable
    -Wno-unused-function
    -Wno-extra
)

# Flags ONLY for C++
add_compile_options(
    $<$<COMPILE_LANGUAGE:CXX>:-Wno-deprecated-enum-enum-conversion>
    # $<$<COMPILE_LANGUAGE:CXX>:-Wno-volatile>
)

# Flags ONLY for C
# add_compile_options(
#     $<$<COMPILE_LANGUAGE:C>:-Wno-pointer-sign>
# )

project(right_screen)


