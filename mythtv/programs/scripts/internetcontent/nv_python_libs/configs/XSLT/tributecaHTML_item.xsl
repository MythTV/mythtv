<?xml version="1.0" encoding="UTF-8"?>
<!--
    Tribute.ca items conversion to MythNetvision item format
-->
<xsl:stylesheet version="1.0"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:media="http://search.yahoo.com/mrss/"
    xmlns:itunes="http://www.itunes.com/dtds/podcast-1.0.dtd"
    xmlns:mnvXpath="http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format"
    xmlns:mythtv="http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format"
	xmlns:xhtml="http://www.w3.org/1999/xhtml">

    <xsl:output method="xml" indent="yes" version="1.0" encoding="UTF-8" omit-xml-declaration="yes"/>

    <!--
        This template calls all other templates which allows for multiple sources to be processed
        within a single Xslt file
    -->
    <xsl:template match="/">
        <xsl:if test="//a/@href='http://www.tribute.ca/'">
            <xml>
                <xsl:call-template name='featured'/>
                <xsl:call-template name='nowplaying'/>
                <xsl:call-template name='comingsoon'/>
                <xsl:call-template name='topupcoming'/>
                <xsl:call-template name='topboxoffice'/>
            </xml>
        </xsl:if>
    </xsl:template>

    <xsl:template name="featured">
        <xsl:element name="directory">
            <xsl:attribute name="name">
                <xsl:value-of select="string('Featured movie Trailers')"/>
            </xsl:attribute>
            <xsl:attribute name="thumbnail">
                <xsl:value-of select="string('http://www.tribute.ca/images/tribute_title.gif')"/>
            </xsl:attribute>
            <xsl:for-each select="//a[@class]/img[@src]/..">
                <dataSet>
                    <directoryThumbnail>http://www.tribute.ca/images/tribute_title.gif</directoryThumbnail>
                    <xsl:choose>
                        <xsl:when test="mnvXpath:tributecaCheckIfDBItem(normalize-space(string(.)), 'Tribute.ca', normalize-space(img/@alt))">
                            <xsl:copy-of select="mnvXpath:getItemElement('dummy')" />
                        </xsl:when>
                        <xsl:otherwise>
                            <xsl:element name="item">
                                <title><xsl:value-of select="normalize-space(string(.))"/></title>
                                <author>Tribute.ca</author>
                                <pubDate><xsl:value-of select="mnvXpath:pubDate('Now')"/></pubDate>
                                <description><xsl:value-of select="normalize-space(img/@alt)"/></description>
                                <link><xsl:value-of select="mnvXpath:tributecaLinkGeneration(position(), //a[@class]/img[@src]/..)"/></link>
                                <xsl:if test="mnvXpath:tributecaIsCustomHTML(('dummy'))">
                                    <mythtv:customhtml>true</mythtv:customhtml>
                                </xsl:if>
                                <xsl:element name="media:group">
                                    <xsl:element name="media:thumbnail">
                                        <xsl:attribute name="url"><xsl:value-of select="normalize-space(mnvXpath:tributecaThumbnailLink(string(.//img/@src)))"/></xsl:attribute>
                                    </xsl:element>
                                    <xsl:element name="media:content">
                                        <xsl:attribute name="url"><xsl:value-of select="mnvXpath:tributecaLinkGeneration(position(), //a[@class]/img[@src]/..)"/></xsl:attribute>
                                        <xsl:attribute name="duration"></xsl:attribute>
                                        <xsl:attribute name="width"></xsl:attribute>
                                        <xsl:attribute name="height"></xsl:attribute>
                                        <xsl:attribute name="length"></xsl:attribute>
                                        <xsl:attribute name="lang">en</xsl:attribute>
                                    </xsl:element>
                                </xsl:element>
                            </xsl:element>
                        </xsl:otherwise>
                    </xsl:choose>
                </dataSet>
            </xsl:for-each>
        </xsl:element>
    </xsl:template>

    <xsl:template name="nowplaying">
        <xsl:element name="directory">
            <xsl:attribute name="name"><xsl:value-of select="string('Movie Trailers Now Playing')"/></xsl:attribute>
            <xsl:attribute name="thumbnail"><xsl:value-of select="string('http://www.tribute.ca/images/tribute_title.gif')"/></xsl:attribute>
            <!-- ul = 1 for 'Movie Trailers Now Playing'-->
            <xsl:for-each select="mnvXpath:tributecaGetAnchors(//ul[@class='clump'], 1)">
                <dataSet>
                    <directoryThumbnail>http://www.tribute.ca/images/tribute_title.gif</directoryThumbnail>
                    <xsl:choose>
                        <xsl:when test="mnvXpath:tributecaCheckIfDBItem(normalize-space(string(.)), 'Tribute.ca', normalize-space(./@title))">
                            <xsl:copy-of select="mnvXpath:getItemElement('dummy')" />
                        </xsl:when>
                        <xsl:otherwise>
                            <xsl:element name="item">
                                <title><xsl:value-of select="normalize-space(string(.))"/></title>
                                <author>Tribute.ca</author>
                                <pubDate><xsl:value-of select="mnvXpath:pubDate('Now')"/></pubDate>
                                <description><xsl:value-of select="normalize-space(./@title)"/></description>
                                <link><xsl:value-of select="mnvXpath:tributecaLinkGeneration(position(), ../..//a)"/></link>
                                <xsl:if test="mnvXpath:tributecaIsCustomHTML(('dummy'))">
                                    <mythtv:customhtml>true</mythtv:customhtml>
                                </xsl:if>
                                <xsl:element name="media:group">
                                    <xsl:element name="media:thumbnail">
                                        <xsl:attribute name="url"><xsl:value-of select="mnvXpath:tributecaThumbnailLink(string(.))"/></xsl:attribute>
                                    </xsl:element>
                                    <xsl:element name="media:content">
                                        <xsl:attribute name="url"><xsl:value-of select="mnvXpath:tributecaLinkGeneration(position(), ../..//a)"/></xsl:attribute>
                                        <xsl:attribute name="duration"></xsl:attribute>
                                        <xsl:attribute name="width"></xsl:attribute>
                                        <xsl:attribute name="height"></xsl:attribute>
                                        <xsl:attribute name="length"></xsl:attribute>
                                        <xsl:attribute name="lang">en</xsl:attribute>
                                    </xsl:element>
                                </xsl:element>
                            </xsl:element>
                        </xsl:otherwise>
                    </xsl:choose>
                </dataSet>
            </xsl:for-each>
        </xsl:element>
    </xsl:template>

    <xsl:template name="comingsoon">
        <xsl:element name="directory">
            <xsl:attribute name="name"><xsl:value-of select="string('Coming Soon  Movie Trailers')"/></xsl:attribute>
            <xsl:attribute name="thumbnail"><xsl:value-of select="string('http://www.tribute.ca/images/tribute_title.gif')"/></xsl:attribute>
            <!-- ul = 3 for 'Coming Soon  Movie Trailers'-->
            <xsl:for-each select="mnvXpath:tributecaGetAnchors(//ul[@class='clump'], 3)">
                <dataSet>
                    <directoryThumbnail>http://www.tribute.ca/images/tribute_title.gif</directoryThumbnail>
                    <xsl:choose>
                        <xsl:when test="mnvXpath:tributecaCheckIfDBItem(normalize-space(string(.)), 'Tribute.ca', concat(normalize-space(string(.)), ' coming ', normalize-space(string(../small/i))))">
                            <xsl:copy-of select="mnvXpath:getItemElement('dummy')" />
                        </xsl:when>
                        <xsl:otherwise>
                            <xsl:element name="item">
                                <title><xsl:value-of select="normalize-space(string(.))"/></title>
                                <author>Tribute.ca</author>
                                <pubDate><xsl:value-of select="mnvXpath:pubDate('Now')"/></pubDate>
                                <description><xsl:value-of select="concat(normalize-space(string(.)), ' coming ', normalize-space(string(../small/i)))"/></description>
                                <link><xsl:value-of select="mnvXpath:tributecaLinkGeneration(position(), ../..//a)"/></link>
                                <xsl:if test="mnvXpath:tributecaIsCustomHTML(('dummy'))">
                                    <mythtv:customhtml>true</mythtv:customhtml>
                                </xsl:if>
                                <xsl:element name="media:group">
                                    <xsl:element name="media:thumbnail">
                                        <xsl:attribute name="url"><xsl:value-of select="mnvXpath:tributecaThumbnailLink(string(.))"/></xsl:attribute>
                                    </xsl:element>
                                    <xsl:element name="media:content">
                                        <xsl:attribute name="url"><xsl:value-of select="mnvXpath:tributecaLinkGeneration(position(), ../..//a)"/></xsl:attribute>
                                        <xsl:attribute name="duration"></xsl:attribute>
                                        <xsl:attribute name="width"></xsl:attribute>
                                        <xsl:attribute name="height"></xsl:attribute>
                                        <xsl:attribute name="length"></xsl:attribute>
                                        <xsl:attribute name="lang">en</xsl:attribute>
                                    </xsl:element>
                                </xsl:element>
                            </xsl:element>
                        </xsl:otherwise>
                    </xsl:choose>
                </dataSet>
            </xsl:for-each>
        </xsl:element>
    </xsl:template>

    <xsl:template name="topupcoming">
        <xsl:element name="directory">
            <xsl:attribute name="name"><xsl:value-of select="string('Top 10 Upcoming Trailers')"/></xsl:attribute>
            <xsl:attribute name="thumbnail"><xsl:value-of select="string('http://www.tribute.ca/images/tribute_title.gif')"/></xsl:attribute>
            <!-- ul = 2 for 'Top 10 Upcoming Trailers'-->
            <xsl:for-each select="mnvXpath:tributecaGetAnchors(//ul[@class='clump'], 2)">
                <dataSet>
                    <directoryThumbnail>http://www.tribute.ca/images/tribute_title.gif</directoryThumbnail>
                    <xsl:choose>
                        <xsl:when test="mnvXpath:tributecaCheckIfDBItem(normalize-space(string(.)), 'Tribute.ca', normalize-space(./@title))">
                            <xsl:copy-of select="mnvXpath:getItemElement('dummy')" />
                        </xsl:when>
                        <xsl:otherwise>
                            <xsl:element name="item">
                                <title><xsl:value-of select="normalize-space(string(.))"/></title>
                                <author>Tribute.ca</author>
                                <pubDate><xsl:value-of select="mnvXpath:pubDate('Now')"/></pubDate>
                                <description><xsl:value-of select="normalize-space(./@title)"/></description>
                                <link><xsl:value-of select="mnvXpath:tributecaLinkGeneration(position(), ../..//a)"/></link>
                                <xsl:if test="mnvXpath:tributecaIsCustomHTML(('dummy'))">
                                    <mythtv:customhtml>true</mythtv:customhtml>
                                </xsl:if>
                                <xsl:element name="media:group">
                                    <xsl:element name="media:thumbnail">
                                        <xsl:attribute name="url"><xsl:value-of select="mnvXpath:tributecaThumbnailLink(string(.))"/></xsl:attribute>
                                    </xsl:element>
                                    <xsl:element name="media:content">
                                        <xsl:attribute name="url"><xsl:value-of select="mnvXpath:tributecaLinkGeneration(position(), ../..//a)"/></xsl:attribute>
                                        <xsl:attribute name="duration"></xsl:attribute>
                                        <xsl:attribute name="width"></xsl:attribute>
                                        <xsl:attribute name="height"></xsl:attribute>
                                        <xsl:attribute name="length"></xsl:attribute>
                                        <xsl:attribute name="lang">en</xsl:attribute>
                                    </xsl:element>
                                </xsl:element>
                            </xsl:element>
                        </xsl:otherwise>
                    </xsl:choose>
                </dataSet>
            </xsl:for-each>
        </xsl:element>
    </xsl:template>

    <xsl:template name="topboxoffice">
        <xsl:element name="directory">
            <xsl:attribute name="name"><xsl:value-of select="string('Top Box Office Movie Trailers')"/></xsl:attribute>
            <xsl:attribute name="thumbnail"><xsl:value-of select="string('http://www.tribute.ca/images/tribute_title.gif')"/></xsl:attribute>
            <!-- ul = 0 for 'Top Box Office Movie Trailers'-->
            <xsl:for-each select="mnvXpath:tributecaGetAnchors(//ul[@class='clump'], 0)">
                <dataSet>
                    <directoryThumbnail>http://www.tribute.ca/images/tribute_title.gif</directoryThumbnail>
                    <xsl:choose>
                        <xsl:when test="mnvXpath:tributecaCheckIfDBItem(normalize-space(mnvXpath:tributecaTopTenTitle(string(..))), 'Tribute.ca', normalize-space(./@title))">
                            <xsl:copy-of select="mnvXpath:getItemElement('dummy')" />
                        </xsl:when>
                        <xsl:otherwise>
                            <xsl:element name="item">
                                <title><xsl:value-of select="normalize-space(mnvXpath:tributecaTopTenTitle(string(..)))"/></title>
                                <author>Tribute.ca</author>
                                <pubDate><xsl:value-of select="mnvXpath:pubDate('Now')"/></pubDate>
                                <description><xsl:value-of select="normalize-space(./@title)"/></description>
                                <link><xsl:value-of select="mnvXpath:tributecaLinkGeneration(position(), ..//a)"/></link>
                                <xsl:if test="mnvXpath:tributecaIsCustomHTML(('dummy'))">
                                    <mythtv:customhtml>true</mythtv:customhtml>
                                </xsl:if>
                                <xsl:element name="media:group">
                                    <xsl:element name="media:thumbnail">
                                        <xsl:attribute name="url"><xsl:value-of select="mnvXpath:tributecaThumbnailLink(string(.))"/></xsl:attribute>
                                    </xsl:element>
                                    <xsl:element name="media:content">
                                        <xsl:attribute name="url"><xsl:value-of select="mnvXpath:tributecaLinkGeneration(position(), ..//a)"/></xsl:attribute>
                                        <xsl:attribute name="duration"></xsl:attribute>
                                        <xsl:attribute name="width"></xsl:attribute>
                                        <xsl:attribute name="height"></xsl:attribute>
                                        <xsl:attribute name="length"></xsl:attribute>
                                        <xsl:attribute name="lang">en</xsl:attribute>
                                    </xsl:element>
                                </xsl:element>
                            </xsl:element>
                        </xsl:otherwise>
                    </xsl:choose>
                </dataSet>
            </xsl:for-each>
        </xsl:element>
    </xsl:template>

</xsl:stylesheet>
