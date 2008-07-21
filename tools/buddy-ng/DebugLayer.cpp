/* **********************************************************************
 *  $Id$
 * **********************************************************************/

/*
 * @note Copyright (C) 2008 Richard Hacker
 * <lerichi@gmx.net>
 *
 *  License: GPL
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include <iostream>
#include "DebugLayer.h"

////////////////////////////////////////////////////////////////////////
DebugLayer::DebugLayer(Layer* below):
    Layer(below,"DebugLayer\n",0)
{
}

////////////////////////////////////////////////////////////////////////
DebugLayer::~DebugLayer()
{
}

////////////////////////////////////////////////////////////////////////
// Method from class Layer
size_t DebugLayer::receive(const char* buf, size_t data_len)
{
    unsigned int i = 0;

    for (i = 0; i < data_len; i += 16) {
        printf(i ? "\n%04u" : "%04X", i);
        for (unsigned int j = 0; j < 16; j++) {
            if (i + j < data_len)
                printf(" %02X", (unsigned char)buf[i+j]);
            else
                printf("   ");
        }
        printf("    ");
        for (unsigned int j = 0; j < 16; j++) {
            if (i + j < data_len)
                printf("%c", isprint(buf[i+j]) ? buf[i+j] : '.');
            else
                printf(" ");
        }
    }

    return post(buf,data_len);
}
