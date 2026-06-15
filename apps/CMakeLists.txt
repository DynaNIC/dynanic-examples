cmake_minimum_required(VERSION 3.3.2)
project(dynanic-examples VERSION 1.0.0 LANGUAGES C)

# DPDK #########################################################################

find_package(PkgConfig REQUIRED)
pkg_check_modules(DPDK REQUIRED libdpdk)

add_definitions(${DPDK_STATIC_CFLAGS})
include_directories(${DPDK_INCLUDE_DIR} common)
link_libraries(${DPDK_LIBRARIES})
add_definitions(-DHAVE_DPDK)

# OTHER SETTINGS ###############################################################

#add_compile_options(-Wall -Wextra -Wpedantic)

# EXAMPLES #####################################################################

add_subdirectory(flow)
add_subdirectory(generate)
add_subdirectory(loopback)
add_subdirectory(metadata_viewer)
add_subdirectory(minimal_generate)
add_subdirectory(minimal_read)
add_subdirectory(read)
add_subdirectory(receive)
add_subdirectory(table_filler)
add_subdirectory(transmit)
add_subdirectory(bonding)
