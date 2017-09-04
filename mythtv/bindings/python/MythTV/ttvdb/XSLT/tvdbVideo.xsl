<?xml version="1.0" encoding="UTF-8"?>
<!--
    thetvdb.com Video data conversion to MythTV Universal Metadata Format
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
        <xsl:if test="//data/series">
            <metadata>
                <xsl:call-template name='tvdbVideoData'/>
            </metadata>
        </xsl:if>
    </xsl:template>

    <xsl:template name="tvdbVideoData">
        <xsl:for-each select="//data/series">
            <item>
                <title><xsl:value-of select="normalize-space(seriesName)"/></title>
                <xsl:if test="tvdbXpath:getValue(//requestDetails, //data, 'subtitle') != ''">
                    <subtitle><xsl:value-of select="normalize-space(tvdbXpath:getResult())"/></subtitle>
                </xsl:if>
                <xsl:if test="tvdbXpath:getValue(//requestDetails, //data/series, 'description') != ''">
                    <description><xsl:value-of select="normalize-space(tvdbXpath:htmlToString(tvdbXpath:getResult()))"/></description>
                </xsl:if>
                <season><xsl:value-of select="normalize-space(//requestDetails/@season)"/></season>
                <episode><xsl:value-of select="normalize-space(//requestDetails/@episode)"/></episode>
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
                <xsl:if test="./genre/item != ''">
                    <categories>
                        <xsl:for-each select="./genre/item">
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
                <collectionref><xsl:value-of select="normalize-space(id)"/></collectionref>
                <xsl:if test="./imdbId/text() != '' and tvdbXpath:getValue(//requestDetails, //data, 'IMDB') = ''">
                    <imdb><xsl:value-of select="normalize-space(substring-after(string(imdbId), 'tt'))"/></imdb>
                </xsl:if>
                <xsl:if test="./zap2itId/text() != ''">
                    <tmsref><xsl:value-of select="normalize-space(zap2itId)"/></tmsref>
                </xsl:if>
                <xsl:for-each select="tvdbXpath:getValue(//requestDetails, //data, 'allEpisodes', 'allresults')">
                    <xsl:if test="./imdbId/text() != ''">
                        <imdb><xsl:value-of select="normalize-space(substring-after(string(zap2itId), 'tt'))"/></imdb>
                    </xsl:if>
                    <xsl:if test="./rating/text() != ''">
                        <userrating><xsl:value-of select="normalize-space(./rating)"/></userrating>
                    </xsl:if>
                    <xsl:if test="./language/overview/text() != ''">
                        <language><xsl:value-of select="normalize-space(./language/overview)"/></language>
                    </xsl:if>
                    <xsl:if test="./firstAired/text() != ''">
                        <year><xsl:value-of select="normalize-space(substring(string(./firstAired), 1, 4))"/></year>
                        <releasedate><xsl:value-of select="normalize-space(./firstAired)"/></releasedate>
                    </xsl:if>
                    <xsl:if test="./lastupdated/text() != ''">
                        <lastupdated><xsl:value-of select="tvdbXpath:lastUpdated(string(./lastupdated))"/></lastupdated>
                    </xsl:if>
                    <xsl:if test="./DVD_season/text() != ''">
                        <dvdseason><xsl:value-of select="normalize-space(./DVD_season)"/></dvdseason>
                    </xsl:if>
                    <xsl:if test="./DVD_episodenumber/text() != ''">
                        <dvdepisode><xsl:value-of select="normalize-space(./DVD_episodenumber)"/></dvdepisode>
                    </xsl:if>
                    <xsl:if test="./seriesid/text() != '' and ./seasonid/text() != ''">
                        <homepage><xsl:value-of select="normalize-space(concat('http://thetvdb.com/?tab=episode&amp;seriesid=', string(./seriesid), '&amp;seasonid=', string(./seasonid), '&amp;id=', string(./id)))"/></homepage>
                    </xsl:if>
                    <xsl:if test="//data/series/_actors/actor or .//guestStars/text() != '' or .//director/text() != '' or .//writer/text() != ''">
                        <people>
                            <xsl:for-each select="//data/series/_actors/actor">
                                <xsl:element name="person">
                                    <xsl:attribute name="job">Actor</xsl:attribute>
                                    <xsl:attribute name="name"><xsl:value-of select="normalize-space(name)"/></xsl:attribute>
                                    <xsl:attribute name="character"><xsl:value-of select="normalize-space(role)"/></xsl:attribute>
                                    <xsl:if test="./image/text() != ''">
                                        <xsl:attribute name="url"><xsl:value-of select="normalize-space(image)"/></xsl:attribute>
                                        <xsl:attribute name="thumb"><xsl:value-of select="normalize-space(image)"/></xsl:attribute>
                                    </xsl:if>
                                </xsl:element>
                            </xsl:for-each>
                            <xsl:for-each select="./guestStars/item">
                                <xsl:element name="person">
                                    <xsl:attribute name="job">Guest Star</xsl:attribute>
                                    <xsl:attribute name="name"><xsl:value-of select="normalize-space(.)"/></xsl:attribute>
                                </xsl:element>
                            </xsl:for-each>
                            <xsl:for-each select="./directors/item">
                                <xsl:element name="person">
                                    <xsl:attribute name="job">Director</xsl:attribute>
                                    <xsl:attribute name="name"><xsl:value-of select="normalize-space(.)"/></xsl:attribute>
                                </xsl:element>
                            </xsl:for-each>
                            <xsl:for-each select="./writers/item">
                                <xsl:element name="person">
                                    <xsl:attribute name="job">Author</xsl:attribute>
                                    <xsl:attribute name="name"><xsl:value-of select="normalize-space(.)"/></xsl:attribute>
                                </xsl:element>
                            </xsl:for-each>
                        </people>
                    </xsl:if>
                    <xsl:if test="./filename/text() != '' or //data/series/_banners/*/raw/item">
                        <images>
                            <xsl:if test="./filename/text() != ''">
                                <xsl:element name="image">
                                    <xsl:attribute name="type">screenshot</xsl:attribute>
                                    <xsl:attribute name="url"><xsl:value-of select="normalize-space(filename)"/></xsl:attribute>
                                    <xsl:attribute name="thumb"><xsl:value-of select="normalize-space(tvdbXpath:replace(string(filename), 'banners', 'banners/_cache'))"/></xsl:attribute>
                                </xsl:element>
                            </xsl:if>
                            <xsl:for-each select="tvdbXpath:imageElements(//data/series/_banners/poster/raw, 'poster', //requestDetails)">
                                <xsl:sort select="@rating" data-type="number" order="descending"/>
                                <xsl:element name="image">
                                    <xsl:attribute name="type"><xsl:value-of select="normalize-space(@type)"/></xsl:attribute>
                                    <xsl:attribute name="url"><xsl:value-of select="normalize-space(@url)"/></xsl:attribute>
                                    <xsl:attribute name="thumb"><xsl:value-of select="normalize-space(@thumb)"/></xsl:attribute>
                                    <xsl:if test="@width != ''">
                                        <xsl:attribute name="width"><xsl:value-of select="normalize-space(@width)"/></xsl:attribute>
                                    </xsl:if>
                                    <xsl:if test="@height != ''">
                                        <xsl:attribute name="height"><xsl:value-of select="normalize-space(@height)"/></xsl:attribute>
                                    </xsl:if>
                                </xsl:element>
                            </xsl:for-each>
                            <xsl:for-each select="tvdbXpath:imageElements(//data/series/_banners/fanart/raw, 'fanart', //requestDetails)">
                                <xsl:sort select="@rating" data-type="number" order="descending"/>
                                <xsl:element name="image">
                                    <xsl:attribute name="type"><xsl:value-of select="normalize-space(@type)"/></xsl:attribute>
                                    <xsl:attribute name="url"><xsl:value-of select="normalize-space(@url)"/></xsl:attribute>
                                    <xsl:attribute name="thumb"><xsl:value-of select="normalize-space(@thumb)"/></xsl:attribute>
                                    <xsl:if test="@width != ''">
                                        <xsl:attribute name="width"><xsl:value-of select="normalize-space(@width)"/></xsl:attribute>
                                    </xsl:if>
                                    <xsl:if test="@height != ''">
                                        <xsl:attribute name="height"><xsl:value-of select="normalize-space(@height)"/></xsl:attribute>
                                    </xsl:if>
                                </xsl:element>
                            </xsl:for-each>
                            <xsl:for-each select="tvdbXpath:imageElements(//data/series/_banners/banner/raw, 'banner', //requestDetails)">
                                <xsl:sort select="@rating" data-type="number" order="descending"/>
                                <xsl:element name="image">
                                    <xsl:attribute name="type"><xsl:value-of select="normalize-space(@type)"/></xsl:attribute>
                                    <xsl:attribute name="url"><xsl:value-of select="normalize-space(@url)"/></xsl:attribute>
                                    <xsl:attribute name="thumb"><xsl:value-of select="normalize-space(@thumb)"/></xsl:attribute>
                                    <xsl:if test="@width != ''">
                                        <xsl:attribute name="width"><xsl:value-of select="normalize-space(@width)"/></xsl:attribute>
                                    </xsl:if>
                                    <xsl:if test="@height != ''">
                                        <xsl:attribute name="height"><xsl:value-of select="normalize-space(@height)"/></xsl:attribute>
                                    </xsl:if>
                                </xsl:element>
                            </xsl:for-each>
                        </images>
                    </xsl:if>
                </xsl:for-each>
            </item>
        </xsl:for-each>
    </xsl:template>
</xsl:stylesheet>
