<?xml version="1.0" encoding="UTF-8"?>
<!--
    Hubblecast items conversion to MythNetvision item format
-->
<xsl:stylesheet version="1.0"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:media="http://search.yahoo.com/mrss/"
    xmlns:mnvXpath="http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format"
    xmlns:mythtv="http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format"
    xmlns:itunes="http://www.itunes.com/dtds/podcast-1.0.dtd"
    xmlns:amp="http://www.adobe.com/amp/1.0"
    xmlns:atom="http://www.w3.org/2005/Atom"
    xmlns:atom10="http://www.w3.org/2005/Atom"
    xmlns:feedburner="http://rssnamespace.org/feedburner/ext/1.0" >

    <xsl:output method="xml" indent="yes" version="1.0" encoding="UTF-8" omit-xml-declaration="yes"/>

    <!--
        This template calls all other templates which allows for multiple sources to be processed
        within a single Xslt file
    -->
    <xsl:template match="/">
        <xsl:if test="//channel/link='http://www.spacetelescope.org/videos/hubblecast/'">
            <xml>
                <xsl:call-template name='hubblecast'/>
            </xml>
        </xsl:if>
    </xsl:template>

    <xsl:template name="hubblecast">
        <xsl:for-each select="//item">
            <dataSet>
                <xsl:if test="../itunes:image/@href">
                    <directoryThumbnail><xsl:value-of select="normalize-space(../itunes:image/@href)"/></directoryThumbnail>
                </xsl:if>
                <xsl:element name="specialDirectories">
                    <xsl:element name="mostrecent">
                        <xsl:attribute name="dirname">Most Recent</xsl:attribute>
                        <xsl:attribute name="key"><xsl:value-of select="mnvXpath:pubDate(string(pubDate), '%a, %d %b %Y %H:%M:%S', '%Y%m%dT%H:%M:%S')"/></xsl:attribute>
                        <xsl:attribute name="reverse">true</xsl:attribute>
                    </xsl:element>
                </xsl:element>
                <xsl:element name="item">
                    <xsl:if test="not(contains(string(title), 'Hubblecast '))">
                        <title><xsl:value-of select="normalize-space(title)"/></title>
                    </xsl:if>
                    <xsl:if test="contains(string(title), 'Hubblecast ')">
                        <xsl:element name="title"><xsl:value-of select="normalize-space(concat('EP', substring-after(string(title),'Hubblecast ')))"/></xsl:element>
                    </xsl:if>
                    <author><xsl:value-of select="normalize-space(itunes:author)"/></author>
                    <pubDate><xsl:value-of select="mnvXpath:pubDate(string(pubDate), '%a, %d %b %Y %H:%M:%S')"/></pubDate>
                    <description><xsl:value-of select="normalize-space(itunes:summary)"/></description>
                    <link><xsl:value-of select="mnvXpath:hubbleCastLinkGeneration(string(guid))"/></link>
                    <xsl:element name="media:group">
                        <xsl:element name="media:thumbnail">
                            <xsl:attribute name="url"><xsl:value-of select="normalize-space(media:thumbnail/@url)"/></xsl:attribute>
                        </xsl:element>
                        <xsl:element name="media:content">
                            <xsl:attribute name="url"><xsl:value-of select="normalize-space(enclosure/@url)"/></xsl:attribute>
                            <xsl:attribute name="duration"><xsl:value-of select="normalize-space(itunes:duration)"/></xsl:attribute>
                             <xsl:attribute name="length"><xsl:value-of select='normalize-space(enclosure/@length)'/></xsl:attribute>
                            <xsl:attribute name="lang"><xsl:value-of select="substring-before(normalize-space(../language), '-')"/></xsl:attribute>
                        </xsl:element>
                        <xsl:if test="contains(string(title), 'Hubblecast ')">
                            <xsl:element name="mythtv:episode"><xsl:value-of select="substring-before(substring-after(string(title),'Hubblecast '), ':')"/></xsl:element>
                        </xsl:if>
                    </xsl:element>
                    <mythtv:customhtml>true</mythtv:customhtml>
                </xsl:element>
            </dataSet>
        </xsl:for-each>
    </xsl:template>
</xsl:stylesheet>
