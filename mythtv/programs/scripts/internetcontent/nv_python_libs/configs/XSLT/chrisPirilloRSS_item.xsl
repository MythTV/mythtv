<?xml version="1.0" encoding="UTF-8"?>
<!--
    The Chris Pirillo Show items conversion to MythNetvision item format
-->
<xsl:stylesheet version="1.0"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:content="http://purl.org/rss/1.0/modules/content/"
    xmlns:wfw="http://wellformedweb.org/CommentAPI/"
    xmlns:dc="http://purl.org/dc/elements/1.1/"
    xmlns:atom="http://www.w3.org/2005/Atom"
    xmlns:sy="http://purl.org/rss/1.0/modules/syndication/"
    xmlns:slash="http://purl.org/rss/1.0/modules/slash/"
    xmlns:itunes="http://www.itunes.com/dtds/podcast-1.0.dtd"
    xmlns:media="http://search.yahoo.com/mrss/"
    xmlns:feedburner="http://rssnamespace.org/feedburner/ext/1.0"
    xmlns:atom10="http://www.w3.org/2005/Atom"
    xmlns:mnvXpath="http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format"
    xmlns:mythtv="http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format">

    <xsl:output method="xml" indent="yes" version="1.0" encoding="UTF-8" omit-xml-declaration="yes"/>

    <!--
        This template calls all other templates which allows for multiple sources to be processed
        within a single Xslt file
    -->
    <xsl:template match="/">
        <xsl:if test="//channel/link='http://chris.pirillo.com'">
            <xml>
                <xsl:call-template name='chrisPirillo'/>
            </xml>
        </xsl:if>
    </xsl:template>

    <xsl:template name='chrisPirillo'>
        <xsl:for-each select='//item[category/text() != "Partner"]'>
            <dataSet>
                <directoryThumbnail>http://chris.pirillo.com/images/heads/large/27.png</directoryThumbnail>
                <xsl:element name="specialDirectories">
                    <xsl:element name="mostrecent">
                        <xsl:attribute name="dirname">Most Recent</xsl:attribute>
                        <xsl:attribute name="key"><xsl:value-of select="mnvXpath:pubDate(mnvXpath:stringReplace(string(pubDate), 'GMT'), '%a, %d %b %Y %H:%M:%S', '%Y%m%dT%H:%M:%S')"/></xsl:attribute>
                        <xsl:attribute name="reverse">true</xsl:attribute>
                        <xsl:attribute name="count">3</xsl:attribute>
                    </xsl:element>
                </xsl:element>
                <xsl:element name="item">
                    <title><xsl:value-of select="normalize-space(title)"/></title>
                    <xsl:if test="itunes:author">
                        <author><xsl:value-of select="normalize-space(itunes:author)"/></author>
                    </xsl:if>
                    <pubDate><xsl:value-of select="mnvXpath:pubDate(mnvXpath:stringReplace(string(pubDate), 'GMT'), '%a, %d %b %Y %H:%M:%S')"/></pubDate>
                    <description><xsl:value-of select="normalize-space(substring-after(substring-before(mnvXpath:htmlToString(string(description)), 'Want to embed this video'), 'RSS Feed'))"/></description>
                    <link><xsl:value-of select="mnvXpath:chrisPirilloLinkGeneration(normalize-space(link), normalize-space(description))"/></link>
                    <xsl:element name="media:group">
                        <xsl:element name="media:thumbnail">
                            <xsl:attribute name="url">http://chris.pirillo.com/images/heads/large/27.png</xsl:attribute>
                        </xsl:element>
                        <xsl:element name="media:content">
                            <xsl:choose>
                                <xsl:when test="enclosure/@url">
                                    <xsl:attribute name="url"><xsl:value-of select='normalize-space(enclosure/@url)'/></xsl:attribute>
                                    <xsl:attribute name="length"><xsl:value-of select="normalize-space(enclosure/@length)"/></xsl:attribute>
                                </xsl:when>
                                <xsl:otherwise>
                                    <xsl:attribute name="url"><xsl:value-of select='mnvXpath:chrisPirilloLinkGeneration(normalize-space(link), normalize-space(description))'/></xsl:attribute>
                                </xsl:otherwise>
                            </xsl:choose>
                            <xsl:attribute name="lang"><xsl:value-of select="normalize-space(../language)"/></xsl:attribute>
                        </xsl:element>
                    </xsl:element>
                </xsl:element>
            </dataSet>
        </xsl:for-each>
    </xsl:template>

</xsl:stylesheet>
