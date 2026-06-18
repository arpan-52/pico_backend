#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "finufft::finufft" for configuration "Release"
set_property(TARGET finufft::finufft APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(finufft::finufft PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libfinufft.a"
  )

list(APPEND _cmake_import_check_targets finufft::finufft )
list(APPEND _cmake_import_check_files_for_finufft::finufft "${_IMPORT_PREFIX}/lib/libfinufft.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
