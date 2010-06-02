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
        <xsl:if test="//Series">
            <metadata>
                <xsl:call-template name='tvdbQuery'/>
            </metadata>
        </xsl:if>
    </xsl:template>

    <xsl:template name="tvdbQuery">
        <xsl:for-each select="//Series">
            <item>
                <language><xsl:value-of select="normalize-space(language)"/></language>
                <title><xsl:value-of select="normalize-space(SeriesName)"/></title>
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
                <xsl:if test="./Overview/text() != ''">
                    <description><xsl:value-of select="normalize-space(Overview)"/></description>
                </xsl:if>
                <xsl:if test="./FirstAired/text() != ''">
                    <releasedate><xsl:value-of select="normalize-space(FirstAired)"/></releasedate>
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
