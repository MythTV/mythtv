<?xml version="1.0" encoding="UTF-8"?>
<!--
    Revision3 items conversion to MythNetvision item format
-->
<xsl:stylesheet version="1.0"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:itunes="http://www.itunes.com/dtds/podcast-1.0.dtd"
    xmlns:content="http://purl.org/rss/1.0/modules/content/"
    xmlns:cnettv="http://cnettv.com/mrss/"
    xmlns:creativeCommons="http://backend.userland.com/creativeCommonsRssModule"
    xmlns:media="http://search.yahoo.com/mrss/"
    xmlns:atom="http://www.w3.org/2005/Atom"
    xmlns:amp="http://www.adobe.com/amp/1.0"
    xmlns:mnvXpath="http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format"
    xmlns:mythtv="http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format">

    <xsl:output method="xml" indent="yes" version="1.0" encoding="UTF-8" omit-xml-declaration="yes"/>

    <!--
        This template calls all other templates which allows for multiple sources to be processed
        within a single Xslt file
    -->
    <xsl:template match="/">
        <xsl:if test="//channel/itunes:author/text()='Revision3'">
            <xml>
                <xsl:call-template name='rev3'/>
            </xml>
        </xsl:if>
    </xsl:template>

    <xsl:template name='rev3'>
        <xsl:for-each select='//item'>
            <dataSet>
                <directoryThumbnail><xsl:value-of select="normalize-space(../itunes:image/@href)"/></directoryThumbnail>
                <xsl:element name="specialDirectories">
                    <xsl:element name="mostrecent">
                        <xsl:attribute name="dirname">Most Recent</xsl:attribute>
                        <xsl:attribute name="key"><xsl:value-of select="mnvXpath:pubDate(string(pubDate), '%a, %d %b %Y %H:%M:%S GMT', '%Y%m%dT%H:%M:%S')"/></xsl:attribute>
                        <xsl:attribute name="reverse">true</xsl:attribute>
                        <xsl:attribute name="count">2</xsl:attribute>
                    </xsl:element>
                </xsl:element>
                <xsl:choose>
                    <xsl:when test="mnvXpath:revision3checkIfDBItem(.)">
                        <xsl:copy-of select="mnvXpath:getItemElement('dummy')" />
                    </xsl:when>
                    <xsl:otherwise>
                        <xsl:element name="item">
                            <xsl:for-each select='mnvXpath:revision3Episode(.)'>
                                <title><xsl:value-of select="normalize-space(./title)"/></title>
                                <xsl:if test="./episode">
                                    <xsl:element name="mythtv:episode"><xsl:value-of select="normalize-space(./episode)"/></xsl:element>
                                </xsl:if>
                            </xsl:for-each>
                            <xsl:if test="../itunes:author">
                                <author><xsl:value-of select="normalize-space(../itunes:author)"/></author>
                            </xsl:if>
                            <pubDate><xsl:value-of select="normalize-space(pubDate)"/></pubDate>
                            <description><xsl:value-of select="normalize-space(description)"/></description>
                            <link><xsl:value-of select="mnvXpath:revision3LinkGeneration(string(link))"/></link>
                            <xsl:element name="media:group">
                                <xsl:element name="media:thumbnail">
                                    <xsl:attribute name="url"><xsl:value-of select="normalize-space(.//media:thumbnail/@url)"/></xsl:attribute>
                                </xsl:element>
                                <xsl:element name="media:content">
                                    <xsl:attribute name="url"><xsl:value-of select='normalize-space(.//media:content/@url)'/></xsl:attribute>
                                    <xsl:if test=".//media:content/@duration">
                                        <xsl:attribute name="duration"><xsl:value-of select="normalize-space(.//media:content/@duration)"/></xsl:attribute>
                                    </xsl:if>
                                    <xsl:if test=".//media:content/@fileSize">
                                        <xsl:attribute name="length"><xsl:value-of select="normalize-space(.//media:content/@fileSize)"/></xsl:attribute>
                                    </xsl:if>
                                    <xsl:if test="../language">
                                        <xsl:attribute name="lang"><xsl:value-of select="normalize-space(substring-before(string(../language), '-'))"/></xsl:attribute>
                                    </xsl:if>
                                </xsl:element>
                            </xsl:element>
                        </xsl:element>
                    </xsl:otherwise>
                </xsl:choose>
            </dataSet>
        </xsl:for-each>
    </xsl:template>

</xsl:stylesheet>
