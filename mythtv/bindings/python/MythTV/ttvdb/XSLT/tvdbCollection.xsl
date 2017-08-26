<?xml version="1.0" encoding="UTF-8"?>
<!--
    thetvdb.com series collection result conversion to MythTV Universal Metadata Format
    See: http://www.mythtv.org/wiki/MythTV_Universal_Metadata_Format
-->
<xsl:stylesheet version="1.0"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:opensearch="http://a9.com/-/spec/opensearch/1.1/"
    xmlns:tvdbXpath="http://www.mythtv.org/wiki/MythTV_Universal_Metadata_Format"
    >

    <xsl:output method="xml" indent="yes" version="1.0" encoding="UTF-8" omit-xml-declaration="yes"/>

    <!--
        This template calls all other templates which allows for multiple sources to be processed
        within a single Xslt file
    -->
    <xsl:template match="/">
        <xsl:if test="/data/series">
            <metadata>
                <xsl:call-template name='tvdbCollection'/>
            </metadata>
        </xsl:if>
    </xsl:template>

    <xsl:template name="tvdbCollection">
        <xsl:for-each select="/data/series">
            <item>
                <language><xsl:value-of select="normalize-space(language)"/></language>
                <title><xsl:value-of select="normalize-space(seriesName)"/></title>
                <xsl:if test="./network/text() != ''">
                    <network><xsl:value-of select="normalize-space(network)"/></network>
                </xsl:if>
                <xsl:if test="./airsDayOfWeek/text() != ''">
                    <airday><xsl:value-of select="normalize-space(airsDayOfWeek)"/></airday>
                </xsl:if>
                <xsl:if test="./airsTime/text() != ''">
                    <airtime><xsl:value-of select="normalize-space(airsTime)"/></airtime>
                </xsl:if>
                <xsl:if test="./overview/text() != ''">
                    <description><xsl:value-of select="normalize-space(overview)"/></description>
                </xsl:if>
