<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet version="1.0" xmlns:w="http://schemas.xmlsoap.org/wsdl/" xmlns:xsl="http://www.w3.org/1999/XSL/Transform" xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:soap="http://schemas.xmlsoap.org/wsdl/soap/">
	<xsl:output method="html" version="1.0" encoding="UTF-8" indent="yes"/>
<xsl:template match="/"	>
<html>

<head>
<title><xsl:value-of select="/w:definitions/w:service/@name"/> Service Interface</title>
<meta content="text/html; charset=utf-8" http-equiv="Content-Type" />
<link href="/css/wsdl.css" rel="stylesheet" type="text/css" />
</head>
	<body>
	
		<div class="masthead">
			<a href="http://www.mythtv.org"><img alt="mythTV" src="/images/mythtv.png" /></a>
			<div class="title">
				<h1><xsl:value-of select="/w:definitions/w:service/@name"/> Service Interface </h1>
					<xsl:value-of select="/w:definitions/w:service/w:port/soap:address/@location" />
			</div>
		</div>
		<hr />
		<p class="alignLeft"><xsl:value-of select="/w:definitions/w:service/w:documentation/text()" /></p>
		<p class="alignRight"><a href="./wsdl?raw=1">View Xsd</a></p>
		<br />

		<xsl:apply-templates select="/w:definitions/w:portType/w:operation" />
	</body>

</html>
</xsl:template>

<xsl:template match="/w:definitions/w:portType/w:operation" >
	<xsl:variable name="ResponseType" select="w:output/@message" />

	<xsl:variable name="RespTypeName" select="/w:definitions/w:message[ @name=substring-after( $ResponseType, 'tns:') ]/w:part/@element" />
	<xsl:variable name="ResultType"        select="substring-after( $RespTypeName, 'tns:' )" />
	<xsl:variable name="FQNType"          select="/w:definitions/w:types/xs:schema/xs:element[ @name=$ResultType]//xs:element[1]/@type" />
	<xsl:variable name="Type"                select="substring-after( $FQNType, 'tns:' )" />

	<xsl:variable name="RequestType" select="w:input/@message" />

	<xsl:variable name="ParamTypeName" select="/w:definitions/w:message[ @name=substring-after( $RequestType, 'tns:') ]/w:part/@element" />
	<xsl:variable name="ParamReqType"    select="substring-after( $ParamTypeName, 'tns:' )" />
	<xsl:variable name="ParamFQNType"  select="/w:definitions/w:types/xs:schema/xs:element[ @name=$ParamReqType]//xs:element[1]/@type" />
	<xsl:variable name="ParamType"         select="substring-after( $ParamFQNType, 'tns:' )" />

	<xsl:variable name="RetTypeDispName" >
		<xsl:choose>
			<xsl:when test="starts-with($Type, 'ArrayOf')">
				<xsl:value-of select="concat(substring-after($Type, 'ArrayOf'), '[]')" />
			</xsl:when>
			<xsl:otherwise>
				<xsl:value-of select="$Type" />
			</xsl:otherwise>
		</xsl:choose>
   
    </xsl:variable>

	<div class="method" >
		<div class="method_name">
			<p class="alignLeft"><xsl:value-of select="@name"/></p>
			<p class="alignRight"></p>
		</div>
		<div style="clear: both;"><span class="HTTPMethod"><xsl:value-of select="w:documentation/text()" /></span></div>
		<div class="method_props">
			<table>
				<tr>
					<td width="25%"><span class="label">Returns:</span></td>
					<td width="25%">
						<xsl:choose>
							<xsl:when test="$Type and /w:definitions/w:types/xs:schema/xs:import[contains( @schemaLocation, $Type)]">
								<a href="{ /w:definitions/w:types/xs:schema/xs:import[contains( @schemaLocation, $Type)]/@schemaLocation}"><xsl:value-of select="$RetTypeDispName"/></a>
							</xsl:when>
							<xsl:otherwise>
								<xsl:value-of select="$FQNType"/>
							</xsl:otherwise>
						</xsl:choose>
					</td>
					<td width="25%" />
					<td width="25%" />
				</tr>
				<tr>
					<td colspan="4"><span class="label">Parameters:</span></td>
				</tr>
				<xsl:apply-templates select="/w:definitions/w:types/xs:schema/xs:element[ @name=$ParamReqType]//xs:element" />
			</table>
		</div>
	</div>
	<br />

</xsl:template>

<xsl:template match="xs:element" >

	<xsl:variable name="Search" select="substring-after( @type, 'tns:' )" />

	<tr>
		<td></td>
		<xsl:choose>
			<xsl:when test="$Search and /xs:schema/xs:include[contains( @schemaLocation, $Search)]">
				<td class="prop_type"><a href="{ /xs:schema/xs:include[contains( @schemaLocation, $Search)]/@schemaLocation}"><xsl:value-of select="@type"/></a></td>
			</xsl:when>
			<xsl:otherwise>
				<td class="prop_type"><xsl:value-of select="@type"/></td>
			</xsl:otherwise>
		</xsl:choose>
	
		<td class="prop_name"><xsl:value-of select="@name"/></td>
		<td></td>
	</tr>

</xsl:template>

</xsl:stylesheet>
