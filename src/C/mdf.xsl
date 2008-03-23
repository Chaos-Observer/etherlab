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

#include "etl_mdf.h"

#include "cvt.h"
#include "param.h"

#include <stddef.h>         /* For NULL */

    ]]>
    </xsl:text>
    <xsl:apply-templates select="modeldescription/data"/>
  </xsl:template>
  
  <xsl:template match="data">
    <xsl:text>struct capi_signals capi_signals[] = {
    </xsl:text>
    <xsl:apply-templates select="//signal"/>
    <xsl:text>{NULL, }   /* NULL terminated list */
    };

    </xsl:text>
    <xsl:text>struct capi_parameters capi_parameters[] = {
    </xsl:text>
    <xsl:apply-templates select="//parameter"/>
    <xsl:text>{NULL, }   /* NULL terminated list */
    };

    </xsl:text>
  </xsl:template>
  
  <xsl:template match="signal">
    <xsl:text>{ </xsl:text> 

    <!-- Address of signal -->
    <xsl:text><![CDATA[&S(]]></xsl:text> 
    <xsl:value-of select="@cvar"/>
    <xsl:text>), </xsl:text> 

    <!-- Signal data type -->
    <xsl:value-of select="@datatype"/>
    <xsl:text>_en, </xsl:text> 

    <!-- Signal data type -->
    <xsl:value-of select="@sampleTimeIndex"/>
    <xsl:text>, </xsl:text> 

    <!-- Orientation and row and column counts -->
    <xsl:choose>
      <xsl:when test="@orientation='vector'">
        <xsl:text>orientation_vector, </xsl:text> 
        <xsl:value-of select="@elements"/>
        <xsl:text>, 1, </xsl:text> 
      </xsl:when>
      <xsl:when test="@orientation='matrix_row_major'">
        <xsl:text>orientation_matrix_col_major, </xsl:text> 
        <xsl:value-of select="@rnum"/>
        <xsl:text>, </xsl:text> 
        <xsl:value-of select="@cnum"/>
        <xsl:text>, </xsl:text> 
      </xsl:when>
      <xsl:when test="@orientation='matrix_col_major'">
        <xsl:text>orientation_matrix_row_major, </xsl:text> 
        <xsl:value-of select="@rnum"/>
        <xsl:text>, </xsl:text> 
        <xsl:value-of select="@cnum"/>
        <xsl:text>, </xsl:text> 
      </xsl:when>
      <xsl:otherwise>
        <xsl:text>orientation_scalar, </xsl:text> 
        <xsl:text>1, 1, </xsl:text> 
      </xsl:otherwise>
    </xsl:choose>
    
    <!-- Now write the full path as a comment -->
    <xsl:text>
    "/</xsl:text>
    <!-- First write the names of all subsystems -->
    <xsl:for-each select="ancestor::subsystem">
    	<xsl:value-of select="@name"/>
    	<xsl:text>/</xsl:text>
    </xsl:for-each>
    <xsl:value-of select="@name"/>
    <xsl:text>" },
    </xsl:text>
     
  </xsl:template>
  
  <xsl:template match="parameter">
    <xsl:text>{ </xsl:text> 

    <!-- Address of signal -->
    <xsl:text><![CDATA[&P(]]></xsl:text> 
    <xsl:value-of select="@cvar"/>
    <xsl:text>), </xsl:text> 

    <!-- Signal data type -->
    <xsl:value-of select="@datatype"/>
    <xsl:text>_en, </xsl:text> 

    <!-- Orientation and row and column counts -->
    <xsl:choose>
      <xsl:when test="@orientation='vector'">
        <xsl:text>orientation_vector, </xsl:text> 
        <xsl:value-of select="@elements"/>
        <xsl:text>, 1, </xsl:text> 
      </xsl:when>
      <xsl:when test="@orientation='matrix_row_major'">
        <xsl:text>orientation_matrix_col_major, </xsl:text> 
        <xsl:value-of select="@rnum"/>
        <xsl:text>, </xsl:text> 
        <xsl:value-of select="@cnum"/>
        <xsl:text>, </xsl:text> 
      </xsl:when>
      <xsl:when test="@orientation='matrix_col_major'">
        <xsl:text>orientation_matrix_row_major, </xsl:text> 
        <xsl:value-of select="@rnum"/>
        <xsl:text>, </xsl:text> 
        <xsl:value-of select="@cnum"/>
        <xsl:text>, </xsl:text> 
      </xsl:when>
      <xsl:otherwise>
        <xsl:text>orientation_scalar, </xsl:text> 
        <xsl:text>1, 1, </xsl:text> 
      </xsl:otherwise>
    </xsl:choose>
    
    <!-- Now write the full path as a comment -->
    <xsl:text>
    "/</xsl:text>
    <!-- First write the names of all subsystems -->
    <xsl:for-each select="ancestor::subsystem">
    	<xsl:value-of select="@name"/>
    	<xsl:text>/</xsl:text>
    </xsl:for-each>
    <xsl:value-of select="@name"/>
    <xsl:text>" },
    </xsl:text>
     
  </xsl:template>
  
</xsl:stylesheet>