<!--                <xsl:if test="tvdbXpath:getValue(//requestDetails, ./, 'Overview') != ''">-->
<!--                    <description><xsl:value-of select="normalize-space(tvdbXpath:htmlToString(tvdbXpath:getResult()))"/></description>-->
<!--                </xsl:if>-->
                <xsl:if test="./rating/text() != ''">
                    <certifications>
                        <xsl:for-each select=".//rating">
                            <xsl:element name="certification">
                                <xsl:attribute name="locale">us</xsl:attribute>
                                <xsl:attribute name="name"><xsl:value-of select="normalize-space(.)"/></xsl:attribute>
                            </xsl:element>
                        </xsl:for-each>
                    </certifications>
                </xsl:if>
                <xsl:if test="./genre/text() != ''">
                    <categories>
                        <xsl:for-each select="tvdbXpath:stringToList(string(./genre))">
                            <xsl:element name="category">
                                <xsl:attribute name="type">genre</xsl:attribute>
                                <xsl:attribute name="name"><xsl:value-of select="normalize-space(.)"/></xsl:attribute>
                            </xsl:element>
                        </xsl:for-each>
                    </categories>
                </xsl:if>
                <xsl:if test="./network/text() != ''">
                    <studios>
                        <xsl:for-each select="./network">
                            <xsl:element name="studio">
                                <xsl:attribute name="name"><xsl:value-of select="normalize-space(.)"/></xsl:attribute>
                            </xsl:element>
                        </xsl:for-each>
                    </studios>
                </xsl:if>
                <xsl:if test="./runtime/text() != ''">
                    <runtime><xsl:value-of select="normalize-space(runtime)"/></runtime>
                </xsl:if>
                <inetref><xsl:value-of select="normalize-space(id)"/></inetref>
                <xsl:if test="./imdbId/text() != ''">
                    <imdb><xsl:value-of select="normalize-space(substring-after(string(imdbId), 'tt'))"/></imdb>
                </xsl:if>
                <xsl:if test="./zap2it_id/text() != ''">
                    <tmsref><xsl:value-of select="normalize-space(zap2it_id)"/></tmsref>
                </xsl:if>
                <xsl:if test="./siteRating/text() != ''">
                    <userrating><xsl:value-of select="normalize-space(siteRating)"/></userrating>
                </xsl:if>
                <xsl:if test="./siteRatingCount/text() != ''">
                    <ratingcount><xsl:value-of select="normalize-space(siteRatingCount)"/></ratingcount>
                </xsl:if>
                <xsl:if test="./firstAired/text() != ''">
                    <year><xsl:value-of select="normalize-space(substring-before(string(firstAired), '-'))"/></year>
                    <releasedate><xsl:value-of select="normalize-space(firstAired)"/></releasedate>
                </xsl:if>
                <xsl:if test="./lastUpdated/text() != ''">
                    <lastupdated><xsl:value-of select="tvdbXpath:lastUpdated(string(./lastUpdated))"/></lastupdated>
                </xsl:if>
                <xsl:if test="./status/text() != ''">
                    <status><xsl:value-of select="normalize-space(status)"/></status>
                </xsl:if>
                <xsl:if test="./poster/text() != '' or ./fanart/text() != '' or ./banner/text() != '' or .//_banners/poster/raw or .//_banners/fanart/raw">
                    <images>
                        <xsl:choose>
                            <xsl:when test="./poster/text() != ''">
                                <xsl:element name="image">
                                    <xsl:attribute name="type">coverart</xsl:attribute>
                                    <xsl:attribute name="url"><xsl:value-of select="normalize-space(poster)"/></xsl:attribute>
                                    <xsl:attribute name="thumb"><xsl:value-of select="normalize-space(tvdbXpath:replace(string(poster), 'banners', 'banners/_cache'))"/></xsl:attribute>
                                </xsl:element>
                            </xsl:when>
                            <xsl:when test="./_banners/seasonswide/raw">
                                <xsl:element name="image">
                                    <xsl:attribute name="type">coverart</xsl:attribute>
                                    <xsl:attribute name="url"><xsl:value-of select="concat('http://www.thetvdb.com/banners/', normalize-space(_banners/seasonswide/raw/item[1]/fileName))"/></xsl:attribute>
                                    <xsl:attribute name="thumb"><xsl:value-of select="concat('http://www.thetvdb.com/banners/', normalize-space(_banners/seasonswide/raw/item[1]/thumbnail))"/></xsl:attribute>
                                </xsl:element>
                            </xsl:when>
                            <xsl:when test="./_banners/poster/raw">
                                <xsl:element name="image">
                                    <xsl:attribute name="type">coverart</xsl:attribute>
                                    <xsl:attribute name="url"><xsl:value-of select="concat('http://www.thetvdb.com/banners/', normalize-space(_banners/poster/raw/item[1]/fileName))"/></xsl:attribute>
                                    <xsl:attribute name="thumb"><xsl:value-of select="concat('http://www.thetvdb.com/banners/', normalize-space(_banners/poster/raw/item[1]/thumbnail))"/></xsl:attribute>
                                </xsl:element>
                            </xsl:when>
                        </xsl:choose>
                        <xsl:choose>
                            <xsl:when test="./fanart/text() != ''">
                                <xsl:element name="image">
                                    <xsl:attribute name="type">fanart</xsl:attribute>
                                    <xsl:attribute name="url"><xsl:value-of select="normalize-space(fanart)"/></xsl:attribute>
                                    <xsl:attribute name="thumb"><xsl:value-of select="normalize-space(tvdbXpath:replace(string(fanart), 'banners', 'banners/_cache'))"/></xsl:attribute>
                                </xsl:element>
                            </xsl:when>
                            <xsl:when test="./_banners/fanart/raw">
                                <xsl:element name="image">
                                    <xsl:attribute name="type">fanart</xsl:attribute>
                                    <xsl:attribute name="url"><xsl:value-of select="concat('http://www.thetvdb.com/banners/', normalize-space(_banners/fanart/raw/item[1]/fileName))"/></xsl:attribute>
                                    <xsl:attribute name="thumb"><xsl:value-of select="concat('http://www.thetvdb.com/banners/', normalize-space(_banners/fanart/raw/item[1]/thumbnail))"/></xsl:attribute>
                                </xsl:element>
                            </xsl:when>
                        </xsl:choose>
                        <xsl:if test="./banner/text() != ''">
                            <xsl:element name="image">
                                <xsl:attribute name="type">banner</xsl:attribute>
                                <xsl:attribute name="url"><xsl:value-of select="normalize-space(banner)"/></xsl:attribute>
                                <xsl:attribute name="thumb"><xsl:value-of select="normalize-space(tvdbXpath:replace(string(banner), 'banners', 'banners/_cache'))"/></xsl:attribute>
                            </xsl:element>
                        </xsl:if>
                    </images>
                </xsl:if>
            </item>
        </xsl:for-each>
    </xsl:template>
</xsl:stylesheet>
