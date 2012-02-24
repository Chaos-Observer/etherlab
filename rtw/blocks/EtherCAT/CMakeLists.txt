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



INSTALL (FILES
    Beckhoff.png 
    Baumer.png 
    ESD.png 
    mts-logo.png 
    Murr.png 
    EtherCAT.tlc 
    getEtherCATAddress.m 
    strjoin.m 
    ec_slave3.c ec_slave3.tlc 
    setup.m
    el1xxx.m 

    master_state.c master_state.tlc 
    domain_state.c domain_state.tlc 
    moog_msd.c moog_msd_callback.m moog_msd.tlc moog.png 
    ethercat_ss_funcs.h
    el31xx_check.m 
    el320x_check.m 
    el5001_check.m 
    el5101_check.m 
    beckhoff_el30xx.m 
    beckhoff_el320x.m 
    beckhoff_el515x.m 
    beckhoff_el73x2.m 
    beckhoff_ep3xxx.m 
    beckhoff_ep4xxx.m 
    beckhoff_ep4374.m 
    baumer_ecatencoder.m 
    esd_cangateway.m 
    murr_impact67.m 
    mtsrseries_check.m 
    ethercatinfo_check.m 
    SoeConfig.m 

    el1xxx.m  el2xxx_check.m  el2xxx.m  el31xx.m
    el4xxx.m ep3xxx.m ep1xxx.m ep4374.m ep2xxx.m
    el5001_check.m el320x_check.m  el31xx_check.m  el5101_check.m
    ethercatinfo_check.m mtsrseries_check.m infrastructure.m

    DESTINATION rtw/blocks/EtherCAT)

INSTALL (DIRECTORY @EtherCATInfo
    DESTINATION rtw/blocks
    FILES_MATCHING PATTERN "*.m")

ADD_SUBDIRECTORY (xml)
