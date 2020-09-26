<?xml version="1.0" encoding="UTF-8"?>
<!--
    thetvdb.com query result conversion to MythTV Universal Metadata Format
    See: http://www.mythtv.org/wiki/MythTV_Universal_Metadata_Format
-->
<xsl:stylesheet version="1.0"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:tvdbXpath="http://www.mythtv.org/wiki/MythTV_Universal_Metadata_Format">

    <xsl:output method="xml" indent="yes" version="1.0" encoding="UTF-8" omit-xml-declaration="yes"/>

    <!--
        This template calls all other templates which allows for multiple sources to be processed
        within a single Xslt file
    -->
    <xsl:template match="/">
        <xsl:if test="//series">
            <metadata>
                <xsl:call-template name='tvdbQuery'/>
            </metadata>
        </xsl:if>
    </xsl:template>

    <xsl:template name="tvdbQuery">
        <xsl:for-each select="//series">
            <item>
                <language><xsl:value-of select="normalize-space(language)"/></language>
                <title><xsl:value-of select="normalize-space(seriesName)"/></title>
                <inetref><xsl:value-of select="normalize-space(id)"/></inetref>
                <collectionref><xsl:value-of select="normalize-space(id)"/></collectionref>
                <xsl:if test="./IMDB_ID/text() != ''">
                    <imdb><xsl:value-of select="normalize-space(substring-after(string(IMDB_ID), 'tt'))"/></imdb>
                </xsl:if>
                <xsl:if test="./zap2it_id/text() != ''">
                    <tmsref><xsl:value-of select="normalize-space(zap2it_id)"/></tmsref>
                </xsl:if>
                <xsl:if test="./rating/text() != ''">
                    <userrating><xsl:value-of select="normalize-space(rating)"/></userrating>
                </xsl:if>
                <xsl:if test="./overview/text() != ''">
                    <description><xsl:value-of select="normalize-space(overview)"/></description>
                </xsl:if>
                <xsl:if test="./firstAired/text() != ''">
                    <releasedate><xsl:value-of select="normalize-space(firstAired)"/></releasedate>
                </xsl:if>
                <xsl:if test=".//poster/text() != '' or .//fanart/text() != '' or .//banner/text() != ''">
                    <images>
                        <xsl:if test=".//poster/text() != ''">
                            <xsl:element name="image">
                                <xsl:attribute name="type">coverart</xsl:attribute>
                                <xsl:attribute name="url"><xsl:value-of select="concat('http://www.thetvdb.com', normalize-space(poster))"/></xsl:attribute>
                                <xsl:attribute name="thumb"><xsl:value-of select="tvdbXpath:replace(concat('http://www.thetvdb.com', normalize-space(poster)), '/banners/', '/banners/_cache/')"/></xsl:attribute>
                            </xsl:element>
                        </xsl:if>
                        <xsl:if test=".//fanart/text() != ''">
                            <xsl:element name="image">
                                <xsl:attribute name="type">fanart</xsl:attribute>
                                <xsl:attribute name="url"><xsl:value-of select="concat('http://www.thetvdb.com', normalize-space(fanart))"/></xsl:attribute>
                                <xsl:attribute name="thumb"><xsl:value-of select="tvdbXpath:replace(concat('http://www.thetvdb.com', normalize-space(fanart)), '/banners/', '/banners/_cache/')"/></xsl:attribute>
                            </xsl:element>
                        </xsl:if>
                        <xsl:if test=".//banner/text() != ''">
                            <xsl:element name="image">
                                <xsl:attribute name="type">banner</xsl:attribute>
                                <xsl:attribute name="url"><xsl:value-of select="concat('http://www.thetvdb.com', normalize-space(banner))"/></xsl:attribute>
                                <xsl:attribute name="thumb"><xsl:value-of select="tvdbXpath:replace(concat('http://www.thetvdb.com', normalize-space(banner)), '/banners/', '/banners/_cache/')"/></xsl:attribute>
                            </xsl:element>
                        </xsl:if>
                    </images>
                </xsl:if>
            </item>
        </xsl:for-each>
    </xsl:template>
</xsl:stylesheet>
