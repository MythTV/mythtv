<?xml version="1.0" encoding="UTF-8"?>
<!--
    YouTube Trailers RSS items conversion to MythNetvision item format
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
        This template calls all other templates which allows for multiple sources to be processed
        within a single Xslt file
    -->
    <xsl:template match="/">
        <xsl:if test="//atm:id/text()='http://gdata.youtube.com/feeds/api/videos'">
            <xml>
                <xsl:call-template name='youtubeTrailers'/>
            </xml>
        </xsl:if>
    </xsl:template>

    <xsl:template name="youtubeTrailers">
        <xsl:for-each select="mnvXpath:youtubeTrailerFilter(//atm:entry)">
            <xsl:call-template name='youtubeRSSItem'/>
        </xsl:for-each>
    </xsl:template>

    <!-- Generic XSLT template to transform YouTube RSS Atom feed entry elements to MNV items elements -->
    <xsl:include href="youtubeRSS_item.xsl"/>

</xsl:stylesheet>
