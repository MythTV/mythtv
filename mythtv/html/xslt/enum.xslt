<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform" xmlns:xs="http://www.w3.org/2001/XMLSchema">
	<xsl:output method="html" version="1.0" encoding="UTF-8" indent="yes"/>
<xsl:template match="/"	>
<html>
	<head>
		<title><xsl:value-of select="/xs:schema/xs:simpleType/@name"/> Enum</title>
		<meta content="text/html; charset=utf-8" http-equiv="Content-Type" />
		<link href="/css/wsdl.css" rel="stylesheet" type="text/css" />
	</head>
	<body>
	
		<div class="masthead">
			<a href="http://www.mythtv.org"><img alt="mythTV" src="/images/mythtv.png" /></a>
			<div class="title">
				<h1><xsl:value-of select="/xs:schema/xs:simpleType/@name"/> Enum</h1>
			</div>
		</div>
		<hr />
		<xsl:variable name="CurrentEnum" select="/xs:schema/xs:simpleType/@name" />

		<p class="alignLeft"><xsl:value-of select="//xs:documentation/text()" /></p>
		<p class="alignRight"><a href="javascript:window.location.href += '&amp;raw=1';" >View Xsd</a></p> 
		<br />
		
		<div class="enum" >
			<div class="enum_name">
				<p class="alignLeft"><xsl:value-of select="/xs:schema/xs:simpleType/@name"/></p>
				<p class="alignRight"></p>
			</div>
			<div class="enum_values">
				<table>
					<tr>
						<td width="10%" />
						<td width="33%" />
						<td width="33%" />
					</tr>

					<xsl:apply-templates select="/xs:schema/xs:simpleType/xs:restriction/xs:enumeration" />
				</table>
			</div>
		</div>
	</body>

</html>

</xsl:template>

<xsl:template match="/xs:schema/xs:simpleType/xs:restriction/xs:enumeration" >

	<tr>
		<td></td>

			<td class="enum_name" ><xsl:value-of select="@value"/></td>
  		<td class="enum_value">= <xsl:value-of select="xs:annotation/xs:appinfo/EnumerationValue"/></td>
      <td class="enum_value">[ <xsl:value-of select="xs:annotation/xs:appinfo/EnumerationDesc" /> ]</td>
  </tr>

</xsl:template>
	
</xsl:stylesheet>

