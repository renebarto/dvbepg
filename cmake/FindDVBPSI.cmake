#
# Find the Google test includes and library
#
# This module defines
# DVBPSI_INCLUDE_DIR, where to find gtest.h, etc.
# DVBPSI_LIBRARIES, the libraries to link against to use Google test.
# DVBPSI_FOUND, If false, do not try to use Google test.

# also defined, but not for general use are
# DVBPSI_LIBRARY, where to find the Google test library.
# DVBPSI_LIBRARY_MAIN, where to find the Google test library for the main() function.

MESSAGE(STATUS "Looking for DVBPSI")

find_path(DVBPSI_INCLUDE_DIR 
          NAMES dvbpsi/dvbpsi.h
          HINTS ${DVBPSI_ROOT}/include /usr/include /usr/local/include /usr/include/x86_64-linux-gnu)

set(DVBPSI_NAMES dvbpsi)
find_library(DVBPSI_LIBRARY 
             NAMES ${DVBPSI_NAMES}
             HINTS ${DVBPSI_ROOT}/lib /usr/lib /usr/local/lib /usr/lib/)

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set DVBPSI_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(DVBPSI "Please install libdvbpsi:\ngit clone http://git.videolan.org/git/libdvbpsi.git\n./bootstrap;./configure;\nmake;sudo make install\n" DVBPSI_INCLUDE_DIR DVBPSI_LIBRARY)

if(NOT DVBPSI_FOUND)
    MESSAGE(FATAL_ERROR  "Could not find library libdvbpsi.")
else()
    set(DVBPSI_LIBRARIES ${DVBPSI_LIBRARY})
endif()
