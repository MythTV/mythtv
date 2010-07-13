<?xml version="1.0" encoding="UTF-8"?>
<!--
    NASA items conversion to MythNetvision item format
-->
<xsl:stylesheet version="1.0"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:media="http://search.yahoo.com/mrss/"
    xmlns:mnvXpath="http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format"
    xmlns:mythtv="http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format"

    xmlns:itunes="http://www.itunes.com/dtds/podcast-1.0.dtd"
    xmlns:java_code="xalan://gov.nasa.build.Utils1">

    <xsl:param name="paraMeter" />

    <xsl:output method="xml" indent="yes" version="1.0" encoding="UTF-8" omit-xml-declaration="yes"/>

    <!--
        This template calls all other templates which allows for multiple sources to be processed
        within a single Xslt file
    -->
    <xsl:template match="/">
        <xml>
            <xsl:call-template name='nasa'/>
        </xml>
    </xsl:template>

    <xsl:template name="nasa">
        <xsl:for-each select="//item">
            <dataSet>
                <xsl:if test="../itunes:image/@href">
                    <xsl:choose>
                        <xsl:when test="string(../title) = 'NASA EDGE'">
                            <directoryThumbnail>http://www.atomic-clock.co.uk/images/logo/NasaLogo.jpg</directoryThumbnail>
                        </xsl:when>
                        <xsl:otherwise>
                            <directoryThumbnail><xsl:value-of select="normalize-space(../itunes:image/@href)"/></directoryThumbnail>
                        </xsl:otherwise>
                    </xsl:choose>
                </xsl:if>
                <xsl:element name="specialDirectories">
                    <xsl:element name="mostrecent">
                        <xsl:attribute name="dirname">Most Recent</xsl:attribute>
                        <xsl:attribute name="key"><xsl:value-of select="mnvXpath:pubDate(string(pubDate), '%a, %d %b %Y %H:%M:%S GMT', '%Y%m%dT%H:%M:%S')"/></xsl:attribute>
                        <xsl:attribute name="reverse">true</xsl:attribute>
                    </xsl:element>
                </xsl:element>
                <xsl:element name="item">
                    <xsl:copy-of select="mnvXpath:nasaTitleEp(string(title))/*"/>
                    <author><xsl:value-of select="normalize-space(../itunes:author)"/></author>
                    <pubDate><xsl:value-of select="mnvXpath:pubDate(string(pubDate), '%a, %d %b %Y %H:%M:%S GMT')"/></pubDate>
                    <description><xsl:value-of select="normalize-space(description)"/></description>
                    <xsl:choose>
                        <xsl:when test="$paraMeter = 'true'">
                            <link><xsl:value-of select="concat(mnvXpath:linkWebPage('nasa'), normalize-space(enclosure/@url))"/></link>
                            <mythtv:customhtml>true</mythtv:customhtml>
                        </xsl:when>
                        <xsl:otherwise>
                            <link><xsl:value-of select="normalize-space(link)"/></link>
                        </xsl:otherwise>
                    </xsl:choose>
                    <xsl:element name="media:group">
                        <xsl:element name="media:thumbnail">
                            <xsl:choose>
                                <xsl:when test="string(../title) = 'NASA EDGE'">
                                    <xsl:attribute name="url">http://www.atomic-clock.co.uk/images/logo/NasaLogo.jpg</xsl:attribute>
                                </xsl:when>
                                <xsl:otherwise>
                                    <xsl:attribute name="url"><xsl:value-of select="normalize-space(../itunes:image/@href)"/></xsl:attribute>
                                </xsl:otherwise>
                            </xsl:choose>
                        </xsl:element>
                        <xsl:element name="media:content">
                            <xsl:attribute name="url"><xsl:value-of select="normalize-space(enclosure/@url)"/></xsl:attribute>
                            <xsl:if test="enclosure/@length and enclosure/@length != '1234567'">
                                <xsl:attribute name="length"><xsl:value-of select='enclosure/@length'/></xsl:attribute>
                            </xsl:if>
                            <xsl:attribute name="lang"><xsl:value-of select="normalize-space(substring-before(string(../language), '-'))"/></xsl:attribute>
                        </xsl:element>
                    </xsl:element>
                </xsl:element>
            </dataSet>
        </xsl:for-each>
    </xsl:template>
</xsl:stylesheet>
