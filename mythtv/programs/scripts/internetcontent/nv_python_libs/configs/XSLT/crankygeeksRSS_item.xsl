<?xml version="1.0" encoding="UTF-8"?>
<!--
    CrankyGeeks items conversion to MythNetvision item format
-->
<xsl:stylesheet version="1.0"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:itunes="http://www.itunes.com/dtds/podcast-1.0.dtd"
    xmlns:media="http://search.yahoo.com/mrss/"
    xmlns:mnvXpath="http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format"
    xmlns:mythtv="http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format">

    <xsl:output method="xml" indent="yes" version="1.0" encoding="UTF-8" omit-xml-declaration="yes"/>

    <!--
        This template calls all other templates which allows for multiple sources to be processed
        within a single Xslt file
    -->
    <xsl:template match="/">
        <xsl:if test="//channel/link='http://www.mevio.com/shows/?show=crankygeeks'">
            <xml>
                <xsl:call-template name='crankygeeks'/>
            </xml>
        </xsl:if>
    </xsl:template>

    <xsl:template name='crankygeeks'>
        <xsl:for-each select='//item[enclosure/@url]'>
            <dataSet>
                <directoryThumbnail><xsl:value-of select="normalize-space(../itunes:image/@href)"/></directoryThumbnail>
                <xsl:element name="specialDirectories">
                    <xsl:element name="mostrecent">
                        <xsl:attribute name="dirname">Most Recent</xsl:attribute>
                        <xsl:attribute name="key"><xsl:value-of select="mnvXpath:pubDate(string(pubDate), '%a, %d %b %Y %H:%M:%S', '%Y%m%dT%H:%M:%S')"/></xsl:attribute>
                        <xsl:attribute name="reverse">true</xsl:attribute>
                        <xsl:attribute name="count">3</xsl:attribute>
                    </xsl:element>
                </xsl:element>
                <xsl:choose>
                    <xsl:when test="mnvXpath:mevioCheckIfDBItem(mnvXpath:mevioTitle(./title/text()), normalize-space(itunes:summary) )">
                        <xsl:copy-of select="mnvXpath:getItemElement('dummy')" />
                    </xsl:when>
                    <xsl:otherwise>
                        <xsl:element name="item">
                            <title><xsl:value-of select="mnvXpath:mevioTitle(./title/text())"/></title>
                            <xsl:if test="../itunes:author">
                                <author><xsl:value-of select="normalize-space(../itunes:author)"/></author>
                            </xsl:if>
                            <pubDate><xsl:value-of select="mnvXpath:pubDate(string(pubDate), '%a, %d %b %Y %H:%M:%S')"/></pubDate>
                            <description><xsl:value-of select="normalize-space(itunes:summary)"/></description>
                            <link><xsl:value-of select="mnvXpath:mevioLinkGeneration(string(link))"/></link>
                            <xsl:element name="media:group">
                                <xsl:element name="media:thumbnail">
                                    <xsl:attribute name="url"><xsl:value-of select="normalize-space(../itunes:image/@href)"/></xsl:attribute>
                                </xsl:element>
                                <xsl:element name="media:content">
                                    <xsl:attribute name="url"><xsl:value-of select='normalize-space(enclosure/@url)'/></xsl:attribute>
                                    <xsl:if test="enclosure/@length">
                                        <xsl:attribute name="length"><xsl:value-of select="normalize-space(enclosure/@length)"/></xsl:attribute>
                                    </xsl:if>
                                    <xsl:attribute name="lang"><xsl:value-of select="normalize-space(../language)"/></xsl:attribute>
                                </xsl:element>
                            </xsl:element>
                            <xsl:if test="string(mnvXpath:mevioEpisode(./title/text())) != ''">
                                <xsl:element name="mythtv:episode"><xsl:value-of select="normalize-space(mnvXpath:mevioEpisode(./title/text()))"/></xsl:element>
                            </xsl:if>
                            <xsl:if test="enclosure/@url">
                                <mythtv:customhtml>true</mythtv:customhtml>
                            </xsl:if>
                        </xsl:element>
                    </xsl:otherwise>
                </xsl:choose>
            </dataSet>
        </xsl:for-each>
    </xsl:template>

</xsl:stylesheet>
