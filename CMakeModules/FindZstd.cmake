# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

#
# - Try to find Facebook zstd library
# This will define
# Zstd_FOUND
# Zstd_INCLUDE_DIR
# Zstd_LIBRARY
#

find_package(PkgConfig QUIET)
pkg_check_modules(PC_ZSTD QUIET libzstd)

find_path(Zstd_INCLUDE_DIR NAMES zstd.h
  HINTS ${PC_ZSTD_INCLUDEDIR} ${PC_ZSTD_INCLUDE_DIRS})

find_library(Zstd_LIBRARY_DEBUG NAMES zstdd zstd_staticd
  HINTS ${PC_ZSTD_LIBDIR} ${PC_ZSTD_LIBRARY_DIRS})
find_library(Zstd_LIBRARY_RELEASE NAMES zstd zstd_static
  HINTS ${PC_ZSTD_LIBDIR} ${PC_ZSTD_LIBRARY_DIRS})

include(SelectLibraryConfigurations)
SELECT_LIBRARY_CONFIGURATIONS(Zstd)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(
    Zstd DEFAULT_MSG
    Zstd_LIBRARY Zstd_INCLUDE_DIR
)

mark_as_advanced(Zstd_INCLUDE_DIR Zstd_LIBRARY)

if(Zstd_FOUND AND NOT (TARGET Zstd::Zstd))
  add_library (Zstd::Zstd UNKNOWN IMPORTED)
  set_target_properties(Zstd::Zstd
    PROPERTIES
    IMPORTED_LOCATION ${Zstd_LIBRARY}
    INTERFACE_INCLUDE_DIRECTORIES ${Zstd_INCLUDE_DIR})
endif()
