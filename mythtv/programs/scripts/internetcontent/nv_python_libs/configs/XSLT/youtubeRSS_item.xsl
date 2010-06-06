<?xml version="1.0" encoding="UTF-8"?>
<!--
    YouTube RSS entry conversion to MythNetvision item format
-->
<xsl:stylesheet version="1.0"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:atm='http://www.w3.org/2005/Atom'
    xmlns:app='http://purl.org/atom/app#'
    xmlns:media='http://search.yahoo.com/mrss/'
    xmlns:openSearch='http://a9.com/-/spec/opensearchrss/1.0/'
    xmlns:gd='http://schemas.google.com/g/2005'
    xmlns:yt='http://gdata.youtube.com/schemas/2007'
    xmlns:mnvXpath="http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format"
    xmlns:mythtv="http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format">

    <xsl:output method="xml" indent="yes" version="1.0" encoding="UTF-8" omit-xml-declaration="yes"/>

    <!--
        This template translates a single Youtube RSS atom entry into a MNV itme
    -->
    <xsl:template name="youtubeRSSItem">
        <xsl:element name="item">
            <title><xsl:value-of select="normalize-space(atm:title)"/></title>
            <author><xsl:value-of select="normalize-space(.//atm:name)"/></author>
            <pubDate><xsl:value-of select="mnvXpath:pubDate(string(atm:published), '%Y-%m-%dT%H:%M:%S.%fZ')"/></pubDate>
            <description><xsl:value-of select="normalize-space(atm:content)"/></description>
            <xsl:if test=".//media:content[@yt:format='5']">
                <link><xsl:value-of select="concat(string(.//media:content[@yt:format='5']/@url), '&amp;autoplay=1')"/></link>
            </xsl:if>
            <xsl:if test="not(.//media:content[@yt:format='5'])">
                <link><xsl:value-of select=".//media:player/@url"/></link>
            </xsl:if>
            <xsl:element name="media:group">
                <xsl:if test=".//media:thumbnail">
                    <!-- Get the largest thumbnail with is the last one available -->
                    <xsl:element name="media:thumbnail">
                        <xsl:attribute name="url"><xsl:value-of select="normalize-space(.//media:thumbnail[last()]/@url)"/></xsl:attribute>
                    </xsl:element>
                </xsl:if>
                <xsl:element name="media:content">
                    <xsl:if test=".//media:content[@yt:format='5']">
                        <xsl:attribute name="url"><xsl:value-of select="concat(string(.//media:content[@yt:format='5']/@url), '&amp;autoplay=1')"/></xsl:attribute>
                    </xsl:if>
                    <xsl:if test="not(.//media:content[@yt:format='5'])">
                        <xsl:attribute name="url"><xsl:value-of select=".//media:player/@url"/></xsl:attribute>
                    </xsl:if>
                    <xsl:if test=".//yt:duration">
                        <xsl:attribute name="duration"><xsl:value-of select=".//yt:duration/@seconds"/></xsl:attribute>
                    </xsl:if>
                </xsl:element>
            </xsl:element>
            <xsl:if test=".//gd:rating/@average">
                <rating><xsl:value-of select="normalize-space(.//gd:rating/@average)"/></rating>
            </xsl:if>
        </xsl:element>
    </xsl:template>
</xsl:stylesheet>
