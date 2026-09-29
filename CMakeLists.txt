cmake_minimum_required(VERSION 3.16)
project(9xOS VERSION 0.1.0 LANGUAGES C
        DESCRIPTION "9xOS - Linux-based safety OS, release 0.1 'Bhargav'")

# Top level for all 9xOS components. Each component under components/ is also
# a standalone CMake project, so Buildroot (or a customer) can build one alone.

option(NINEXOS_BUILD_TESTS "Build tests for all components" ON)
set(GARUDA_BUILD_TESTS ${NINEXOS_BUILD_TESTS} CACHE BOOL "" FORCE)

if(NINEXOS_BUILD_TESTS)
  enable_testing()
endif()

add_subdirectory(components/garuda)
