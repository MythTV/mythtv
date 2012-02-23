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
        <xsl:if test="//Data/Series">
            <metadata>
                <xsl:call-template name='tvdbVideoData'/>
            </metadata>
        </xsl:if>
    </xsl:template>

    <xsl:template name="tvdbVideoData">
        <xsl:for-each select="//Data/Series">
            <item>
                <title><xsl:value-of select="normalize-space(SeriesName)"/></title>
                <xsl:if test="tvdbXpath:getValue(//requestDetails, //Data, 'subtitle') != ''">
                    <subtitle><xsl:value-of select="normalize-space(tvdbXpath:getResult())"/></subtitle>
                </xsl:if>
                <language><xsl:value-of select="normalize-space(Language)"/></language>
                <xsl:if test="tvdbXpath:getValue(//requestDetails, //Data, 'description') != ''">
                    <description><xsl:value-of select="normalize-space(tvdbXpath:htmlToString(tvdbXpath:getResult()))"/></description>
                </xsl:if>
                <season><xsl:value-of select="normalize-space(//requestDetails/@season)"/></season>
                <episode><xsl:value-of select="normalize-space(//requestDetails/@episode)"/></episode>
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
                <collectionref><xsl:value-of select="normalize-space(id)"/></collectionref>
                <xsl:if test="./IMDB_ID/text() != '' and tvdbXpath:getValue(//requestDetails, //Data, 'IMDB') = ''">
                    <imdb><xsl:value-of select="normalize-space(substring-after(string(IMDB_ID), 'tt'))"/></imdb>
                </xsl:if>
                <xsl:if test="./zap2it_id/text() != ''">
                    <tmsref><xsl:value-of select="normalize-space(zap2it_id)"/></tmsref>
                </xsl:if>
                <xsl:for-each select="tvdbXpath:getValue(//requestDetails, //Data, 'allEpisodes', 'allresults')">
                    <xsl:if test="./IMDB_ID/text() != ''">
                        <imdb><xsl:value-of select="normalize-space(substring-after(string(IMDB_ID), 'tt'))"/></imdb>
                    </xsl:if>
                    <xsl:if test="./Rating/text() != ''">
                        <userrating><xsl:value-of select="normalize-space(./Rating)"/></userrating>
                    </xsl:if>
                    <xsl:if test="./FirstAired/text() != ''">
                        <year><xsl:value-of select="normalize-space(substring(string(./FirstAired), 1, 4))"/></year>
                        <releasedate><xsl:value-of select="normalize-space(./FirstAired)"/></releasedate>
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
                    <xsl:if test="//Actors/Actor or .//GuestStars/text() != '' or .//Director/text() != '' or .//Writer/text() != ''">
                        <people>
                            <xsl:for-each select="//Actors/Actor">
                                <xsl:element name="person">
                                    <xsl:attribute name="job">Actor</xsl:attribute>
                                    <xsl:attribute name="name"><xsl:value-of select="normalize-space(Name)"/></xsl:attribute>
                                    <xsl:attribute name="character"><xsl:value-of select="normalize-space(Role)"/></xsl:attribute>
                                    <xsl:if test="./Image/text() != ''">
                                        <xsl:attribute name="url"><xsl:value-of select="concat('http://www.thetvdb.com/banners/', normalize-space(Image))"/></xsl:attribute>
                                        <xsl:attribute name="thumb"><xsl:value-of select="concat('http://www.thetvdb.com/banners/_cache/', normalize-space(Image))"/></xsl:attribute>
                                    </xsl:if>
                                </xsl:element>
                            </xsl:for-each>
                            <xsl:for-each select="tvdbXpath:stringToList(string(./GuestStars))">
                                <xsl:element name="person">
                                    <xsl:attribute name="job">Guest Star</xsl:attribute>
                                    <xsl:attribute name="name"><xsl:value-of select="normalize-space(.)"/></xsl:attribute>
                                </xsl:element>
                            </xsl:for-each>
                            <xsl:for-each select="tvdbXpath:stringToList(string(./Director))">
                                <xsl:element name="person">
                                    <xsl:attribute name="job">Director</xsl:attribute>
                                    <xsl:attribute name="name"><xsl:value-of select="normalize-space(.)"/></xsl:attribute>
                                </xsl:element>
                            </xsl:for-each>
                            <xsl:for-each select="tvdbXpath:stringToList(string(./Writer))">
                                <xsl:element name="person">
                                    <xsl:attribute name="job">Author</xsl:attribute>
                                    <xsl:attribute name="name"><xsl:value-of select="normalize-space(.)"/></xsl:attribute>
                                </xsl:element>
                            </xsl:for-each>
                        </people>
                    </xsl:if>
                    <xsl:if test="./filename/text() != '' or //Banners/Banner">
                        <images>
                            <xsl:if test="./filename/text() != ''">
                                <xsl:element name="image">
                                    <xsl:attribute name="type">screenshot</xsl:attribute>
                                    <xsl:attribute name="url"><xsl:value-of select="concat('http://www.thetvdb.com/banners/', normalize-space(filename))"/></xsl:attribute>
                                    <xsl:attribute name="thumb"><xsl:value-of select="concat('http://www.thetvdb.com/banners/_cache/', normalize-space(filename))"/></xsl:attribute>
                                </xsl:element>
                            </xsl:if>
                            <xsl:for-each select="tvdbXpath:imageElements(//Banners, 'poster', //requestDetails)">
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
                            <xsl:for-each select="tvdbXpath:imageElements(//Banners, 'fanart', //requestDetails)">
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
                            <xsl:for-each select="tvdbXpath:imageElements(//Banners, 'banner', //requestDetails)">
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
