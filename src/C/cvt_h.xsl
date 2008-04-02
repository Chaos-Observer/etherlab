<?xml version="1.0"?>
<!--
 * $Id$
 *
 * This file is used by an XSLT processor to generate the header file of
 * the current value table (cvt). All model calculations operate on the 
 * current value table, and only these values can be made visible to the
 * user by the rt_kernel.
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
  
  <xsl:template match="/model">
    <xsl:apply-templates select="data"/>
  </xsl:template>
  
  <xsl:template match="data">
    <xsl:text>
/* 
 * This is a generated file. Do not edit.
 * 
 * This is the structure definition for the Current Value Table,
 * or cvt for short. All operations should use this table for input
 * and output.
 *
 * To use variables in this table, do not reference the structure
 * directly, but use S(sig) instead.
 */

    <![CDATA[#include "include/etltypes.h"]]>

    </xsl:text>
    <xsl:text>struct cvt {
    </xsl:text>
    <xsl:apply-templates/>
    <xsl:text>};
    extern struct cvt cvt;

    #define S(x) cvt.x
    
    </xsl:text>

  </xsl:template>
  
  <xsl:template match="subsystem">
    <xsl:param name="reference"/>
    <xsl:param name="prefix"/>
    <xsl:param name="path">
      <xsl:value-of select="$prefix"/>/<xsl:value-of select="@name"/>
    </xsl:param>

    <xsl:text>struct </xsl:text>
    <!-- Now write the full path as a comment -->
    <xsl:text>/* </xsl:text>
    <!-- First write the names of all subsystems -->
    <xsl:value-of select="$path"/>
    <xsl:text> */
    </xsl:text>
    <xsl:text> {
    </xsl:text>
    <xsl:apply-templates select="*[name()!='dim']">
      <xsl:with-param name="prefix">
        <xsl:value-of select="$path"/>
      </xsl:with-param>
    </xsl:apply-templates>
    <xsl:text>} </xsl:text>
    <xsl:value-of select="@name"/>

    <!-- 
    If subsystem was called by reference, the caller must do the dimensioning
    -->
    <xsl:if test="not($reference)">
      <xsl:apply-templates select="dim"/>
      <xsl:text>;
      </xsl:text>
    </xsl:if>
  </xsl:template>

  <xsl:template match="cstruct">
    <xsl:text>struct </xsl:text>
    <xsl:value-of select="@structname"/>
    <xsl:text> </xsl:text>
    <xsl:value-of select="@name"/>
    <xsl:apply-templates select="dim"/>
    <xsl:text>;
    </xsl:text>
  </xsl:template>

  <xsl:template match="dim">
    <xsl:text>[</xsl:text>
    <xsl:value-of select="@value"/>
    <xsl:text>]</xsl:text>
  </xsl:template>

  <xsl:template match="reference">
    <xsl:param name="prefix"/>
    <xsl:param name="v"><xsl:value-of select="@value"/></xsl:param>
    <xsl:apply-templates select="/model/subsystem[@name=$v]">
      <xsl:with-param name="prefix">
        <xsl:value-of select="$prefix"/>
      </xsl:with-param>
      <xsl:with-param name="reference">Y</xsl:with-param>
    </xsl:apply-templates>
    <xsl:apply-templates select="dim"/>
    <xsl:text>;
    </xsl:text>
  </xsl:template>

  <xsl:template match="signal|parameter">
    <xsl:value-of select="@datatype"/>
    <xsl:text> </xsl:text> 
    <xsl:value-of select="@name"/>
    <xsl:apply-templates select="dim"/>
    <xsl:text>;
    </xsl:text>
  </xsl:template>
  
</xsl:stylesheet>
