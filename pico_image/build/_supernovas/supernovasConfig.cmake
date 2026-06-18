
####### Expanded from @PACKAGE_INIT@ by configure_package_config_file() #######
####### Any changes to this file will be overwritten by the next CMake run ####
####### The input file was supernovasConfig.cmake.in                            ########

get_filename_component(PACKAGE_PREFIX_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../" ABSOLUTE)

macro(set_and_check _var _file)
  set(${_var} "${_file}")
  if(NOT EXISTS "${_file}")
    message(FATAL_ERROR "File or directory ${_file} referenced by variable ${_var} does not exist !")
  endif()
endmacro()

macro(check_required_components _NAME)
  foreach(comp ${${_NAME}_FIND_COMPONENTS})
    if(NOT ${_NAME}_${comp}_FOUND)
      if(${_NAME}_FIND_REQUIRED_${comp})
        set(${_NAME}_FOUND FALSE)
      endif()
    endif()
  endforeach()
endmacro()

####################################################################################

# supernovas CMake configuration file

include(CMakeFindDependencyMacro)

# Find math library if needed
if(NOT WIN32)
    find_library(MATH_LIBRARY m)
    if(NOT MATH_LIBRARY)
        message(FATAL_ERROR "Math library not found")
    endif()
endif()

# Include targets
include("${CMAKE_CURRENT_LIST_DIR}/supernovasTargets.cmake")

# Get include directories from target properties
get_target_property(supernovas_INCLUDE_DIRS supernovas::core INTERFACE_INCLUDE_DIRECTORIES)

# Check if optional components are available
if(TARGET supernovas::cpp)
    set(supernovas_cpp_FOUND TRUE)
else()
    set(supernovas_cpp_FOUND FALSE)
endif()

if(TARGET supernovas::solsys-calceph)
    find_library(CALCEPH_LIB calceph)
    find_dependency(Threads)
    set(supernovas_solsys-calceph_FOUND TRUE)
else()
    set(supernovas_solsys-calceph_FOUND FALSE)
endif()

if(TARGET supernovas::solsys-cspice)
    find_library(CSPICE_LIB cspice)
    find_dependency(Threads)
    set(supernovas_solsys-cspice_FOUND TRUE)
else()
    set(supernovas_solsys-cspice_FOUND FALSE)
endif()

check_required_components(supernovas)


