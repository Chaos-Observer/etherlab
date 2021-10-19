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
# - run build/release.sh
# - add tag release-x.x.x
#
SET (MAJOR_VERSION 2)
SET (MINOR_VERSION 3)
SET (PATCH_LEVEL 3)
SET (VERSION ${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_LEVEL})

INCLUDE (GNUInstallDirs)

# In this section, define
#    - ETHERLAB_DIR:    Directory where all the files are copied
#    - INSTALL_PATH:    path relative to ETHERLAB_DIR in TMF where include/
#                       and src/ are located
IF (NOT DEFINED TARGETDIR)
    SET (TARGETDIR ${PROJECT_NAME})
ENDIF()

SET (INSTALL_PATH "${CMAKE_INSTALL_DATADIR}/${TARGETDIR}")
SET (ETHERLAB_DIR "${CMAKE_INSTALL_FULL_DATADIR}/${TARGETDIR}")

# Make sure that the project options are remembered
SET (TARGETDIR "${TARGETDIR}" CACHE PATH
    "Subdirectory within ${TARGET_ROOT} as install root (default: ${PROJECT_NAME})"
    FORCE)

# Search for mercurial, used to add ID information to the library
LIST (APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/Modules/")
FIND_PACKAGE (Mercurial)
IF (MERCURIAL_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.hg")
MERCURIAL_HG_INFO (${PROJECT_SOURCE_DIR} Package)
ENDIF ()

IF (NOT CMAKE_VERSION VERSION_LESS "3.0")
    # Matlab requires this format
    STRING (TIMESTAMP DATE "%d-%b-%Y")
ENDIF()

INSTALL (DIRECTORY rtw/
    DESTINATION "${ETHERLAB_DIR}"
    USE_SOURCE_PERMISSIONS
    PATTERN "*~" EXCLUDE
    PATTERN "*.sw?" EXCLUDE
    PATTERN "*.orig" EXCLUDE
    PATTERN "*.in" EXCLUDE)
INSTALL (DIRECTORY ${CMAKE_BINARY_DIR}/rtw/
    DESTINATION "${ETHERLAB_DIR}")

CONFIGURE_FILE("rtw/Contents.m.in"
    "${CMAKE_CURRENT_BINARY_DIR}/rtw/Contents.m")
CONFIGURE_FILE("rtw/etherlab_hrt.tmf.in"
    "${CMAKE_CURRENT_BINARY_DIR}/rtw/etherlab_hrt.tmf")

# Custom release make target
IF (EXISTS "${PROJECT_SOURCE_DIR}/scripts")
    CONFIGURE_FILE (
        "${PROJECT_SOURCE_DIR}/scripts/release.sh.in"
        "${PROJECT_BINARY_DIR}/release.sh"
        )
    ADD_CUSTOM_TARGET (dist COMMAND sh ./release.sh)
ENDIF ()
