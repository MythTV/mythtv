<?xml version="1.0" encoding="UTF-8"?>
<!--
    Earth-Touch items conversion to MythNetvision item format
-->
<xsl:stylesheet version="1.0"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:media="http://search.yahoo.com/mrss/"
    xmlns:itunes="http://www.itunes.com/dtds/podcast-1.0.dtd"
    xmlns:feedburner="http://rssnamespace.org/feedburner/ext/1.0"
    xmlns:mnvXpath="http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format">

    <xsl:output method="xml" indent="yes" version="1.0" encoding="UTF-8" omit-xml-declaration="yes"/>

    <!--
        This template calls all other templates which allows for multiple sources to be processed
        within a single Xslt file
    -->
    <xsl:template match="/">
        <xsl:if test="//channel/link='http://www.earth-touch.com/'">
            <xml>
                <xsl:call-template name='earth-touch'/>
            </xml>
        </xsl:if>
    </xsl:template>

    <xsl:template name='earth-touch'>
        <xsl:for-each select='//item'>
            <dataSet>

                <xsl:if test="../itunes:image/@href">
                    <xsl:if test="not(contains(mnvXpath:stringLower(..//image/title/text()), 'featured videos'))">
                        <directoryThumbnail><xsl:value-of select="normalize-space(../itunes:image/@href)"/></directoryThumbnail>
                    </xsl:if>
                    <xsl:if test="contains(mnvXpath:stringLower(..//image/title/text()), 'featured videos')">
                        <directoryThumbnail>%SHAREDIR%/mythnetvision/icons/earthtouch.png</directoryThumbnail>
                    </xsl:if>
                </xsl:if>
                <xsl:element name="specialDirectories">
                    <xsl:element name="mostrecent">
                        <xsl:attribute name="dirname">Most Recent</xsl:attribute>
                        <xsl:attribute name="key"><xsl:value-of select="mnvXpath:pubDate(string(pubDate), '%a, %d %b %Y %H:%M:%S', '%Y%m%dT%H:%M:%S')"/></xsl:attribute>
                        <xsl:attribute name="reverse">true</xsl:attribute>
                    </xsl:element>
                </xsl:element>
                <xsl:element name="item">
                    <title><xsl:value-of select="normalize-space(title)"/></title>
                    <author><xsl:value-of select="normalize-space(itunes:author)"/></author>
                    <pubDate><xsl:value-of select="mnvXpath:pubDate(string(pubDate), '%a, %d %b %Y %H:%M:%S')"/></pubDate>
                    <description><xsl:value-of select="normalize-space(itunes:subtitle)"/></description>
                    <link><xsl:value-of select="link"/></link>
                    <xsl:element name="media:group">
                        <xsl:element name="media:thumbnail">
                            <xsl:attribute name="url"><xsl:value-of select="normalize-space(mnvXpath:getHtmlData('//img/@src', string(description)))"/></xsl:attribute>
                        </xsl:element>
                        <xsl:element name="media:content">
                            <xsl:attribute name="url"><xsl:value-of select='media:content/@url'/></xsl:attribute>
                            <xsl:attribute name="duration"><xsl:value-of select="mnvXpath:convertDuration(string(itunes:duration))"/></xsl:attribute>
                            <xsl:attribute name="width"></xsl:attribute>
                            <xsl:attribute name="height"></xsl:attribute>
                            <xsl:attribute name="length"><xsl:value-of select='media:content/@fileSize'/></xsl:attribute>
                            <xsl:attribute name="lang"><xsl:value-of select="normalize-space(../language)"/></xsl:attribute>
                        </xsl:element>
                    </xsl:element>
                    <rating></rating>
                </xsl:element>
            </dataSet>
        </xsl:for-each>
    </xsl:template>
</xsl:stylesheet>
