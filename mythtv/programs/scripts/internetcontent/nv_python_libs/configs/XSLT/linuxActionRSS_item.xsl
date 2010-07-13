<?xml version="1.0" encoding="UTF-8"?>
<!--
    The Linux Action Show! items conversion to MythNetvision item format
-->
<xsl:stylesheet version="1.0"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:atom="http://www.w3.org/2005/Atom"
    xmlns:itunes="http://www.itunes.com/dtds/podcast-1.0.dtd"
    xmlns:media="http://search.yahoo.com/mrss/"
    xmlns:feedburner="http://rssnamespace.org/feedburner/ext/1.0"
    xmlns:atom10="http://www.w3.org/2005/Atom"
    xmlns:mnvXpath="http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format"
    xmlns:mythtv="http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format"
	xmlns:xhtml="http://www.w3.org/1999/xhtml">

    <xsl:output method="xml" indent="yes" version="1.0" encoding="UTF-8" omit-xml-declaration="yes"/>

    <!--
        This template calls all other templates which allows for multiple sources to be processed
        within a single Xslt file
    -->
    <xsl:template match="/">
        <xsl:if test="//channel/link/text()='http://www.jupiterbroadcasting.com'">
            <xml>
                <xsl:call-template name='linuxAction'/>
            </xml>
        </xsl:if>
    </xsl:template>

    <xsl:template name="linuxAction">
        <xsl:for-each select="//item">
            <dataSet>
                <directoryThumbnail><xsl:value-of select="normalize-space(../itunes:image/@href)"/></directoryThumbnail>
                <xsl:element name="specialDirectories">
                    <xsl:element name="mostrecent">
                        <xsl:attribute name="dirname">Most Recent</xsl:attribute>
                        <xsl:attribute name="key"><xsl:value-of select="mnvXpath:pubDate(string(pubDate), '%a, %d %b %Y %H:%M:%S', '%Y%m%dT%H:%M:%S')"/></xsl:attribute>
                        <xsl:attribute name="reverse">true</xsl:attribute>
                        <xsl:attribute name="count">2</xsl:attribute>
                    </xsl:element>
                </xsl:element>
                <xsl:choose>
                    <xsl:when test="mnvXpath:linuxActioncheckIfDBItem(normalize-space(title), normalize-space(author) )">
                        <xsl:copy-of select="mnvXpath:getItemElement('dummy')" />
                    </xsl:when>
                    <xsl:otherwise>
                        <xsl:element name="item">
                            <xsl:for-each select="mnvXpath:linuxActionTitleSeEp(normalize-space(title))">
                                <xsl:copy-of select="." />
                            </xsl:for-each>
                            <author><xsl:value-of select="normalize-space(itunes:author)"/></author>
                            <pubDate><xsl:value-of select="mnvXpath:pubDate(string(pubDate), '%a, %d %b %Y %H:%M:%S')"/></pubDate>
                            <description><xsl:value-of select="normalize-space(mnvXpath:htmlToString(normalize-space(itunes:summary)))"/></description>
                            <link><xsl:value-of select="mnvXpath:linuxActionLinkGeneration(normalize-space(feedburner:origLink))"/></link>
                            <xsl:element name="media:group">
                                <xsl:element name="media:thumbnail">
                                    <xsl:attribute name="url"><xsl:value-of select="normalize-space(../itunes:image/@href)"/></xsl:attribute>
                                </xsl:element>
                                <xsl:element name="media:content">
                                    <xsl:attribute name="url"><xsl:value-of select="normalize-space(media:content/@url)"/></xsl:attribute>
                                    <xsl:attribute name="length"><xsl:value-of select="normalize-space(media:content/@fileSize)"/></xsl:attribute>
                                    <xsl:attribute name="lang"><xsl:value-of select="normalize-space(../language)"/></xsl:attribute>
                                </xsl:element>
                            </xsl:element>
                        </xsl:element>
                    </xsl:otherwise>
                </xsl:choose>
            </dataSet>
        </xsl:for-each>
    </xsl:template>

</xsl:stylesheet>
