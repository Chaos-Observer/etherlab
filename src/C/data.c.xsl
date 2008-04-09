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
  
  <xsl:template match="/model">
#include "include/model.h"

/* Parameter initialisation and definition */
    <xsl:apply-templates select="data"/>
/* Current Value Table definition */
struct signal signal;

/* Task timing information */
    <xsl:apply-templates select="task"/>
  </xsl:template>
  
  <xsl:template match="task">
      <xsl:text>
      unsigned int task_period[] = {
      </xsl:text>
      <xsl:value-of select="@basetick"/>,
      <xsl:for-each select="subtask">
        <xsl:value-of select="@decimation*../@basetick"/>,
      </xsl:for-each>
      <xsl:text> };
      </xsl:text>
  </xsl:template>
  
  <xsl:template match="data">
    <xsl:text>struct param param = {
    </xsl:text>
    <xsl:apply-templates>
      <xsl:with-param name="type">param</xsl:with-param>
    </xsl:apply-templates>
    <xsl:text>};
    </xsl:text>
    <xsl:text>struct inputptr inputptr = {
    </xsl:text>
    <xsl:apply-templates>
      <xsl:with-param name="type">inputptr</xsl:with-param>
    </xsl:apply-templates>
    <xsl:text>};
    </xsl:text>
  </xsl:template>

  <xsl:template match="subsystem">
    <xsl:param name="prefix"/>
    <xsl:param name="type"/>
    <xsl:param name="path" select="concat($prefix,'.',@name)"/>
    <!-- only descend if there are any "parameter" child elements -->
    <xsl:choose>
      <xsl:when test="$type='param'">
        <xsl:if test=".//parameter">
          <xsl:apply-templates>
            <xsl:with-param name="prefix" select="$path"/>
            <xsl:with-param name="type" select="$type"/>
          </xsl:apply-templates>
        </xsl:if>
      </xsl:when>
      <xsl:when test="$type='inputptr'">
        <xsl:if test=".//pointer">
          <xsl:apply-templates>
            <xsl:with-param name="prefix" select="$path"/>
            <xsl:with-param name="type" select="$type"/>
          </xsl:apply-templates>
        </xsl:if>
      </xsl:when>
    </xsl:choose>
  </xsl:template>

  <xsl:template match="reference">
    <xsl:param name="prefix"/>
    <xsl:param name="type"/>
    <xsl:choose>
      <xsl:when test="$type='param'">
        <xsl:if test="param">
          <xsl:value-of select="concat($prefix,'.',name())"/>
          <xsl:text> = {
          </xsl:text>
          <xsl:for-each select="param">
            <xsl:value-of select="concat('.',@path,' = ')"/>
            <xsl:apply-templates select="value"/>
            <xsl:text>
            </xsl:text>
          </xsl:for-each>
          <xsl:text>},
          </xsl:text>
        </xsl:if>
      </xsl:when>
      <xsl:when test="$type='inputptr'">
        <xsl:if test="pointer">
          <xsl:value-of select="concat($prefix,'.',name())"/>
          <xsl:text> = {
          </xsl:text>
          <xsl:for-each select="pointer">
            <xsl:value-of select="concat('.',@path,' = ')"/>
            <xsl:value-of select="concat('&amp;S(',value,')')"/>
            <xsl:text>,
            </xsl:text>
          </xsl:for-each>
          <xsl:text>},
          </xsl:text>
        </xsl:if>
      </xsl:when>
    </xsl:choose>
  </xsl:template>
  
  <xsl:template match="cstruct">
  </xsl:template>
  
  <xsl:template match="parameter">
    <xsl:param name="prefix"/>
    <xsl:param name="type"/>
    <xsl:param name="path" select="concat($prefix,'.',@name)"/>
    <xsl:if test="$type='param'">
      <xsl:value-of select="$path"/>
      <xsl:text> = </xsl:text> 
      <xsl:apply-templates select="value"/>
    </xsl:if>
  </xsl:template>

  <xsl:template match="value">
    <xsl:choose>
      <xsl:when test=".//value">
        <xsl:text> { </xsl:text> 
        <xsl:apply-templates/>
        <xsl:text> },
        </xsl:text> 
      </xsl:when>
      <xsl:when test="value">
        <xsl:apply-templates/>
      </xsl:when>
      <xsl:otherwise>
        <xsl:value-of select="text()"/>
        <xsl:text>,</xsl:text>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>
  
</xsl:stylesheet>
