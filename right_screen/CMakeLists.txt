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

# Override flash image to target ota_0
partition_table_get_partition_info(ota_0_offset "--partition-name ota_0" "offset")
message(STATUS "Setting app flash target to ota_0 offset: ${ota_0_offset}")

# Correct target properties for esptool_py
set_property(TARGET app-flash PROPERTY IMAGES "${ota_0_offset} ${PROJECT_BIN}")
set_property(TARGET flash PROPERTY FLASH_FILE
    "\"0x0\" : \"bootloader/bootloader.bin\""
    "\"${ota_0_offset}\" : \"${PROJECT_BIN}\""
    "\"0x8000\" : \"partition_table/partition-table.bin\""
    "\"0xd000\" : \"ota_data_initial.bin\""
)
set_property(TARGET flash PROPERTY FLASH_ENTRY
    "\"bootloader\" : { \"offset\" : \"0x0\", \"file\" : \"bootloader/bootloader.bin\", \"encrypted\" : \"false\" }"
    "\"app\" : { \"offset\" : \"${ota_0_offset}\", \"file\" : \"${PROJECT_BIN}\", \"encrypted\" : \"false\" }"
    "\"partition-table\" : { \"offset\" : \"0x8000\", \"file\" : \"partition_table/partition-table.bin\", \"encrypted\" : \"false\" }"
    "\"otadata\" : { \"offset\" : \"0xd000\", \"file\" : \"ota_data_initial.bin\", \"encrypted\" : \"false\" }"
)
set_property(TARGET flash PROPERTY IMAGES
    "0x0 bootloader/bootloader.bin"
    "${ota_0_offset} ${PROJECT_BIN}"
    "0x8000 partition_table/partition-table.bin"
    "0xd000 ota_data_initial.bin"
)
