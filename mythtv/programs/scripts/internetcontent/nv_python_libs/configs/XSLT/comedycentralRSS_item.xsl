<?xml version="1.0" encoding="UTF-8"?>
<!--
    Comedy Central items conversion to MythNetvision item format
-->
<xsl:stylesheet version="1.0"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:media="http://search.yahoo.com/mrss/"
    xmlns:mnvXpath="http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format"
    xmlns:mythtv="http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format"

    xmlns:dc="http://purl.org/dc/elements/1.1/">

    <xsl:param name="paraMeter" />

    <xsl:output method="xml" indent="yes" version="1.0" encoding="UTF-8" omit-xml-declaration="yes"/>

    <!--
        This template calls all other templates which allows for multiple sources to be processed
        within a single Xslt file
    -->
    <xsl:template match="/">
        <xml>
            <xsl:call-template name='comedycentral'/>
        </xml>
    </xsl:template>

    <xsl:template name="comedycentral">
        <xsl:for-each select="//item">
            <dataSet>
                <directoryThumbnail><xsl:value-of select="normalize-space($paraMeter)"/></directoryThumbnail>
                <xsl:element name="item">
                    <title><xsl:value-of select="normalize-space(title)"/></title>
                    <author>Comedy Central</author>
                    <pubDate><xsl:value-of select="mnvXpath:pubDate(string(pubDate), '%Y-%m-%d')"/></pubDate>
                    <description><xsl:value-of select="normalize-space(description)"/></description>
                    <link><xsl:value-of select="mnvXpath:comedycentralMakeLink(.)"/></link>
                    <xsl:element name="media:group">
                        <xsl:element name="media:thumbnail">
                            <xsl:if test="normalize-space(.//url) != 'http://www.comedycentral.com'">
                                <xsl:attribute name="url"><xsl:value-of select="normalize-space(.//url)"/></xsl:attribute>
                            </xsl:if>
                            <xsl:if test="normalize-space(.//url) = 'http://www.comedycentral.com'">
                                <xsl:attribute name="url">%SHAREDIR%/mythnetvision/icons/comedycentral.png</xsl:attribute>
                            </xsl:if>
                        </xsl:element>
                        <xsl:element name="media:content">
                            <xsl:attribute name="url"><xsl:value-of select="mnvXpath:comedycentralMakeLink(.)"/></xsl:attribute>
                            <xsl:attribute name="lang"><xsl:value-of select="substring-before(normalize-space(../language), '-')"/></xsl:attribute>
                        </xsl:element>
                    </xsl:element>
                    <mythtv:country>us</mythtv:country>
                    <mythtv:customhtml>true</mythtv:customhtml>
                </xsl:element>
            </dataSet>
        </xsl:for-each>
    </xsl:template>
</xsl:stylesheet>
