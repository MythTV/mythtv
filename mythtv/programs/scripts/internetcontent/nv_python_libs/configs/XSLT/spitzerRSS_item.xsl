<?xml version="1.0" encoding="UTF-8"?>
<!--
    NASA's Spitzer Space Telescope items conversion to MythNetvision item format
-->
<xsl:stylesheet version="1.0"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:media="http://search.yahoo.com/mrss/"
    xmlns:itunes="http://www.itunes.com/dtds/podcast-1.0.dtd"
    xmlns:mnvXpath="http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format"
    xmlns:mythtv="http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format">

    <xsl:param name="paraMeter" />

    <xsl:output method="xml" indent="yes" version="1.0" encoding="UTF-8" omit-xml-declaration="yes"/>

    <!--
        This template calls all other templates which allows for multiple sources to be processed
        within a single Xslt file
    -->
    <xsl:template match="/">
        <xml>
            <xsl:call-template name='spitzer'/>
        </xml>
    </xsl:template>

    <xsl:template name="spitzer">
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
                <xsl:choose>
                    <xsl:when test="mnvXpath:spitzerCheckIfDBItem(normalize-space(title), normalize-space(itunes:author), mnvXpath:htmlToString(normalize-space(description)))">
                        <xsl:copy-of select="mnvXpath:getItemElement('dummy')" />
                    </xsl:when>
                    <xsl:otherwise>
                        <xsl:element name="item">
                            <title><xsl:value-of select="normalize-space(title)"/></title>
                            <author><xsl:value-of select="normalize-space(itunes:author)"/></author>
                            <pubDate><xsl:value-of select="mnvXpath:pubDate(string(pubDate), '%a, %d %b %Y %H:%M:%S')"/></pubDate>
                            <description><xsl:value-of select="mnvXpath:htmlToString(normalize-space(description))"/></description>
                            <link><xsl:value-of select="mnvXpath:spitzerLinkGeneration(normalize-space(link), $paraMeter)"/></link>
                            <xsl:element name="media:group">
                                <xsl:choose>
                                    <xsl:when test="mnvXpath:spitzerThumbnailLink('dummy') != ''">
                                        <xsl:element name="media:thumbnail">
                                            <xsl:attribute name="url"><xsl:value-of select="mnvXpath:spitzerThumbnailLink('dummy')"/></xsl:attribute>
                                        </xsl:element>
                                    </xsl:when>
                                    <xsl:otherwise>
                                        <xsl:element name="media:thumbnail">
                                            <xsl:attribute name="url"><xsl:value-of select="normalize-space(../itunes:image/@href)"/></xsl:attribute>
                                        </xsl:element>
                                    </xsl:otherwise>
                                </xsl:choose>
                                <xsl:element name="media:content">
                                    <xsl:attribute name="url"><xsl:value-of select="normalize-space(enclosure/@url)"/></xsl:attribute>
                                    <xsl:attribute name="lang"><xsl:value-of select="substring-before(normalize-space(../language), '-')"/></xsl:attribute>
                                </xsl:element>
                            </xsl:element>
                        </xsl:element>
                    </xsl:otherwise>
                </xsl:choose>
            </dataSet>
        </xsl:for-each>
    </xsl:template>
</xsl:stylesheet>
