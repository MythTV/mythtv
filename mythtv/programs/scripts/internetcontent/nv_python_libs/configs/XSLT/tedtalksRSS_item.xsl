<?xml version="1.0" encoding="UTF-8"?>
<!--
    TedTalks RSS items conversion to MythNetvision item format
-->
<xsl:stylesheet version="1.0"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:media="http://search.yahoo.com/mrss/"
    xmlns:mnvXpath="http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format"
    xmlns:mythtv="http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format"

    xmlns:itunes="http://www.itunes.com/dtds/podcast-1.0.dtd"
    xmlns:atom="http://www.w3.org/2005/Atom"
    xmlns:atom10="http://www.w3.org/2005/Atom"
    xmlns:feedburner="http://rssnamespace.org/feedburner/ext/1.0">

    <xsl:param name="paraMeter" />

    <xsl:output method="xml" indent="yes" version="1.0" encoding="UTF-8" omit-xml-declaration="yes"/>

    <!--
        This template calls all other templates which allows for multiple sources to be processed
        within a single Xslt file
    -->
    <xsl:template match="/">
        <xsl:if test="//channel/link='http://www.ted.com/talks/browse'">
            <xml>
                <xsl:call-template name='tedtalksRSS'/>
            </xml>
        </xsl:if>
    </xsl:template>

    <xsl:template name="tedtalksRSS">
        <xsl:for-each select="//item">
            <dataSet>
                <xsl:if test="../itunes:image/@href">
                    <directoryThumbnail>%SHAREDIR%/mythnetvision/icons/tedtalks.png</directoryThumbnail>
                </xsl:if>
                <xsl:element name="specialDirectories">
                    <xsl:element name="mostrecent">
                        <xsl:attribute name="dirname">Most Recent</xsl:attribute>
                        <xsl:attribute name="key"><xsl:value-of select="mnvXpath:pubDate(string(pubDate), '%a, %d %b %Y %H:%M:%S', '%Y%m%dT%H:%M:%S')"/></xsl:attribute>
                        <xsl:attribute name="reverse">true</xsl:attribute>
                        <xsl:attribute name="count">5</xsl:attribute>
                    </xsl:element>
                </xsl:element>
                <xsl:element name="item">
                    <title><xsl:value-of select="mnvXpath:tedtalksTitleRSS(string(title))"/></title>
                    <author><xsl:value-of select="normalize-space(itunes:author)"/></author>
                    <pubDate><xsl:value-of select="mnvXpath:pubDate(string(pubDate), '%a, %d %b %Y %H:%M:%S')"/></pubDate>
                    <description><xsl:value-of select="normalize-space(itunes:summary)"/></description>
                    <link><xsl:value-of select="mnvXpath:tedtalksMakeLink(normalize-space(enclosure/@url), $paraMeter)"/></link>
                    <xsl:element name="media:group">
                        <xsl:element name="media:thumbnail">
                            <xsl:attribute name="url"><xsl:value-of select="normalize-space(media:thumbnail/@url)"/></xsl:attribute>
                        </xsl:element>
                        <xsl:element name="media:content">
                            <xsl:attribute name="url"><xsl:value-of select="normalize-space(media:content/@url)"/></xsl:attribute>
                            <xsl:attribute name="duration"><xsl:value-of select="mnvXpath:convertDuration(string(itunes:duration))"/></xsl:attribute>
                            <xsl:if test="enclosure/@length">
                                <xsl:attribute name="length"><xsl:value-of select='normalize-space(media:content/@fileSize)'/></xsl:attribute>
                            </xsl:if>
                            <xsl:attribute name="lang"><xsl:value-of select="normalize-space(../language)"/></xsl:attribute>
                        </xsl:element>
                    </xsl:element>
                </xsl:element>
            </dataSet>
        </xsl:for-each>
    </xsl:template>
</xsl:stylesheet>
