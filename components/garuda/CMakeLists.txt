cmake_minimum_required(VERSION 3.16)
project(garuda VERSION 0.1.0 LANGUAGES C
        DESCRIPTION "9xOS Garuda liveness monitor")

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(NinexosHardening)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(GARUDA_BUILD_TESTS "Build Garuda unit and integration tests" ${PROJECT_IS_TOP_LEVEL})
option(GARUDA_BUILD_EXAMPLES "Build the Garuda demo client" ON)

# --- Client library (what applications link against) ---------------------
add_library(garuda_client src/client.c)
add_library(9xos::garuda_client ALIAS garuda_client)
target_include_directories(garuda_client PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
set_target_properties(garuda_client PROPERTIES
  VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})
ninexos_harden(garuda_client)

# --- Monitor core (pure logic, unit-tested) -------------------------------
add_library(garuda_monitor STATIC src/monitor.c)
target_include_directories(garuda_monitor PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/include)
ninexos_harden(garuda_monitor)

# --- Daemon ---------------------------------------------------------------
add_executable(garudad src/garudad.c)
target_link_libraries(garudad PRIVATE garuda_monitor)
ninexos_harden(garudad)

install(TARGETS garudad RUNTIME DESTINATION ${CMAKE_INSTALL_SBINDIR})
install(TARGETS garuda_client EXPORT GarudaTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/garuda DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# --- Package files: find_package(Garuda) and pkg-config -------------------
install(EXPORT GarudaTargets NAMESPACE 9xos::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Garuda)
configure_package_config_file(cmake/GarudaConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/GarudaConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Garuda)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/GarudaConfigVersion.cmake
  COMPATIBILITY SameMinorVersion)
install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/GarudaConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/GarudaConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Garuda)
configure_file(cmake/garuda.pc.in ${CMAKE_CURRENT_BINARY_DIR}/garuda.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/garuda.pc
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

# --- Examples and tests ---------------------------------------------------
if(GARUDA_BUILD_EXAMPLES)
  add_executable(garuda_demo examples/garuda_demo.c)
  target_link_libraries(garuda_demo PRIVATE garuda_client)
  ninexos_harden(garuda_demo)
  install(TARGETS garuda_demo RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

if(GARUDA_BUILD_TESTS)
  enable_testing()
  add_executable(test_garuda_monitor tests/test_monitor.c)
  target_link_libraries(test_garuda_monitor PRIVATE garuda_monitor)
  ninexos_harden(test_garuda_monitor)
  add_test(NAME garuda.monitor.unit COMMAND test_garuda_monitor)

  if(GARUDA_BUILD_EXAMPLES AND NOT CMAKE_CROSSCOMPILING)
    add_test(NAME garuda.heartbeat.integration
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/tests/it_heartbeat.sh
              $<TARGET_FILE:garudad> $<TARGET_FILE:garuda_demo>)
    set_tests_properties(garuda.heartbeat.integration PROPERTIES TIMEOUT 30)
  endif()
endif()
