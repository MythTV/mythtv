<?xml version="1.0" encoding="UTF-8"?>
<!--
    Blip.tv RSS entry conversion to MythNetvision item format
-->
<xsl:stylesheet version="1.0"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:creativeCommons="http://backend.userland.com/creativeCommonsRssModule"
    xmlns:media="http://search.yahoo.com/mrss/"
    xmlns:itunes="http://www.itunes.com/dtds/podcast-1.0.dtd"
    xmlns:geo="http://www.w3.org/2003/01/geo/wgs84_pos#"
    xmlns:blip="http://blip.tv/dtd/blip/1.0"
    xmlns:wfw="http://wellformedweb.org/CommentAPI/"
    xmlns:amp="http://www.adobe.com/amp/1.0"
    xmlns:dcterms="http://purl.org/dc/terms"
    xmlns:gm="http://www.google.com/schemas/gm/1.1"
    xmlns:mnvXpath="http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format"
    xmlns:mythtv="http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format"
    xmlns:mediaad="http://blip.tv/dtd/mediaad/1.0">

    <xsl:output method="xml" indent="yes" version="1.0" encoding="UTF-8" omit-xml-declaration="yes"/>

    <!--
        This template translates a single Blip.tv RSS item into a MNV item
    -->
    <xsl:template name="bliptvRSSItem">
        <xsl:element name="item">
            <title><xsl:value-of select="normalize-space(title)"/></title>
            <xsl:if test="../itunes:author">
                <author><xsl:value-of select="normalize-space(../itunes:author)"/></author>
            </xsl:if>
            <xsl:if test="not(../itunes:author) and ./blip:show">
                <author><xsl:value-of select="normalize-space(./blip:show)"/></author>
            </xsl:if>
            <pubDate><xsl:value-of select="mnvXpath:pubDate(string(pubDate), '%a, %d %b %Y %H:%M:%S')"/></pubDate>
            <description><xsl:value-of select="normalize-space(blip:puredescription)"/></description>
            <link><xsl:value-of select="mnvXpath:bliptvFlvLinkGeneration(.)"/></link>
            <xsl:element name="media:group">
                <xsl:element name="media:thumbnail">
                    <xsl:attribute name="url"><xsl:value-of select="normalize-space(.//media:thumbnail/@url)"/></xsl:attribute>
                </xsl:element>
                <xsl:element name="media:content">
                    <xsl:for-each select='mnvXpath:bliptvDownloadLinkGeneration(.)'>
                        <xsl:attribute name="url"><xsl:value-of select='normalize-space(./@url)'/></xsl:attribute>
                        <xsl:attribute name="width"><xsl:value-of select="normalize-space(./@width)"/></xsl:attribute>
                        <xsl:attribute name="height"><xsl:value-of select="normalize-space(./@height)"/></xsl:attribute>
                        <xsl:attribute name="length"><xsl:value-of select="normalize-space(./@length)"/></xsl:attribute>
                        <xsl:attribute name="duration"><xsl:value-of select="normalize-space(./@duration)"/></xsl:attribute>
                        <xsl:attribute name="lang"><xsl:value-of select="normalize-space(./@lang)"/></xsl:attribute>
                    </xsl:for-each>
                </xsl:element>
            </xsl:element>
            <xsl:if test="./blip:rating/text() != '0.0'">
                <rating><xsl:value-of select="normalize-space(./blip:rating)"/></rating>
            </xsl:if>
            <xsl:if test="string(mnvXpath:bliptvEpisode(./title/text())) != ''">
                <xsl:element name="mythtv:episode"><xsl:value-of select="normalize-space(mnvXpath:bliptvEpisode(./title/text()))"/></xsl:element>
            </xsl:if>
            <xsl:if test="mnvXpath:bliptvIsCustomHTML(.)">
                <mythtv:customhtml>true</mythtv:customhtml>
            </xsl:if>
        </xsl:element>
    </xsl:template>
</xsl:stylesheet>
