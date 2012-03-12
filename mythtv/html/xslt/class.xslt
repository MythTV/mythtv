<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform" xmlns:xs="http://www.w3.org/2001/XMLSchema">
	<xsl:output method="html" version="1.0" encoding="UTF-8" indent="yes"/>
<xsl:template match="/"	>
<html>
	<head>
		<title><xsl:value-of select="/xs:schema/xs:complexType/@name"/> Data Contract</title>
		<meta content="text/html; charset=utf-8" http-equiv="Content-Type" />
		<link href="/css/wsdl.css" rel="stylesheet" type="text/css" />
	</head>
	<body>
	
		<div class="masthead">
			<a href="http://www.mythtv.org"><img alt="mythTV" src="/images/mythtv.png" /></a>
			<div class="title">
				<h1><xsl:value-of select="/xs:schema/xs:complexType/@name"/> Data Contract</h1>
			</div>
		</div>
		<hr />
		<xsl:variable name="CurrentType" select="/xs:schema/xs:complexType/@name" />

		<p class="alignLeft"><xsl:value-of select="//xs:documentation/text()" /></p>
		<p class="alignRight"><a href="javascript:window.location.href += '&amp;raw=1';" >View Xsd</a></p> 
		<br />
		
		<div class="method" >
			<div class="method_name">
				<p class="alignLeft"><xsl:value-of select="/xs:schema/xs:complexType/@name"/></p>
				<p class="alignRight"></p>
			</div>
			<div class="method_props">
				<table>
					<tr>
						<td width="33%"><span class="label">Properties:</span></td>
						<td width="33%" />
						<td width="33%" />
					</tr>

					<xsl:apply-templates select="/xs:schema/xs:complexType/xs:sequence/xs:element" />
				</table>
			</div>
		</div>
	</body>

</html>

</xsl:template>

<xsl:template match="/xs:schema/xs:complexType/xs:sequence/xs:element" >

	<xsl:variable name="Search" select="substring-after( @type, 'tns:' )" />

	<tr>
		<td></td>

		<xsl:variable name="PropType" >
			<xsl:choose>
				<xsl:when test="starts-with(@type, 'tns:ArrayOf')">
					<xsl:value-of select="concat(substring-after(@type, 'ArrayOf'), '[]')" />
				</xsl:when>
				<xsl:otherwise>
					<xsl:value-of select="@type" />
				</xsl:otherwise>
			</xsl:choose>
   
	    </xsl:variable>

		<xsl:choose>
			<xsl:when test="$Search and /xs:schema/xs:include[contains( @schemaLocation, $Search)]">
				<td class="prop_type"><a href="{ /xs:schema/xs:include[contains( @schemaLocation, $Search)]/@schemaLocation}"><xsl:value-of select="$PropType"/></a></td>
			</xsl:when>
			<xsl:otherwise>
				<td class="prop_type"><xsl:value-of select="$PropType"/></td>
			</xsl:otherwise>
		</xsl:choose>
		<td class="prop_name"><xsl:value-of select="@name"/></td>
	</tr>

</xsl:template>
	
</xsl:stylesheet>

