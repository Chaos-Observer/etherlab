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
    mts-logo.png
    Murr.png
    EtherCAT.tlc
    getEtherCATAddress.m
    ec_slave3.c ec_slave3.tlc
    setup.m

    master_state.c master_state.tlc
    domain_state.c domain_state.tlc
    baumer_ecatencoder.m
    mts_temposonics.m

    murr_dio.m
    update_library.m update_devices.m
    slave_dc.m update_mask_state.m
    EtherCATSlaveBlock.m

    ekxxxx.m
    el1xxx.m
    el2xxx.m
    el30xx.m el31xx.m el320x.m el335x.m
    el4xxx.m
    el500x.m el5021.m el5101_0010.m el5101.m el515x.m
    el3356.m

    ep1xxx.m
    ep2xxx.m
    ep31xx.m
    ep41xx.m
    ep43xx.m

    mtsrseries_check.m

    DESTINATION rtw/blocks/EtherCAT)
