<?xml version="1.0" encoding="UTF-8"?>
<!--
    api.giantbomb.com Game data conversion to MythTV Universal Metadata Format
    See: http://www.mythtv.org/wiki/MythTV_Universal_Metadata_Format
-->
<xsl:stylesheet version="1.0"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:gamebombXpath="http://www.mythtv.org/wiki/MythTV_Universal_Metadata_Format">

    <xsl:output method="xml" indent="yes" version="1.0" encoding="UTF-8" omit-xml-declaration="yes"/>

    <!--
        This template calls all other templates which allows for multiple sources to be processed
        within a single Xslt file
    -->
    <xsl:template match="/">
        <xsl:if test="not(string(/response/limit)='0')">
            <metadata>
                <xsl:call-template name='giantbombGameData'/>
            </metadata>
        </xsl:if>
    </xsl:template>

    <xsl:template name="giantbombGameData">
        <xsl:for-each select="//results">
            <item>
                <language>en</language>
                <title><xsl:value-of select="normalize-space(name)"/></title>
                <xsl:if test="./deck/text() != ''">
                    <description><xsl:value-of select="normalize-space(gamebombXpath:htmlToString(string(deck)))"/></description>
                </xsl:if>
                <xsl:if test="./deck/text() = ''">
                    <description><xsl:value-of select="normalize-space(gamebombXpath:htmlToString(string(description)))"/></description>
                </xsl:if>
                <xsl:if test="./original_game_rating/text() != ''">
                    <certifications>
                        <!--
                            This code will need to be modified when multiple country cerification
                            information is supported. Right now it assumes ONLY ratings from the US.
                        -->
                        <xsl:for-each select=".//original_game_rating">
                            <xsl:element name="certification">
                                <xsl:attribute name="locale">us</xsl:attribute>
                                <xsl:attribute name="name"><xsl:value-of select="normalize-space(.)"/></xsl:attribute>
                            </xsl:element>
                        </xsl:for-each>
                    </certifications>
                </xsl:if>
                <xsl:if test=".//genre">
                    <categories>
                        <xsl:for-each select=".//genre">
                            <xsl:element name="category">
                                <xsl:attribute name="type"><xsl:value-of select="normalize-space('genre')"/></xsl:attribute>
                                <xsl:attribute name="name"><xsl:value-of select="normalize-space(./name)"/></xsl:attribute>
                            </xsl:element>
                        </xsl:for-each>
                    </categories>
                </xsl:if>
                <xsl:if test=".//platform">
                    <systems>
                        <xsl:for-each select=".//platform">
                            <system><xsl:value-of select="normalize-space(./name)"/></system>
                        </xsl:for-each>
                    </systems>
                </xsl:if>
                <xsl:if test=".//company">
                    <studios>
                        <xsl:for-each select=".//company">
                            <xsl:element name="studio">
                                <xsl:attribute name="name"><xsl:value-of select="normalize-space(./name)"/></xsl:attribute>
                            </xsl:element>
                        </xsl:for-each>
                    </studios>
                </xsl:if>
                <!-- Input format: 2001-10-23 00:00:00 -->
                <xsl:if test="./original_release_date/text() != ''">
                    <releasedate><xsl:value-of select="gamebombXpath:pubDate(string(./original_release_date), '%Y-%m-%d %H:%M:%S', '%Y-%m-%d')"/></releasedate>
                </xsl:if>
                <xsl:if test="./original_release_date/text() = ''">
                    <xsl:if test="gamebombXpath:futureReleaseDate(.) != ''">
                        <releasedate><xsl:value-of select="gamebombXpath:futureReleaseDate(.)"/></releasedate>
                    </xsl:if>
                </xsl:if>
                <!--Input format: 2010-05-10 16:59:49.972923 -->
                <xsl:if test="./date_last_updated/text() != ''">
                    <lastupdated><xsl:value-of select="gamebombXpath:pubDate(substring-before(./date_last_updated, '.'), '%Y-%m-%d %H:%M:%S')"/></lastupdated>
                </xsl:if>
                <inetref><xsl:value-of select="normalize-space(id)"/></inetref>
                <xsl:if test="./site_detail_url">
                    <homepage><xsl:value-of select="normalize-space(./site_detail_url)"/></homepage>
                </xsl:if>
                <xsl:if test=".//video/url">
                    <trailer><xsl:value-of select="normalize-space(.//video/url[0])"/></trailer>
                </xsl:if>
                <xsl:if test="gamebombXpath:supportedJobs(.//person/role/text())">
                    <people>
                        <xsl:for-each select=".//person">
                            <xsl:if test="gamebombXpath:supportedJobs((string(./role)))">
                                <xsl:element name="person">
                                    <xsl:attribute name="name"><xsl:value-of select="normalize-space(./name)"/></xsl:attribute>
                                    <xsl:attribute name="job"><xsl:value-of select="normalize-space(gamebombXpath:translateName(string(./role)))"/></xsl:attribute>
                                </xsl:element>
                            </xsl:if>
                        </xsl:for-each>
                    </people>
                </xsl:if>
                <xsl:if test="gamebombXpath:findImages(./image, description, .//images/image)">
                    <images>
                        <xsl:for-each select="gamebombXpath:getImages('dummy')">
                            <xsl:element name="image">
                                <xsl:attribute name="type"><xsl:value-of select="normalize-space(./@type)"/></xsl:attribute>
                                <xsl:attribute name="url"><xsl:value-of select="normalize-space(./@url)"/></xsl:attribute>
                                <xsl:attribute name="thumb"><xsl:value-of select="normalize-space(./@thumb)"/></xsl:attribute>
                            </xsl:element>
                        </xsl:for-each>
                    </images>
                </xsl:if>
            </item>
        </xsl:for-each>
    </xsl:template>
</xsl:stylesheet>
