<?xml version="1.0" encoding="UTF-8"?>
<!--
    PBS Videos items conversion to MythNetvision item format
-->
<xsl:stylesheet version="1.0"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:media="http://search.yahoo.com/mrss/"
    xmlns:mnvXpath="http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format"
    xmlns:mythtv="http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format">

    <xsl:output method="xml" indent="yes" version="1.0" encoding="UTF-8" omit-xml-declaration="yes"/>

    <!--
        This template calls all other templates which allows for multiple sources to be processed
        within a single Xslt file
    -->
    <xsl:template match="/">
        <xsl:if test="//channel/link='http://video.pbs.org/'">
            <xml>
                <xsl:call-template name='pbsVideo'/>
            </xml>
        </xsl:if>
    </xsl:template>

    <xsl:template name="pbsVideo">
        <xsl:for-each select="//item">
            <dataSet>
                <directoryThumbnail>%SHAREDIR%/mythnetvision/icons/pbs.png</directoryThumbnail>
                <xsl:element name="specialDirectories">
                    <xsl:element name="mostrecent">
                        <xsl:attribute name="dirname">Most Recent</xsl:attribute>
                        <xsl:attribute name="key"><xsl:value-of select="mnvXpath:pubDate(string(pubDate), '%a, %d %b %Y %H:%M:%S', '%Y%m%dT%H:%M:%S')"/></xsl:attribute>
                        <xsl:attribute name="reverse">true</xsl:attribute>
                    </xsl:element>
                </xsl:element>
                <xsl:element name="item">
                    <xsl:copy-of select="mnvXpath:pbsTitleSeriesEpisodeLink(normalize-space(./title),normalize-space(./link))/*" />
                    <author>Public Broadcasting Service (PBS)</author>
                    <pubDate><xsl:value-of select="mnvXpath:pubDate(string(pubDate), '%a, %d %b %Y %H:%M:%S')"/></pubDate>
                    <description><xsl:value-of select="normalize-space(media:description)"/></description>
                    <xsl:element name="media:group">
                        <xsl:element name="media:thumbnail">
                            <xsl:if test="media:thumbnail/@url">
                                <xsl:attribute name="url"><xsl:value-of select="normalize-space(media:thumbnail/@url)"/></xsl:attribute>
                            </xsl:if>
                        </xsl:element>
                        <xsl:element name="media:content">
                            <xsl:attribute name="url"><xsl:value-of select="mnvXpath:pbsDownloadlink(normalize-space(./link))"/></xsl:attribute>
                            <xsl:attribute name="duration"><xsl:value-of select="mnvXpath:pbsDuration(normalize-space(media:content/@duration))"/></xsl:attribute>
                            <xsl:attribute name="lang"><xsl:value-of select="substring-before(normalize-space(../language), '-')"/></xsl:attribute>
                        </xsl:element>
                    </xsl:element>
                    <mythtv:customhtml>true</mythtv:customhtml>
                </xsl:element>
            </dataSet>
        </xsl:for-each>
    </xsl:template>

</xsl:stylesheet>
