# Copyright (C) The libssh2 project and its contributors.
# SPDX-License-Identifier: BSD-3-Clause

include(CMakeFindDependencyMacro)
set(_libssh2_cmake_module_path_save "${CMAKE_MODULE_PATH}")
list(PREPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR})

if("OpenSSL" STREQUAL "OpenSSL")
  find_dependency(OpenSSL)
elseif("OpenSSL" STREQUAL "wolfSSL")
  find_dependency(WolfSSL)
elseif("OpenSSL" STREQUAL "Libgcrypt")
  find_dependency(Libgcrypt)
elseif("OpenSSL" STREQUAL "mbedTLS")
  find_dependency(MbedTLS)
endif()
set(CMAKE_MODULE_PATH ${_libssh2_cmake_module_path_save})

if(ON)
  find_dependency(ZLIB)
endif()

include("${CMAKE_CURRENT_LIST_DIR}/libssh2-targets.cmake")

# Alias for either shared or static library
if(NOT TARGET libssh2::libssh2)
 if(CMAKE_VERSION VERSION_LESS "3.18.0")
  # cannot add alias to non-global imported library
  add_library(libssh2::libssh2 INTERFACE IMPORTED)
  set_target_properties(libssh2::libssh2 PROPERTIES INTERFACE_LINK_LIBRARIES libssh2::libssh2_shared)
 else()
  add_library(libssh2::libssh2 ALIAS libssh2::libssh2_shared)
 endif()
endif()

# Compatibility alias
if(NOT TARGET Libssh2::libssh2)
 if(CMAKE_VERSION VERSION_LESS "3.18.0")
  # cannot add alias to non-global imported library
  add_library(Libssh2::libssh2 INTERFACE IMPORTED)
  set_target_properties(Libssh2::libssh2 PROPERTIES INTERFACE_LINK_LIBRARIES libssh2::libssh2_shared)
 else()
  add_library(Libssh2::libssh2 ALIAS libssh2::libssh2_shared)
 endif()
endif()
