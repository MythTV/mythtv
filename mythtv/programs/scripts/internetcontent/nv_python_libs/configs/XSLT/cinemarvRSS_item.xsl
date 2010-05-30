<?xml version="1.0" encoding="UTF-8"?>
<!--
    CinemaRV.com items conversion to MythNetvision item format
-->
<xsl:stylesheet version="1.0"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:media="http://search.yahoo.com/mrss/"
    xmlns:itunes="http://www.itunes.com/dtds/podcast-1.0.dtd"
    xmlns:mnvXpath="http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format"
    xmlns:mythtv="http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format"
	xmlns:content="http://purl.org/rss/1.0/modules/content/"
	xmlns:wfw="http://wellformedweb.org/CommentAPI/"
	xmlns:dc="http://purl.org/dc/elements/1.1/"
	xmlns:atom="http://www.w3.org/2005/Atom"
	xmlns:sy="http://purl.org/rss/1.0/modules/syndication/"
	xmlns:slash="http://purl.org/rss/1.0/modules/slash/">

    <xsl:output method="xml" indent="yes" version="1.0" encoding="UTF-8" omit-xml-declaration="yes"/>

    <!--
        This template calls all other templates which allows for multiple sources to be processed
        within a single Xslt file
    -->
    <xsl:template match="/">
        <xsl:if test="//channel/link='http://cinemarv.com'">
            <xml>
                <xsl:call-template name='cinemarv'/>
            </xml>
        </xsl:if>
    </xsl:template>

    <xsl:template name="cinemarv">
        <xsl:for-each select="//item">
            <dataSet>
                <xsl:if test="../itunes:image/@href">
                    <directoryThumbnail><xsl:value-of select="normalize-space(../itunes:image/@href)"/></directoryThumbnail>
                </xsl:if>
                <xsl:element name="specialDirectories">
                    <xsl:element name="mostrecent">
                        <xsl:attribute name="dirname">Most Recent</xsl:attribute>
                        <xsl:attribute name="count">2</xsl:attribute>
                        <xsl:attribute name="key"><xsl:value-of select="mnvXpath:pubDate(string(pubDate), '%a, %d %B %Y %H:%M:%S', '%Y%m%dT%H:%M:%S')"/></xsl:attribute>
                        <xsl:attribute name="reverse">true</xsl:attribute>
                    </xsl:element>
                </xsl:element>
                <xsl:element name="item">
                    <xsl:if test="contains(string(title), 'Trailer')">
                        <title><xsl:value-of select="normalize-space(substring-before(string(title), 'Trailer'))"/></title>
                    </xsl:if>
                    <xsl:if test="not(contains(string(title), 'Trailer'))">
                        <title><xsl:value-of select='normalize-space(title)'/></title>
                    </xsl:if>
                    <author><xsl:value-of select="normalize-space(dc:creator)"/></author>
                    <pubDate><xsl:value-of select="mnvXpath:pubDate(string(pubDate), '%a, %d %B %Y %H:%M:%S')"/></pubDate>
                    <description><xsl:value-of select="normalize-space(description)"/></description>
                    <link><xsl:value-of select="mnvXpath:cinemarvLinkGeneration(string(link))"/></link>
                    <xsl:if test="mnvXpath:cinemarvIsCustomHTML(('dummy'))">
                        <mythtv:customhtml>true</mythtv:customhtml>
                    </xsl:if>
                    <xsl:element name="media:group">
                        <xsl:element name="media:thumbnail">
                            <xsl:attribute name="url"><xsl:value-of select="normalize-space('%SHAREDIR%/mythnetvision/icons/trailers.png')"/></xsl:attribute>
                        </xsl:element>
                        <xsl:element name="media:content">
                            <xsl:attribute name="url"><xsl:value-of select="mnvXpath:cinemarvLinkGeneration(string(link))"/></xsl:attribute>
                            <xsl:attribute name="duration"></xsl:attribute>
                            <xsl:attribute name="width"></xsl:attribute>
                            <xsl:attribute name="height"></xsl:attribute>
                             <xsl:attribute name="length"></xsl:attribute>
                            <xsl:attribute name="lang"><xsl:value-of select="normalize-space(../language)"/></xsl:attribute>
                        </xsl:element>
                    </xsl:element>
                    <rating></rating>
                </xsl:element>
            </dataSet>
        </xsl:for-each>
    </xsl:template>
</xsl:stylesheet>
