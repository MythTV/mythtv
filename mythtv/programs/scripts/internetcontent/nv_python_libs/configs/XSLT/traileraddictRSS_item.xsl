<?xml version="1.0" encoding="UTF-8"?>
<!--
    TrailerAddict.Com items conversion to MythNetvision item format
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
        <xsl:if test="//channel/title='Movie Trailers at TrailerAddict.Com'">
            <xml>
                <xsl:call-template name='traileraddictMovie'/>
            </xml>
        </xsl:if>
        <xsl:if test="//channel/title='Movie Clips at TrailerAddict.Com'">
            <xml>
                <xsl:call-template name='traileraddictClip'/>
            </xml>
        </xsl:if>
    </xsl:template>

    <xsl:template name='traileraddictMovie'>
        <xsl:for-each select='//item'>
            <dataSet>
                <directoryThumbnail>http://www.traileraddict.com/images/widgettop.png</directoryThumbnail>
                <xsl:choose>
                    <xsl:when test="mnvXpath:traileraddictsCheckIfDBItem(normalize-space(title), 'TrailerAddict.Com', normalize-space(mnvXpath:htmlToString(string(description))))">
                        <xsl:copy-of select="mnvXpath:getItemElement('dummy')" />
                    </xsl:when>
                    <xsl:otherwise>
                        <xsl:element name="item">
                            <title><xsl:value-of select="normalize-space(title)"/></title>
                            <author>TrailerAddict.Com</author>
                            <pubDate><xsl:value-of select="mnvXpath:pubDate(string(pubDate), '%a, %d %b %Y %H:%M:%S')"/></pubDate>
                            <description><xsl:value-of select="normalize-space(mnvXpath:htmlToString(string(description)))"/></description>
                            <link><xsl:value-of select="mnvXpath:traileraddictsLinkGenerationMovie(position(), string(link))"/></link>
                            <xsl:element name="media:group">
                                <xsl:element name="media:thumbnail">
                                    <xsl:attribute name="url">http://www.traileraddict.com/images/widgettop.png</xsl:attribute>
                                </xsl:element>
                                <xsl:element name="media:content">
                                    <xsl:attribute name="url"><xsl:value-of select='mnvXpath:traileraddictsLinkGenerationMovie(position(), string(link))'/></xsl:attribute>
                                    <xsl:attribute name="duration"></xsl:attribute>
                                    <xsl:attribute name="width"></xsl:attribute>
                                    <xsl:attribute name="height"></xsl:attribute>
                                    <xsl:attribute name="length"></xsl:attribute>
                                    <xsl:attribute name="lang"><xsl:value-of select="normalize-space(substring-before(../language, '-'))"/></xsl:attribute>
                                </xsl:element>
                            </xsl:element>
                            <rating></rating>
                        </xsl:element>
                    </xsl:otherwise>
                </xsl:choose>
            </dataSet>
        </xsl:for-each>
    </xsl:template>

    <xsl:template name='traileraddictClip'>
        <xsl:for-each select='//item'>
            <dataSet>
                <directoryThumbnail>http://www.traileraddict.com/images/widgettop.png</directoryThumbnail>
                <xsl:choose>
                    <xsl:when test="mnvXpath:traileraddictsCheckIfDBItem(normalize-space(title), 'TrailerAddict.Com', normalize-space(mnvXpath:htmlToString(string(description))))">
                        <xsl:copy-of select="mnvXpath:getItemElement('dummy')" />
                    </xsl:when>
                    <xsl:otherwise>
                        <xsl:element name="item">
                            <title><xsl:value-of select="normalize-space(title)"/></title>
                            <author>TrailerAddict.Com</author>
                            <pubDate><xsl:value-of select="mnvXpath:pubDate(string(pubDate), '%a, %d %b %Y %H:%M:%S')"/></pubDate>
                            <description><xsl:value-of select="normalize-space(mnvXpath:htmlToString(string(description)))"/></description>
                            <link><xsl:value-of select="mnvXpath:traileraddictsLinkGenerationClip(position(), string(link))"/></link>
                            <xsl:element name="media:group">
                                <xsl:element name="media:thumbnail">
                                    <xsl:attribute name="url">http://www.traileraddict.com/images/widgettop.png</xsl:attribute>
                                </xsl:element>
                                <xsl:element name="media:content">
                                    <xsl:attribute name="url"><xsl:value-of select='mnvXpath:traileraddictsLinkGenerationClip(position(), string(link))'/></xsl:attribute>
                                    <xsl:attribute name="duration"></xsl:attribute>
                                    <xsl:attribute name="width"></xsl:attribute>
                                    <xsl:attribute name="height"></xsl:attribute>
                                    <xsl:attribute name="length"></xsl:attribute>
                                    <xsl:attribute name="lang"><xsl:value-of select="normalize-space(substring-before(../language, '-'))"/></xsl:attribute>
                                </xsl:element>
                            </xsl:element>
                            <rating></rating>
                        </xsl:element>
                    </xsl:otherwise>
                </xsl:choose>
            </dataSet>
        </xsl:for-each>
    </xsl:template>
</xsl:stylesheet>
