#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "ISAL::isal" for configuration "Release"
set_property(TARGET ISAL::isal APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(ISAL::isal PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libisal.so.2.31.0"
  IMPORTED_SONAME_RELEASE "libisal.so.2"
  )

list(APPEND _cmake_import_check_targets ISAL::isal )
list(APPEND _cmake_import_check_files_for_ISAL::isal "${_IMPORT_PREFIX}/lib/libisal.so.2.31.0" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
