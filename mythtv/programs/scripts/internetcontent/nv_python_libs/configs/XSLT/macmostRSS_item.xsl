<?xml version="1.0" encoding="UTF-8"?>
<!--
    MacMost Tech & Gadgets items conversion to MythNetvision item format
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
        This template calls all other templates which allows for multiple sources to be processed
        within a single Xslt file
    -->
    <xsl:template match="/">
        <xsl:if test="//channel/link='http://macmost.com'">
            <xml>
                <xsl:call-template name='macmost'/>
            </xml>
        </xsl:if>
    </xsl:template>

    <xsl:template name='macmost'>
        <xsl:for-each select='//item'>
            <dataSet>
                <directoryThumbnail><xsl:value-of select="normalize-space(../image/url)"/></directoryThumbnail>
                <xsl:element name="specialDirectories">
                    <xsl:element name="mostrecent">
                        <xsl:attribute name="dirname">Most Recent</xsl:attribute>
                        <xsl:attribute name="key"><xsl:value-of select="mnvXpath:pubDate(string(pubDate), '%a, %d %b %Y %H:%M:%S', '%Y%m%dT%H:%M:%S')"/></xsl:attribute>
                        <xsl:attribute name="reverse">true</xsl:attribute>
                    </xsl:element>
                </xsl:element>
                <xsl:call-template name='bliptvRSSItem'/>
            </dataSet>
        </xsl:for-each>
    </xsl:template>

    <!-- Generic XSLT template to transform Blip.tv RSS item elements to MNV items elements -->
    <xsl:include href="bliptvRSS_item.xsl"/>

</xsl:stylesheet>
