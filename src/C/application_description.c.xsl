<?xml version="1.0"?>
<!--
 * $Id$
 *
 * This file is used by an XSLT processor to generate the application
 * description file (mdf) as C-code. The mdf is used later when the 
 * application gets registered with the rt_kernel.
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
  <xsl:variable name="count">0</xsl:variable>

  <xsl:template match="text()|@*"/>
  
  <xsl:template match="/application">
    <xsl:text><![CDATA[
/************************************************************************
 * This is a generated file. Do not edit.
 *
 * This file contains a list of signal and parameter descriptions that
 * is used when registering the application with rt_kernel.
 *
 ************************************************************************/

#include <include/etl_application_description.h>

#include "signal.h"
#include "param.h"

#include <stddef.h>         /* For NULL */

]]>
    </xsl:text>
    <xsl:apply-templates select="data"/>
  </xsl:template>

  <xsl:template match="data">
    <xsl:for-each select=".//signal[count(dim)>2]|.//parameter[count(dim)>2]">
      <xsl:value-of select="concat('size_t dim_',generate-id(.),'[] = {')"/>
      <xsl:for-each select="dim">
        <xsl:value-of select="concat(@value,', ')"/>
      </xsl:for-each>
      <xsl:text>};

      </xsl:text>
    </xsl:for-each>
    <xsl:text>struct capi_variable capi_signals[] = {
    </xsl:text>
    <xsl:apply-templates>
      <xsl:with-param name="select">signal</xsl:with-param>
    </xsl:apply-templates>
    <xsl:text>{.address = NULL, }   /* NULL terminated list */
    };

    </xsl:text>
    <xsl:text>struct capi_variable capi_parameters[] = {
    </xsl:text>
    <xsl:apply-templates>
      <xsl:with-param name="select">parameter</xsl:with-param>
    </xsl:apply-templates>
    <xsl:text>{.address = NULL, }   /* NULL terminated list */
    };

    </xsl:text>
  </xsl:template>

  <xsl:template match="subsystem">
    <xsl:param name="prefix"/>
    <xsl:param name="select"/>
    <xsl:param name="path">
      <xsl:choose>
        <xsl:when test="$prefix">
          <xsl:value-of select="concat($prefix,'/',@name)"/>
        </xsl:when>
        <xsl:otherwise>
          <xsl:value-of select="@name"/>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:param>
    <xsl:apply-templates>
      <xsl:with-param name="prefix" select="$path"/>
      <xsl:with-param name="select" select="$select"/>
    </xsl:apply-templates>
  </xsl:template>
  
  <xsl:template match="refsystem">
    <xsl:param name="prefix"/>
    <xsl:param name="select"/>
    <xsl:apply-templates>
      <xsl:with-param name="prefix" select="$prefix"/>
      <xsl:with-param name="select" select="$select"/>
    </xsl:apply-templates>
  </xsl:template>
  
  <xsl:template match="reference">
    <xsl:param name="prefix"/>
    <xsl:param name="select"/>
    <xsl:param name="s" select="@system"/>
    <xsl:param name="path">
      <xsl:choose>
        <xsl:when test="$prefix">
          <xsl:value-of select="concat($prefix,'/',@name)"/>
        </xsl:when>
        <xsl:otherwise>
          <xsl:value-of select="@name"/>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:param>
    <xsl:apply-templates select="/application/refsystem[@name=$s]">
      <xsl:with-param name="prefix" select="$path"/>
      <xsl:with-param name="select" select="$select"/>
    </xsl:apply-templates>
  </xsl:template>

  <xsl:template match="signal">
    <xsl:param name="select"/>
    <xsl:param name="prefix"/>
    <xsl:if test="$select='signal'">
      <xsl:call-template name="signal_attributes">
        <xsl:with-param name="structure">S</xsl:with-param>
        <xsl:with-param name="stidx" select="@sampleTimeIndex"/>
        <xsl:with-param name="name" select="@name"/>
        <xsl:with-param name="path" select="$prefix"/>
        <xsl:with-param name="alias" select="@alias"/>
      </xsl:call-template>
    </xsl:if>
  </xsl:template>

  <xsl:template match="parameter">
    <xsl:param name="select"/>
    <xsl:param name="prefix"/>
    <xsl:if test="$select='parameter'">
      <xsl:call-template name="signal_attributes">
        <xsl:with-param name="structure">P</xsl:with-param>
        <xsl:with-param name="name" select="@name"/>
        <xsl:with-param name="path" select="$prefix"/>
        <xsl:with-param name="alias" select="@alias"/>
      </xsl:call-template>
    </xsl:if>
  </xsl:template>

  <xsl:template name="signal_attributes">
    <!-- Parameters for this template -->
    <xsl:param name="stidx"/>    <!-- Sample time index -->
    <xsl:param name="structure"/><!-- Set to P for parameter, S for signal -->
    <xsl:param name="name"/>     <!-- Name of signal -->
    <xsl:param name="path"/>     <!-- Signal path -->
    <xsl:param name="alias"/>    <!-- Signal alias -->

    <xsl:text>{ 
    </xsl:text> 

    <!-- Signal data type -->
    <xsl:value-of select="concat('.data_type = si_',@datatype)"/>
    <xsl:text>,
    </xsl:text> 

    <!-- Sample time index if it has a value -->
    <xsl:if test="$stidx">
    <xsl:value-of select="concat('.st_index = ',$stidx)"/>
    <xsl:text>,
    </xsl:text> 
    </xsl:if>

    <!-- Orientation and row and column counts -->
    <xsl:choose>
      <xsl:when test="count(dim)=0">
        <xsl:text>.dim = {1,0},
        </xsl:text>
      </xsl:when>
      <xsl:when test="count(dim)=1">
        <xsl:value-of select="concat('.dim = {',dim[1]/@value,',0}')"/>
        <xsl:text>,
        </xsl:text>
      </xsl:when>
      <xsl:when test="count(dim)=2">
        <xsl:value-of select="concat(
                '.dim = {',
                dim[1]/@value,
                ',',
                dim[2]/@value,
                '}'
                )"/>
        <xsl:text>,
        </xsl:text>
      </xsl:when>
      <xsl:otherwise>
        <xsl:value-of select="concat('.dim = {0,',count(dim),'}')"/>
        <xsl:text>,
        </xsl:text>
      </xsl:otherwise>
    </xsl:choose>
    <xsl:variable name="count"><xsl:value-of select="$count+1"/></xsl:variable>
    
    <xsl:text>.name = "</xsl:text>
    <xsl:value-of select="$name"/>
    <xsl:text>",
    </xsl:text>

    <xsl:text>.alias = "</xsl:text>
    <xsl:value-of select="$alias"/>
    <xsl:text>",
    </xsl:text>

    <!-- Now write the full path as a comment -->
    <xsl:text>.path = "</xsl:text>
    <xsl:value-of select="$path"/>
    <xsl:text>",
    </xsl:text>

    <!-- Address of signal -->
    <xsl:value-of select="concat('.address = &amp;',$structure,'(')"/>

    <!-- Drop the first '/' and translate all slashes to dots. -->
    <xsl:value-of select="concat(translate($path,'/','.'),'.',$name)"/>
    <xsl:for-each select="dim">
      <xsl:text>[0]</xsl:text>
    </xsl:for-each>
    <xsl:text>),
    </xsl:text> 

    <xsl:if test="count(dim)>2">
      <xsl:value-of select="concat('.extra_dim = dim_',generate-id(.))"/>
      <xsl:text>,
      </xsl:text>
    </xsl:if>
    <xsl:text> },
    </xsl:text>
     
  </xsl:template>
  
</xsl:stylesheet>
