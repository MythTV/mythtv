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
        <xsl:if test="//Series">
            <metadata>
                <xsl:call-template name='tvdbCollection'/>
            </metadata>
        </xsl:if>
    </xsl:template>

    <xsl:template name="tvdbCollection">
        <xsl:for-each select="//Series">
            <item>
                <language><xsl:value-of select="normalize-space(Language)"/></language>
                <title><xsl:value-of select="normalize-space(SeriesName)"/></title>
                <xsl:if test="./Network/text() != ''">
                    <network><xsl:value-of select="normalize-space(Network)"/></network>
                </xsl:if>
                <xsl:if test="./Airs_DayOfWeek/text() != ''">
                    <airday><xsl:value-of select="normalize-space(Airs_DayOfWeek)"/></airday>
                </xsl:if>
                <xsl:if test="./Airs_Time/text() != ''">
                    <airtime><xsl:value-of select="normalize-space(Airs_Time)"/></airtime>
                </xsl:if>
                <xsl:if test="./Overview/text() != ''">
                    <description><xsl:value-of select="normalize-space(Overview)"/></description>
                </xsl:if>
<!--                <xsl:if test="tvdbXpath:getValue(//requestDetails, ./, 'Overview') != ''">-->
<!--                    <description><xsl:value-of select="normalize-space(tvdbXpath:htmlToString(tvdbXpath:getResult()))"/></description>-->
<!--                </xsl:if>-->
                <xsl:if test="./ContentRating/text() != ''">
                    <certifications>
                        <xsl:for-each select=".//ContentRating">
                            <xsl:element name="certification">
                                <xsl:attribute name="locale">us</xsl:attribute>
                                <xsl:attribute name="name"><xsl:value-of select="normalize-space(.)"/></xsl:attribute>
                            </xsl:element>
                        </xsl:for-each>
                    </certifications>
                </xsl:if>
                <xsl:if test="./Genre/text() != ''">
                    <categories>
                        <xsl:for-each select="tvdbXpath:stringToList(string(./Genre))">
                            <xsl:element name="category">
                                <xsl:attribute name="type">genre</xsl:attribute>
                                <xsl:attribute name="name"><xsl:value-of select="normalize-space(.)"/></xsl:attribute>
                            </xsl:element>
                        </xsl:for-each>
                    </categories>
                </xsl:if>
                <xsl:if test="./Network/text() != ''">
                    <studios>
                        <xsl:for-each select="./Network">
                            <xsl:element name="studio">
                                <xsl:attribute name="name"><xsl:value-of select="normalize-space(.)"/></xsl:attribute>
                            </xsl:element>
                        </xsl:for-each>
                    </studios>
                </xsl:if>
                <xsl:if test="./Runtime/text() != ''">
                    <runtime><xsl:value-of select="normalize-space(Runtime)"/></runtime>
                </xsl:if>
                <inetref><xsl:value-of select="normalize-space(id)"/></inetref>
                <xsl:if test="./IMDB_ID/text() != ''">
                    <imdb><xsl:value-of select="normalize-space(substring-after(string(IMDB_ID), 'tt'))"/></imdb>
                </xsl:if>
                <xsl:if test="./zap2it_id/text() != ''">
                    <tmsref><xsl:value-of select="normalize-space(zap2it_id)"/></tmsref>
                </xsl:if>
                <xsl:if test="./Rating/text() != ''">
                    <userrating><xsl:value-of select="normalize-space(Rating)"/></userrating>
                </xsl:if>
                <xsl:if test="./RatingCount/text() != ''">
                    <ratingcount><xsl:value-of select="normalize-space(RatingCount)"/></ratingcount>
                </xsl:if>
                <xsl:if test="./FirstAired/text() != ''">
                    <year><xsl:value-of select="normalize-space(substring-before(string(FirstAired), '-'))"/></year>
                    <releasedate><xsl:value-of select="normalize-space(FirstAired)"/></releasedate>
                </xsl:if>
                <xsl:if test="./lastupdated/text() != ''">
                    <lastupdated><xsl:value-of select="tvdbXpath:lastUpdated(string(./lastupdated))"/></lastupdated>
                </xsl:if>
                <xsl:if test="./Status/text() != ''">
                    <status><xsl:value-of select="normalize-space(Status)"/></status>
                </xsl:if>
                <xsl:if test=".//poster/text() != '' or .//fanart/text() != '' or .//banner/text() != ''">
                    <images>
                        <xsl:if test=".//poster/text() != ''">
                            <xsl:element name="image">
                                <xsl:attribute name="type">coverart</xsl:attribute>
                                <xsl:attribute name="url"><xsl:value-of select="concat('http://www.thetvdb.com/banners/', normalize-space(poster))"/></xsl:attribute>
                                <xsl:attribute name="thumb"><xsl:value-of select="concat('http://www.thetvdb.com/banners/_cache/', normalize-space(poster))"/></xsl:attribute>
                            </xsl:element>
                        </xsl:if>
                        <xsl:if test=".//fanart/text() != ''">
                            <xsl:element name="image">
                                <xsl:attribute name="type">fanart</xsl:attribute>
                                <xsl:attribute name="url"><xsl:value-of select="concat('http://www.thetvdb.com/banners/', normalize-space(fanart))"/></xsl:attribute>
                                <xsl:attribute name="thumb"><xsl:value-of select="concat('http://www.thetvdb.com/banners/_cache/', normalize-space(fanart))"/></xsl:attribute>
                            </xsl:element>
                        </xsl:if>
                        <xsl:if test=".//banner/text() != ''">
                            <xsl:element name="image">
                                <xsl:attribute name="type">banner</xsl:attribute>
                                <xsl:attribute name="url"><xsl:value-of select="concat('http://www.thetvdb.com/banners/', normalize-space(banner))"/></xsl:attribute>
                                <xsl:attribute name="thumb"><xsl:value-of select="concat('http://www.thetvdb.com/banners/_cache/', normalize-space(banner))"/></xsl:attribute>
                            </xsl:element>
                        </xsl:if>
                    </images>
                </xsl:if>
            </item>
        </xsl:for-each>
    </xsl:template>
</xsl:stylesheet>
