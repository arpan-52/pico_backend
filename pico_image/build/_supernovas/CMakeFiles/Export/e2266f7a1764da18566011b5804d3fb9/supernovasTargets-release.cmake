#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "supernovas::core" for configuration "Release"
set_property(TARGET supernovas::core APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(supernovas::core PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "C"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libsupernovas.a"
  )

list(APPEND _cmake_import_check_targets supernovas::core )
list(APPEND _cmake_import_check_files_for_supernovas::core "${_IMPORT_PREFIX}/lib/libsupernovas.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
