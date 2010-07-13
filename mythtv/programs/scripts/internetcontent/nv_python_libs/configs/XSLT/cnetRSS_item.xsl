<?xml version="1.0" encoding="UTF-8"?>
<!--
    CNet items conversion to MythNetvision item format
-->
<xsl:stylesheet version="1.0"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:itunes="http://www.itunes.com/dtds/podcast-1.0.dtd"
    xmlns:atom="http://www.w3.org/2005/Atom"
    xmlns:media="http://search.yahoo.com/mrss/"
    xmlns:feedburner="http://rssnamespace.org/feedburner/ext/1.0"
    xmlns:mnvXpath="http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format"
    xmlns:mythtv="http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format">

    <xsl:output method="xml" indent="yes" version="1.0" encoding="UTF-8" omit-xml-declaration="yes"/>

    <!--
        This template calls all other templates which allows for multiple sources to be processed
        within a single Xslt file
    -->
    <xsl:template match="/">
        <xsl:if test="//channel/link='http://cnettv.cnet.com/'">
            <xml>
                <xsl:call-template name='cnet'/>
            </xml>
        </xsl:if>
    </xsl:template>

    <xsl:template name="cnet">
        <xsl:for-each select="//item">
            <dataSet>
                <xsl:if test="..//itunes:image/@href">
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
                    <title><xsl:value-of select="normalize-space(substring-after(string(title), ':'))"/></title>
                    <author><xsl:value-of select="normalize-space(itunes:author)"/></author>
                    <pubDate><xsl:value-of select="string(pubDate)"/></pubDate>
                    <description><xsl:value-of select="normalize-space(itunes:summary)"/></description>
                    <!--
                        Commented out but left in just in case the fullscreen autoplayer fails. The
                        commented out alternative link only creates a Web page link.
                    -->
<!--                    <link><xsl:value-of select="mnvXpath:stringReplace('http://news.cnet.com/1606-2_3-WEBCODE.html', 'WEBCODE', string(guid))"/></link>-->
                    <link><xsl:value-of select="concat('http://www.hd-trailers.net/mediaplayer/player.swf?autostart=true&amp;file=', normalize-space(link))"/></link>
                    <xsl:element name="media:group">
                        <xsl:element name="media:thumbnail">
                            <xsl:if test="..//itunes:image/@href">
                                <xsl:attribute name="url"><xsl:value-of select="normalize-space(../itunes:image/@href)"/></xsl:attribute>
                            </xsl:if>
                        </xsl:element>
                        <xsl:element name="media:content">
                            <xsl:attribute name="url"><xsl:value-of select="normalize-space(link)"/></xsl:attribute>
                            <xsl:attribute name="duration"><xsl:value-of select="normalize-space(itunes:duration)"/></xsl:attribute>
                            <xsl:attribute name="lang"><xsl:value-of select="substring-before(normalize-space(../language), '-')"/></xsl:attribute>
                        </xsl:element>
                    </xsl:element>
                </xsl:element>
            </dataSet>
        </xsl:for-each>
    </xsl:template>
</xsl:stylesheet>
