<?xml version="1.0"?>
<!--
 * $Id$
 *
 * This file is used by an XSLT processor to generate the parameter source 
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

  <xsl:template match="text()|@*"/>
  
  <xsl:template match="/modeldescription">
#include "include/model.h"

/* Parameter initialisation and definition */
    <xsl:apply-templates select="data"/>
/* Current Value Table definition */
struct cvt cvt;

/* Task timing information */
    <xsl:apply-templates select="task"/>
  </xsl:template>
  
  <xsl:template match="task">
    <xsl:if test="subtask">
      <xsl:text>
      unsigned int subtask_decimation[] = {
      </xsl:text>
      <xsl:for-each select="subtask">
        <xsl:value-of select="@decimation"/>,
      </xsl:for-each>
      <xsl:text> };
      </xsl:text>
    </xsl:if>
  </xsl:template>
  
  <xsl:template match="data">
    <xsl:text>struct param param = {
    </xsl:text>
    <xsl:apply-templates select=".//parameter"/>
    <xsl:text>};
    </xsl:text>
  </xsl:template>
  
  <xsl:template match="parameter">
    <xsl:text>.</xsl:text> 
    <xsl:value-of select="@cvar"/>
    <xsl:text> = </xsl:text> 
    <xsl:if test="@orientation">
    <xsl:text> { </xsl:text> 
    </xsl:if>
    <xsl:value-of select="@value"/>
    <xsl:if test="@orientation">
    <xsl:text> }</xsl:text> 
    </xsl:if>
    <xsl:text>,
    </xsl:text> 
  
  </xsl:template>
  
</xsl:stylesheet>
