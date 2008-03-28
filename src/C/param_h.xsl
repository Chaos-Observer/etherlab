<?xml version="1.0"?>
<!--
 * $Id$
 *
 * This file is used by an XSLT processor to generate the parameter header 
 * file. Parameters are values that are used during the calculation 
 * involving the current value table. Parameters can be changed by the
 * user through the rt_kernel.
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

  <xsl:variable name="xx">signal</xsl:variable>
  
  <xsl:template match="text()|@*"/>
  
  <xsl:template match="/">
    <xsl:apply-templates select="modeldescription/data"/>
  </xsl:template>
  
  <xsl:template match="data">
    <xsl:text>
/*
 * This is a generated file. Do not edit.
 *
 * This is the structure definition for the model parameters.
 * Any parameter that should be changed externally has to exist in this
 * structure.
 *
 * To use variables in this table, do not reference the structure
 * directly, but use P(param) instead.
 */

    #include "include/etltypes.h"
    </xsl:text>
    <xsl:text>struct param {
    </xsl:text>
    <xsl:apply-templates/>
    <xsl:text>};</xsl:text>

    extern struct param param;

    #define P(x) param.x
  </xsl:template>
  
  <xsl:template match="parameter">
    <xsl:value-of select="@datatype"/>
    <xsl:text> </xsl:text> 
    <xsl:value-of select="@cvar"/>
    <xsl:choose>
      <xsl:when test="@orientation='vector'">
        <xsl:call-template name="elements"/>
      </xsl:when>
      <xsl:when test="@orientation='matrix_row_major'">
        <xsl:call-template name="rnum"/>
        <xsl:call-template name="cnum"/>
      </xsl:when>
      <xsl:when test="@orientation='matrix_col_major'">
        <xsl:call-template name="cnum"/>
        <xsl:call-template name="rnum"/>
      </xsl:when>
    </xsl:choose>
    <xsl:text>; </xsl:text>
    
      <!-- Now write the full path as a comment -->
    <xsl:text>/* /</xsl:text>
    <!-- First write the names of all subsystems -->
    <xsl:for-each select="ancestor::subsystem">
    	<xsl:value-of select="@name"/>
    	<xsl:text>/</xsl:text>
    </xsl:for-each>
    <xsl:value-of select="@name"/>
    <xsl:text> */
    </xsl:text>
  
  </xsl:template>
  
  <xsl:template name="rnum">
    <xsl:text>[</xsl:text>
    <xsl:value-of select="@rnum"/>
    <xsl:text>]</xsl:text>
  </xsl:template>
  
  <xsl:template name="cnum">
    <xsl:text>[</xsl:text>
    <xsl:value-of select="@cnum"/>
    <xsl:text>]</xsl:text>
  </xsl:template>

  <xsl:template name="elements">
    <xsl:text>[</xsl:text>
    <xsl:value-of select="@elements"/>
    <xsl:text>]</xsl:text>
  </xsl:template>
</xsl:stylesheet>
