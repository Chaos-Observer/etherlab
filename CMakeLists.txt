#############################################################################
#
#  $Id$
#
#  Copyright 2012-2013  Richard Hacker (lerichi at gmx dot net)
#                 2013  Florian Pose <fp@igh.de>
#
#  This is the main cmake file for EtherLab target for Real-Time Workshop(TM)
#  target of Mathworks.
#
#  The files are installed to:
#       ${CMAKE_INSTALL_PREFIX}/${DATAROOTDIR}/${TARGETDIR}
#
#  where the defaults are (depending on your platform):
#       CMAKE_INSTALL_PREFIX = /usr/local
#       DATAROOTDIR = share
#       TARGETDIR = etherlab
#
#  For detailed instructions, see README
#
#  This file is part of the EtherLab package.
#
#  EtherLab is free software: you can redistribute it and/or modify it under
#  the terms of the GNU General Public License as published by the Free
#  Software Foundation, either version 3 of the License, or (at your option)
#  any later version.
#
#  EtherLab is distributed in the hope that it will be useful, but WITHOUT ANY
#  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
#  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
#  details.
#
#  You should have received a copy of the GNU General Public License along
#  with EtherLab. See COPYING. If not, see <http://www.gnu.org/licenses/>.
#
#############################################################################

CMAKE_MINIMUM_REQUIRED (VERSION 2.6)
PROJECT (etherlab C)    # Need C for detecting CMAKE_INSTALL_LIBDIR
SET (CMAKE_ALLOW_LOOSE_LOOP_CONSTRUCTS TRUE)

#
# Release Instructions
#
# - Update version numbers below
# - Update ChangeLog (hg log --branch <branch> --style=changelog > ChangeLog)
# - Update NEWS file
# - commit
# - make dist
# - add tag release-x.x.x
#
SET (MAJOR_VERSION 2)
SET (MINOR_VERSION 2)
SET (PATCH_LEVEL 0)
SET (VERSION ${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_LEVEL})

INCLUDE (GNUInstallDirs)

IF (DEFINED PREFIX)
    MESSAGE (WARNING
        "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n"
        "PREFIX is deprecated and will be removed in future\n"
        "INSTALL PROCESS HAS CHANGED! See ChangeLog and README\n"
        "You can continue using PREFIX, but it would be better to update\n"
        "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n")

    SET (TARGETDIR "rtw")
    SET (ETHERLAB_DIR "${PREFIX}/rtw")
    SET (RPATH "-L$(ETHERLAB_DIR)/../$(PLATFORM_LIBDIR) -Wl,-rpath=$(ETHERLAB_DIR)/../$(PLATFORM_LIBDIR)")
    SET (OLDINCLUDES "-I$(ETHERLAB_DIR)/../include")
    SET (TARGET_ROOT "${PREFIX}")

ELSE ()

    IF (NOT DEFINED TARGETDIR)
        SET (TARGETDIR ${PROJECT_NAME})
    ENDIF()
    SET (ETHERLAB_DIR "${CMAKE_INSTALL_FULL_DATAROOTDIR}/${TARGETDIR}")
    SET (TARGET_ROOT "${CMAKE_INSTALL_FULL_DATAROOTDIR}")

ENDIF ()

# Make sure that the project options are remembered
SET (TARGETDIR "${TARGETDIR}" CACHE PATH
    "Subdirectory within ${TARGET_ROOT} as install root (default: ${PROJECT_NAME})" FORCE)

# Search for mercurial, used to add ID information to the library
LIST (APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/Modules/")
FIND_PACKAGE (Mercurial)
IF (MERCURIAL_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.hg")
MERCURIAL_HG_INFO (${PROJECT_SOURCE_DIR} Package)
ENDIF ()

IF (NOT CMAKE_VERSION VERSION_LESS "3.0")
    # Matlab requires this format
    STRING (TIMESTAMP DATE "%d-%m-%Y")
ENDIF()

INSTALL (DIRECTORY rtw/
    DESTINATION "${ETHERLAB_DIR}"
    USE_SOURCE_PERMISSIONS
    PATTERN "*.in" EXCLUDE)
INSTALL (DIRECTORY ${CMAKE_BINARY_DIR}/rtw/
    DESTINATION "${ETHERLAB_DIR}")
INSTALL (FILES ${CMAKE_SOURCE_DIR}/README
    DESTINATION "${ETHERLAB_DIR}")

CONFIGURE_FILE("etherlab.spec.in"
    "${CMAKE_CURRENT_BINARY_DIR}/etherlab.spec")
CONFIGURE_FILE("rtw/Contents.m.in"
    "${CMAKE_CURRENT_BINARY_DIR}/rtw/Contents.m")
CONFIGURE_FILE("rtw/etherlab_hrt.tmf.in"
    "${CMAKE_CURRENT_BINARY_DIR}/rtw/etherlab_hrt.tmf")

SET (CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Simulink Coder target using pdserv library")
SET (CPACK_PACKAGE_VENDOR "Richard Hacker")
SET (CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README")
SET (CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/COPYING")
SET (CPACK_PACKAGE_VERSION_MAJOR ${MAJOR_VERSION})
SET (CPACK_PACKAGE_VERSION_MINOR ${MINOR_VERSION})
SET (CPACK_PACKAGE_VERSION_PATCH ${PATCH_LEVEL})
SET (CPACK_PACKAGE_INSTALL_DIRECTORY
    "CMake ${CMake_VERSION_MAJOR}.${CMake_VERSION_MINOR}")
SET (CPACK_SOURCE_GENERATOR "TBZ2")
SET (CPACK_SOURCE_PACKAGE_FILE_NAME
      "${PROJECT_NAME}-${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")
SET (CPACK_GENERATOR "TGZ")
SET (CPACK_GENERATOR "DEB")
SET (CPACK_DEBIAN_PACKAGE_MAINTAINER "Richard Hacker") #required
LIST (APPEND CPACK_SOURCE_IGNORE_FILES
    "~$"
    "/build.*"
    "\\\\.sw.$"
    "\\\\.orig$"
    "/\\\\.hg"
    )

INCLUDE (CPack)
ADD_CUSTOM_TARGET (dist COMMAND ${CMAKE_MAKE_PROGRAM} package_source)

