<?xml version="1.0" encoding="UTF-8"?>
<!--
    themoviedb.org query result conversion to MythTV Universal Metadata Format
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
                <xsl:call-template name='movieQuery'/>
            </metadata>
        </xsl:if>
    </xsl:template>

    <xsl:template name="movieQuery">
        <xsl:for-each select="//movie">
            <item>
                <language><xsl:value-of select="normalize-space(language)"/></language>
                <xsl:for-each select="tmdbXpath:titleElement(string(name))">
                    <title><xsl:value-of select="normalize-space(./@title)"/></title>
                    <xsl:if test="./@subtitle">
                        <subtitle><xsl:value-of select="normalize-space(./@subtitle)"/></subtitle>
                    </xsl:if>
                </xsl:for-each>
                <inetref><xsl:value-of select="normalize-space(id)"/></inetref>
                <imdb><xsl:value-of select="normalize-space(substring-after(string(imdb_id), 'tt'))"/></imdb>
                <userrating><xsl:value-of select="normalize-space(rating)"/></userrating>
                <xsl:if test="./certification/text() != ''">
                    <certifications>
                        <!--
                            This code will need to be modified when multiple country cerification
                            information is supported. Right now it assumes ONLY ratings from the US.
                        -->
                        <xsl:for-each select=".//certification">
                            <xsl:element name="certification">
                                <xsl:attribute name="locale">us</xsl:attribute>
                                <xsl:attribute name="name"><xsl:value-of select="normalize-space(.)"/></xsl:attribute>
                            </xsl:element>
                        </xsl:for-each>
                    </certifications>
                </xsl:if>
                <description><xsl:value-of select="normalize-space(overview)"/></description>
                <releasedate><xsl:value-of select="normalize-space(released)"/></releasedate>
                <xsl:if test=".//image[@type='poster'] or .//image[@type='backdrop']">
                    <images>
                        <xsl:if test=".//image[@type='poster']">
                            <xsl:element name="image">
                                <xsl:attribute name="type">coverart</xsl:attribute>
                                <xsl:if test=".//image[@type='poster' and @size='original']">
                                    <xsl:attribute name="url"><xsl:value-of select="normalize-space(.//image[@type='poster' and @size='original']/@url)"/></xsl:attribute>
                                </xsl:if>
                                <xsl:if test=".//image[@type='poster' and @size='cover']">
                                    <xsl:attribute name="thumb"><xsl:value-of select="normalize-space(.//image[@type='poster' and @size='cover']/@url)"/></xsl:attribute>
                                </xsl:if>
                                <xsl:if test="not(.//image[@type='poster' and @size='cover']) and .//image[@type='poster' and @size='thumb']">
                                    <xsl:attribute name="thumb"><xsl:value-of select="normalize-space(.//image[@type='poster' and @size='thumb']/@url)"/></xsl:attribute>
                                </xsl:if>
                            </xsl:element>
                        </xsl:if>
                        <xsl:if test=".//image[@type='backdrop']">
                            <xsl:element name="image">
                                <xsl:attribute name="type">fanart</xsl:attribute>
                                <xsl:if test=".//image[@type='backdrop' and @size='original']">
                                    <xsl:attribute name="url"><xsl:value-of select="normalize-space(.//image[@type='backdrop' and @size='original']/@url)"/></xsl:attribute>
                                </xsl:if>
                                <xsl:if test=".//image[@type='backdrop' and @size='thumb']">
                                    <xsl:attribute name="thumb"><xsl:value-of select="normalize-space(.//image[@type='backdrop' and @size='thumb']/@url)"/></xsl:attribute>
                                </xsl:if>
                            </xsl:element>
                        </xsl:if>
                    </images>
                </xsl:if>
                <lastupdated><xsl:value-of select="tmdbXpath:lastUpdated(string(last_modified_at), '%Y-%m-%d %H:%M:%S')"/></lastupdated>
            </item>
        </xsl:for-each>
    </xsl:template>
</xsl:stylesheet>
