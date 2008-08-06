<?xml version="1.0"?>
<!--
 * $Id: header.xsl 266 2008-04-11 14:58:33Z ha $
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
  
  <xsl:template match="/application">
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

    <![CDATA[#include <etltypes.h>]]>
    </xsl:text>

    <!-- do include element processing -->
    <xsl:apply-templates select="include"/>
    <xsl:text>
    </xsl:text>

    <!-- do referenced system processing -->
    <xsl:text>/* Referenced C structures */
    </xsl:text>
    <xsl:apply-templates select="refsystem"/>
    <xsl:text>
    </xsl:text>

    <!-- do current value table processing -->
    <xsl:text>/* Current value table vector */
    </xsl:text>
    <xsl:apply-templates select="data"/>
  </xsl:template>
  
  <!--
  Include element
  This processes the element to include C headers.

  <include>
    <file>my-include.h</file>       == local include
    <system>stdio.h</system>        == system include
  </include>

  local includes look like 
    #include "my-include.h"

  whereas system includes look like
    #include <stdio.h>

  in the final output.
  -->
  <xsl:template match="include">
    <xsl:apply-templates/>
  </xsl:template>

  <!-- A locally included file -->
  <xsl:template match="file">
    <xsl:text>#include "</xsl:text>
      <xsl:value-of select="text()"/>
    <xsl:text>"
    </xsl:text>
  </xsl:template>

  <!-- A system included file -->
  <xsl:template match="system">
    <xsl:text><![CDATA[#include <]]></xsl:text>
    <xsl:value-of select="text()"/>
    <xsl:text><![CDATA[>]]>
    </xsl:text>
  </xsl:template>

  <!--
  Referenced system processing.

  Referenced systems are systems that can be used in the data section.
  They will be included as a single struct in that section.
  The structure definition will be done here.

  The output is very similar to that of the data section.

  The difference between referenced systems and <cstruct> is that referenced
  systems have full signal path information, whereas <cstruct> systems are included
  verbatim without any signal information.
  -->

  <xsl:template match="refsystem">
    <xsl:text>struct </xsl:text>
    <xsl:value-of select="concat(@name,'_',$header_type)"/>
    <xsl:text> {
    </xsl:text>
    <xsl:apply-templates/>
    <xsl:text>};
    </xsl:text>
  </xsl:template>

  <xsl:template match="data">
    <xsl:text>struct 
    </xsl:text>
    <xsl:value-of select="$header_type"/>
    <xsl:text> {
    </xsl:text>
    <xsl:apply-templates/>
    <xsl:text>};

    </xsl:text>
    <xsl:value-of select="concat('extern struct ',$header_type,' ',$header_type,';')"/>

    <xsl:text>
    </xsl:text>

    <xsl:choose>
      <xsl:when test="$header_type='signal'">
        #define S(x) signal.x
      </xsl:when>
      <xsl:when test="$header_type='param'">
        #define P(x) param.x
      </xsl:when>
    </xsl:choose>

  </xsl:template>
  
  <xsl:template match="subsystem">
    <xsl:param name="prefix"/>
    <xsl:param name="path">
      <xsl:value-of select="concat($prefix,'/',@name)"/>
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
    <xsl:apply-templates>
      <xsl:with-param name="prefix">
        <xsl:value-of select="$path"/>
      </xsl:with-param>
    </xsl:apply-templates>
    <xsl:text>} </xsl:text>
    <xsl:value-of select="@name"/>
    <xsl:text>;
    </xsl:text>
  </xsl:template>

  <xsl:template match="reference">
    <xsl:param name="prefix"/>
    <xsl:text>struct </xsl:text>
    <xsl:value-of select="@system"/> 
    <xsl:value-of select="concat('_',$header_type,' ')"/>
    <xsl:value-of select="@name"/>;
  </xsl:template>

  <xsl:template match="parameter">
    <xsl:if test="$header_type='param'">
      <xsl:value-of select="@datatype"/>
      <xsl:text> </xsl:text> 
      <xsl:value-of select="@name"/>
      <xsl:apply-templates select="dim"/>
      <xsl:text>;
      </xsl:text>
    </xsl:if>
  </xsl:template>

  <xsl:template match="signal">
    <xsl:if test="$header_type='signal'">
      <xsl:value-of select="@datatype"/>
      <xsl:text> </xsl:text> 
      <xsl:value-of select="@name"/>
      <xsl:apply-templates select="dim"/>
      <xsl:text>;
      </xsl:text>
    </xsl:if>
  </xsl:template>
  
  <xsl:template match="dim">
    <xsl:text>[</xsl:text>
    <xsl:value-of select="@value"/>
    <xsl:text>]</xsl:text>
  </xsl:template>

</xsl:stylesheet>
