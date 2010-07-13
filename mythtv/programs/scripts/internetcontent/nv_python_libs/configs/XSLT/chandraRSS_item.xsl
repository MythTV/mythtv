<?xml version="1.0" encoding="UTF-8"?>
<!--
    Chandra items conversion to MythNetvision item format
-->
<xsl:stylesheet version="1.0"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:media="http://search.yahoo.com/mrss/"
    xmlns:mnvXpath="http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format"
    xmlns:mythtv="http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format"

    xmlns:itunes="http://www.itunes.com/dtds/podcast-1.0.dtd"
    xmlns:atom="http://www.w3.org/2005/Atom"
    xmlns:amp="http://www.adobe.com/amp/1.0">

    <xsl:output method="xml" indent="yes" version="1.0" encoding="UTF-8" omit-xml-declaration="yes"/>

    <!--
        This template calls all other templates which allows for multiple sources to be processed
        within a single Xslt file
    -->
    <xsl:template match="/">
        <xsl:if test="//channel/link='http://chandra.harvard.edu/resources/podcasts/'">
            <xml>
                <xsl:call-template name='chandra'/>
            </xml>
        </xsl:if>
    </xsl:template>

    <xsl:template name="chandra">
        <xsl:for-each select="//item">
            <dataSet>
                <xsl:if test="../itunes:image/@href">
                    <directoryThumbnail><xsl:value-of select="normalize-space(../itunes:image/@href)"/></directoryThumbnail>
                </xsl:if>
                <xsl:element name="specialDirectories">
                    <xsl:element name="mostrecent">
                        <xsl:attribute name="dirname">Most Recent</xsl:attribute>
                        <xsl:attribute name="key"><xsl:value-of select="mnvXpath:pubDate(string(pubDate), '%a, %d %b %Y %H:%M:%S GMT', '%Y%m%dT%H:%M:%S')"/></xsl:attribute>
                        <xsl:attribute name="reverse">true</xsl:attribute>
                    </xsl:element>
                </xsl:element>
                <xsl:element name="item">
                    <title><xsl:value-of select="normalize-space(title)"/></title>
                    <author><xsl:value-of select="normalize-space(../itunes:author)"/></author>
                    <pubDate><xsl:value-of select="mnvXpath:pubDate(string(pubDate), '%a, %d %b %Y %H:%M:%S GMT')"/></pubDate>
                    <description><xsl:value-of select="normalize-space(description)"/></description>
                    <link><xsl:value-of select="concat('http://www.hd-trailers.net/mediaplayer/player.swf?autostart=true&amp;backcolor=000000&amp;frontcolor=999999&amp;lightcolor=000000&amp;screencolor=000000&amp;controlbar=over&amp;file=', normalize-space(enclosure/@url))"/></link>
                    <xsl:element name="media:group">
                        <xsl:element name="media:thumbnail">
                            <xsl:attribute name="url"><xsl:value-of select="normalize-space(.//media:thumbnail/@url)"/></xsl:attribute>
                        </xsl:element>
                        <xsl:element name="media:content">
                            <xsl:attribute name="url"><xsl:value-of select="normalize-space(media:content/@url)"/></xsl:attribute>
                            <xsl:attribute name="duration"><xsl:value-of select="normalize-space(media:content/@duration)"/></xsl:attribute>
                             <xsl:attribute name="length"><xsl:value-of select='normalize-space(media:content/@fileSize)'/></xsl:attribute>
                            <xsl:attribute name="lang"><xsl:value-of select="substring-before(normalize-space(../language), '-')"/></xsl:attribute>
                        </xsl:element>
                    </xsl:element>
                </xsl:element>
            </dataSet>
        </xsl:for-each>
    </xsl:template>
</xsl:stylesheet>
