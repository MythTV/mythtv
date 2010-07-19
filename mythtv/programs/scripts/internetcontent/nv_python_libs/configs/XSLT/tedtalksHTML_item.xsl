<?xml version="1.0" encoding="UTF-8"?>
<!--
    Search TedTalks items conversion to MythNetvision item format
-->
<xsl:stylesheet version="1.0"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:media="http://search.yahoo.com/mrss/"
    xmlns:mnvXpath="http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format"
    xmlns:mythtv="http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format"

	xmlns:xhtml="http://www.w3.org/1999/xhtml"
	xmlns:fb="http://www.facebook.com/2008/fbml">

    <xsl:param name="paraMeter" />

    <xsl:output method="xml" indent="yes" version="1.0" encoding="UTF-8" omit-xml-declaration="yes"/>

    <!--
        This template calls all other templates which allows for multiple sources to be processed
        within a single Xslt file
    -->
    <xsl:template match="/">
        <xml>
            <xsl:call-template name='tedtalks'/>
        </xml>
    </xsl:template>

    <xsl:template name="tedtalks">
        <xsl:for-each select="//dt/a">
            <xsl:if test="mnvXpath:testSubString('ends', normalize-space(.), 'Video on TED.com')">
                <xsl:if test="mnvXpath:tedtalksMakeItem(concat('http://www.ted.com', normalize-space(./@href)), $paraMeter)/link">
                    <xsl:element name="item">
                        <title><xsl:value-of select="normalize-space(substring-before(./text(), '|'))"/></title>
                        <author>Ted.com</author>
                        <xsl:copy-of select="mnvXpath:tedtalksGetItem(concat('http://www.ted.com', normalize-space(./@href)))/*"/>
                    </xsl:element>
                </xsl:if>
            </xsl:if>
        </xsl:for-each>
    </xsl:template>
</xsl:stylesheet>
