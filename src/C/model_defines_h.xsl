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
  
  <xsl:template match="/modeldescription">
    #define NUMST <xsl:value-of select="count(task/subtask)"/>
    #define BASEPERIOD <xsl:value-of select="task/@basetick"/>
    #define MODELVERSION <xsl:value-of select="version"/>
    #define DECIMATION <xsl:value-of select="properties/@sample_decimation"/>
    #define OVERRUNMAX <xsl:value-of select="properties/@max_overruns"/>
    #define BUFFER_TIME <xsl:value-of select="properties/@buffer_time"/>
    #define STACKSIZE <xsl:value-of select="properties/@stacksize"/>
  </xsl:template>
  
</xsl:stylesheet>
