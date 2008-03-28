<?xml version="1.0"?>
<!--
 * $Id$
 *
 * This file is used by an XSLT processor to generate the model description
 * file (mdf) as C-code. The mdf is used later when the model gets registered
 * with the rt_kernel.
 * 
 * Copyright (C) 2008  Richard Hacker
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
-->

<xsl:stylesheet version="1.0"
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

  <xsl:output method="text" encoding="iso-8859-15" omit-xml-declaration="yes"/>
  
  <xsl:strip-space elements="*"/>

  <xsl:template match="text()|@*"/>
  
  <xsl:template match="/">
    <xsl:text><![CDATA[
/************************************************************************
 * This is a generated file. Do not edit.
 *
 * This file contains a list of signal and parameter descriptions that
 * is used when registering the model with rt_kernel.
 *
 ************************************************************************/

#include "include/etl_mdf.h"

#include "cvt.h"
#include "param.h"

#include <stddef.h>         /* For NULL */

]]>
    </xsl:text>
    <xsl:apply-templates select="modeldescription/data"/>
  </xsl:template>
  
  <xsl:template match="data">
    <xsl:text>struct signal_info capi_signals[] = {
    </xsl:text>
    <xsl:apply-templates select="//signal"/>
    <xsl:text>{.address = NULL, }   /* NULL terminated list */
    };

    </xsl:text>
    <xsl:text>struct signal_info capi_parameters[] = {
    </xsl:text>
    <xsl:apply-templates select="//parameter"/>
    <xsl:text>{.address = NULL, }   /* NULL terminated list */
    };

    </xsl:text>
  </xsl:template>
  
  <xsl:template match="signal">
  <xsl:call-template name="signal_attributes">
    <xsl:with-param name="structure">S</xsl:with-param>
    <xsl:with-param name="stidx" select="@sampleTimeIndex"/>
    <xsl:with-param name="name" select="@alias"/>
    <xsl:with-param name="path">
      <xsl:for-each select="ancestor::subsystem">
      	<xsl:text>/</xsl:text>
      	<xsl:value-of select="@name"/>
      </xsl:for-each>
      <xsl:text>/</xsl:text>
      <xsl:value-of select="@name"/>
    </xsl:with-param>
  </xsl:call-template>
  </xsl:template>

  <xsl:template match="parameter">
  <xsl:call-template name="signal_attributes">
    <xsl:with-param name="structure">P</xsl:with-param>
    <xsl:with-param name="name" select="@name"/>
    <xsl:with-param name="path">
      <xsl:for-each select="ancestor::subsystem">
      	<xsl:text>/</xsl:text>
      	<xsl:value-of select="@name"/>
      </xsl:for-each>
    </xsl:with-param>
  </xsl:call-template>
  </xsl:template>

  <xsl:template name="signal_attributes">
    <!-- Parameters for this template -->
    <xsl:param name="stidx"/>    <!-- Sample time index -->
    <xsl:param name="structure"/><!-- Set to P for parameter, S for signal -->
    <xsl:param name="name"/>     <!-- Name of signal -->
    <xsl:param name="path"/>     <!-- Signal path -->


    <xsl:text>{ </xsl:text> 

    <!-- Address of signal -->
    .address = <![CDATA[&]]><xsl:value-of select="$structure"/>(
    <xsl:value-of select="@cvar"/>
    <xsl:text>), </xsl:text> 

    <!-- Signal data type -->
    .data_type = si_<xsl:value-of select="@datatype"/>
    <xsl:text>, </xsl:text> 

    <!-- Signal data type -->
    <xsl:if test="$stidx">
    .st_index = <xsl:value-of select="$stidx"/>
    <xsl:text>, </xsl:text> 
    </xsl:if>

    <!-- Orientation and row and column counts -->
    <xsl:choose>
      <xsl:when test="@orientation='vector'">
        .orientation = si_vector,
        .rnum = <xsl:value-of select="@elements"/>,
        .cnum = 1,
      </xsl:when>
      <xsl:when test="@orientation='matrix_row_major'">
        .orientation = si_matrix_row_major,
        .rnum = <xsl:value-of select="@rnum"/>,
        .cnum = <xsl:value-of select="@cnum"/>,
      </xsl:when>
      <xsl:when test="@orientation='matrix_col_major'">
        .orientation = si_matrix_col_major,
        .rnum = <xsl:value-of select="@rnum"/>,
        .cnum = <xsl:value-of select="@cnum"/>,
      </xsl:when>
      <xsl:otherwise>
        .orientation = si_scalar,
        .rnum = 1, 
        .cnum = 1,
      </xsl:otherwise>
    </xsl:choose>
    
    <xsl:text>.name = "</xsl:text>
    <xsl:value-of select="$name"/>
    <xsl:text>",
    </xsl:text>

    <!-- Now write the full path as a comment -->
    <xsl:text>.path = "</xsl:text>
    <xsl:value-of select="$path"/>
    <xsl:text>", 
    },
    </xsl:text>
     
  </xsl:template>
  
</xsl:stylesheet>
