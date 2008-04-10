<?xml version="1.0"?>
<!--
 * $Id$
 *
 * This file is used by an XSLT processor to generate a header file
 * containing many defines describing the model.
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
  
  <xsl:template match="/application">
    <xsl:text>
/* 
 * This is a generated file. Do not edit.
 * 
 * This generated file contains #defines extracted from the model .xml file
 *
 */
    </xsl:text>
    <xsl:choose>
      <xsl:when test="count(task/subtask)">
        <!-- Subtasks were defined. Add these to the base task  -->
        #define NUMST <xsl:value-of select="count(task/subtask)+1"/>
      </xsl:when>
      <xsl:otherwise>
        <!-- Only the base task was defined -->
        #define NUMST 1
        #define TASK_DECIMATIONS 1
      </xsl:otherwise>
    </xsl:choose>
    #define BASEPERIOD   <xsl:value-of select="task/@basetick"/>
    #define MODELVERSION <xsl:value-of select="version"/>
    <xsl:text>
    </xsl:text>

    <xsl:text>#define DECIMATION </xsl:text>
    <xsl:choose>
      <xsl:when test="parameters/@sample_decimation">
        <xsl:value-of select="parameters/@sample_decimation"/>
      </xsl:when>
      <xsl:otherwise>
        <xsl:text>1</xsl:text>
      </xsl:otherwise>
    </xsl:choose>
    <xsl:text>
    </xsl:text>

    <xsl:text>#define OVERRUNMAX </xsl:text>
    <xsl:choose>
      <xsl:when test="parameters/@max_overruns">
        <xsl:value-of select="parameters/@max_overruns"/>
      </xsl:when>
      <xsl:otherwise>
        <xsl:text>1</xsl:text>
      </xsl:otherwise>
    </xsl:choose>
    <xsl:text>
    </xsl:text>

    <xsl:text>#define BUFFER_TIME </xsl:text>
    <xsl:choose>
      <xsl:when test="parameters/@buffer_time">
        <xsl:value-of select="parameters/@buffer_time"/>
      </xsl:when>
    </xsl:choose>
    <xsl:text>
    </xsl:text>

    <xsl:text>#define STACKSIZE </xsl:text>
    <xsl:choose>
      <xsl:when test="parameters/@stacksize">
        <xsl:value-of select="parameters/@stacksize"/>
      </xsl:when>
      <xsl:otherwise>
        <xsl:text>2048</xsl:text>
      </xsl:otherwise>
    </xsl:choose>
    <xsl:text>
    </xsl:text>

  </xsl:template>
  
</xsl:stylesheet>
