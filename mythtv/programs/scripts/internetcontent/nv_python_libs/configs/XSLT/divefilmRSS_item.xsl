<?xml version="1.0" encoding="UTF-8"?>
<!--
    DiveFilm HD items conversion to MythNetvision item format
-->
<xsl:stylesheet version="1.0"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:itunes="http://www.itunes.com/dtds/podcast-1.0.dtd"
    xmlns:atom="http://www.w3.org/2005/Atom"
    xmlns:media="http://search.yahoo.com/mrss/"
    xmlns:mnvXpath="http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format"
    xmlns:mythtv="http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format">

    <xsl:output method="xml" indent="yes" version="1.0" encoding="UTF-8" omit-xml-declaration="yes"/>

    <!--
        This template calls all other templates which allows for multiple sources to be processed
        within a single Xslt file
    -->
    <xsl:template match="/">
        <xsl:if test="//channel/link='http://divefilm.com'">
            <xml>
                <xsl:call-template name='divefilmhd'/>
            </xml>
        </xsl:if>
    </xsl:template>

    <xsl:template name="divefilmhd">
        <xsl:for-each select="//item">
            <xsl:if test="not(starts-with(normalize-space(title), 'iPod -'))">
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
                        <xsl:if test="contains(string(guid), 'episode')">
                            <xsl:element name="title"><xsl:value-of select="concat('Ep', substring-after(mnvXpath:getSeasonEpisode(string(guid)),'_'), ' ', normalize-space(substring-after(title, 'HD -')))"/></xsl:element>
                        </xsl:if>
                        <xsl:if test="not(contains(string(guid), 'episode'))">
                            <title><xsl:value-of select="normalize-space(substring-after(title, 'HD -'))"/></title>
                        </xsl:if>
                        <author><xsl:value-of select="normalize-space(itunes:author)"/></author>
                        <pubDate><xsl:value-of select="mnvXpath:pubDate(string(pubDate), '%a, %d %b %Y %H:%M:%S')"/></pubDate>
                        <description><xsl:value-of select="normalize-space(itunes:subtitle)"/></description>
                        <!-- There is no Web page link the media must played througha flash player -->
                        <link><xsl:value-of select="concat('http://static.hd-trailers.net/mediaplayer/player.swf?autostart=true&amp;file=', normalize-space(guid))"/></link>
                        <xsl:element name="media:group">
                            <xsl:element name="media:thumbnail">
                                <xsl:attribute name="url"><xsl:value-of select="normalize-space(../itunes:image/@href)"/></xsl:attribute>
                            </xsl:element>
                            <xsl:element name="media:content">
                                <xsl:attribute name="url"><xsl:value-of select="guid"/></xsl:attribute>
                                <xsl:attribute name="duration"><xsl:value-of select="mnvXpath:convertDuration(string(itunes:duration))"/></xsl:attribute>
                                <xsl:if test="enclosure/@length">
                                    <xsl:attribute name="length"><xsl:value-of select='enclosure/@length'/></xsl:attribute>
                                </xsl:if>
                                <xsl:attribute name="lang"><xsl:value-of select="normalize-space(../language)"/></xsl:attribute>
                            </xsl:element>
                        </xsl:element>
                        <rating></rating>
                        <xsl:if test="contains(string(guid), 'episode')">
                            <xsl:element name="mythtv:episode"><xsl:value-of select="substring-after(mnvXpath:getSeasonEpisode(string(guid)),'_')"/></xsl:element>
                        </xsl:if>
                    </xsl:element>
                </dataSet>
            </xsl:if>
        </xsl:for-each>
    </xsl:template>
</xsl:stylesheet>
