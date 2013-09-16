#############################################################################
#
#  $Id$
#
#  Copyright 2012 Richard Hacker (lerichi at gmx dot net)
#
#  This file is part of the PdServ-MATLAB package.
#
#  PdServ-MATLAB is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  PdServ-MATLAB is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with PdServ-MATLAB. See COPYING. If not, see
#  <http://www.gnu.org/licenses/>.
#
#############################################################################

CONFIGURE_FILE("${CMAKE_CURRENT_SOURCE_DIR}/Contents.m.in"
    "${CMAKE_CURRENT_BINARY_DIR}/Contents.m")

CONFIGURE_FILE("${CMAKE_CURRENT_SOURCE_DIR}/etherlab_tools.mk.in"
    "${CMAKE_CURRENT_BINARY_DIR}/etherlab_tools.mk")

INSTALL (FILES
    "${CMAKE_CURRENT_BINARY_DIR}/Contents.m"
    etherlab.tlc
    etherlab_config.m
    etherlab_genfiles.tlc
    etherlab_hrt.tmf
    etherlab_postprocess.m
    "${CMAKE_CURRENT_BINARY_DIR}/etherlab_tools.mk"
    get_meta_tag.m
    postprocess.tlc
    postprocess_rtw.pl
    DESTINATION rtw/etherlab)
