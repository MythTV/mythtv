<?xml version="1.0" encoding="UTF-8"?>
<!--
    themoviedb.org Video data conversion to MythTV Universal Metadata Format
    See: http://www.mythtv.org/wiki/MythTV_Universal_Metadata_Format
-->
<xsl:stylesheet version="1.0"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:opensearch="http://a9.com/-/spec/opensearch/1.1/"
    xmlns:tmdbXpath="http://www.mythtv.org/wiki/MythTV_Universal_Metadata_Format">

    <xsl:output method="xml" indent="yes" version="1.0" encoding="UTF-8" omit-xml-declaration="yes"/>

    <!--
        This template calls all other templates which allows for multiple sources to be processed
        within a single Xslt file
    -->
    <xsl:template match="/">
        <xsl:if test="not(string(opensearch:totalResults)='0')">
            <metadata>
                <xsl:call-template name='movieVideoData'/>
            </metadata>
        </xsl:if>
    </xsl:template>

    <xsl:template name="movieVideoData">
        <xsl:for-each select="//movie">
            <item>
                <title><xsl:value-of select="normalize-space(substring-before(string(name), ':'))"/></title>
                <xsl:if test="substring-after(string(name), ':') != ''">
                    <subtitle><xsl:value-of select="normalize-space(substring-after(string(name), ':'))"/></subtitle>
                </xsl:if>
                <xsl:if test="string(tagline) != ''">
                    <tagline><xsl:value-of select="normalize-space(tagline)"/></tagline>
                </xsl:if>
                <language><xsl:value-of select="normalize-space(language)"/></language>
                <description><xsl:value-of select="normalize-space(overview)"/></description>
                <xsl:if test="string(season) != ''">
                    <season><xsl:value-of select="normalize-space(season)"/></season>
                </xsl:if>
                <xsl:if test="string(episode) != ''">
                    <episode><xsl:value-of select="normalize-space(episode)"/></episode>
                </xsl:if>
                <xsl:if test="./certification/text() != ''">
                    <certifications>
                        <xsl:for-each select=".//certification">
                            <xsl:element name="certification">
                                <xsl:attribute name="locale">us</xsl:attribute>
                                <xsl:attribute name="name"><xsl:value-of select="normalize-space(.)"/></xsl:attribute>
                            </xsl:element>
                        </xsl:for-each>
                    </certifications>
                </xsl:if>
                <xsl:if test=".//category">
                    <categories>
                        <xsl:for-each select=".//category">
                            <xsl:element name="category">
                                <xsl:attribute name="type"><xsl:value-of select="normalize-space(@type)"/></xsl:attribute>
                                <xsl:attribute name="name"><xsl:value-of select="normalize-space(@name)"/></xsl:attribute>
                            </xsl:element>
                        </xsl:for-each>
                    </categories>
                </xsl:if>
                <xsl:if test=".//studio">
                    <studios>
                        <xsl:for-each select=".//studio">
                            <xsl:element name="studio">
                                <xsl:attribute name="name"><xsl:value-of select="normalize-space(@name)"/></xsl:attribute>
                            </xsl:element>
                        </xsl:for-each>
                    </studios>
                </xsl:if>
                <xsl:if test=".//country">
                    <countries>
                        <xsl:for-each select=".//country">
                            <xsl:element name="country">
                                <xsl:attribute name="name"><xsl:value-of select="normalize-space(@name)"/></xsl:attribute>
                            </xsl:element>
                        </xsl:for-each>
                    </countries>
                </xsl:if>
                <xsl:if test="string(popularity) != ''">
                    <popularity><xsl:value-of select="normalize-space(popularity)"/></popularity>
                </xsl:if>
                <xsl:if test="string(rating) != ''">
                    <userrating><xsl:value-of select="normalize-space(rating)"/></userrating>
                </xsl:if>
                <xsl:if test="string(budget) != ''">
                    <budget><xsl:value-of select="normalize-space(budget)"/></budget>
                </xsl:if>
                <xsl:if test="string(revenue) != ''">
                    <revenue><xsl:value-of select="normalize-space(revenue)"/></revenue>
                </xsl:if>
                <xsl:if test="string(released) != ''">
                    <year><xsl:value-of select="normalize-space(substring(string(released), 1, 4))"/></year>
                    <releasedate><xsl:value-of select="normalize-space(released)"/></releasedate>
                </xsl:if>
                <lastupdated><xsl:value-of select="tmdbXpath:lastUpdated(string(last_modified_at), '%Y-%m-%d %H:%M:%S')"/></lastupdated> <!-- RFC-822 date-time -->
                <xsl:if test="string(runtime) != ''">
                    <runtime><xsl:value-of select="normalize-space(runtime)"/></runtime>
                </xsl:if>
                <inetref><xsl:value-of select="normalize-space(id)"/></inetref>
                <xsl:if test="string(imdb_id) != ''">
                    <imdb><xsl:value-of select="normalize-space(normalize-space(substring-after(string(imdb_id), 'tt')))"/></imdb> <!-- IMDB number -->
                </xsl:if>
                <homepage><xsl:value-of select="normalize-space(url)"/></homepage>
                <xsl:if test="string(trailer) != ''">
                    <trailer><xsl:value-of select="normalize-space(trailer)"/></trailer>
                </xsl:if>
                <xsl:if test="tmdbXpath:supportedJobs(.//person/@job)">
                    <people>
                        <xsl:for-each select=".//person">
                            <xsl:if test="tmdbXpath:supportedJobs((@job))">
                                <xsl:element name="person">
                                    <xsl:attribute name="name"><xsl:value-of select="normalize-space(@name)"/></xsl:attribute>
                                    <xsl:attribute name="job"><xsl:value-of select="normalize-space(@job)"/></xsl:attribute>
                                    <xsl:if test="@character != ''">
                                        <xsl:attribute name="character"><xsl:value-of select="normalize-space(@character)"/></xsl:attribute>
                                    </xsl:if>
                                    <xsl:if test="@thumb != ''">
                                        <xsl:attribute name="thumb"><xsl:value-of select="normalize-space(@thumb)"/></xsl:attribute>
                                    </xsl:if>
                                    <xsl:if test="@url != ''">
                                        <xsl:attribute name="url"><xsl:value-of select="normalize-space(@url)"/></xsl:attribute>
                                    </xsl:if>
                                    <xsl:if test="@department != ''">
                                        <xsl:attribute name="department"><xsl:value-of select="normalize-space(@department)"/></xsl:attribute>
                                    </xsl:if>
                                </xsl:element>
                            </xsl:if>
                        </xsl:for-each>
                    </people>
                </xsl:if>
                <xsl:if test="tmdbXpath:verifyName(.//image/@type)">
                    <images>
                        <xsl:for-each select="tmdbXpath:makeImageElements(.//image)">
                            <image>
                                <xsl:attribute name="type"><xsl:value-of select="normalize-space(./@type)"/></xsl:attribute>
                                <xsl:if test="./@thumb != ''">
                                    <xsl:attribute name="thumb"><xsl:value-of select="normalize-space(./@thumb)"/></xsl:attribute>
                                </xsl:if>
                                <xsl:if test="./@url != ''">
                                    <xsl:attribute name="url"><xsl:value-of select="normalize-space(./@url)"/></xsl:attribute>
                                </xsl:if>
                            </image>
                        </xsl:for-each>
                    </images>
                </xsl:if>
            </item>
        </xsl:for-each>
    </xsl:template>
</xsl:stylesheet>
