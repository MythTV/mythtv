<?xml version="1.0" encoding="UTF-8"?>
<!--
    Create PBS Show user configuration XML for "pbs.xml"
-->
<xsl:stylesheet version="1.0"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:mnvXpath="http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format">

    <xsl:output method="xml" indent="yes" version="1.0" encoding="UTF-8" omit-xml-declaration="yes"/>

    <!--
        This template calls all other templates which allows for multiple sources to be processed
        within a single Xslt file
    -->
    <xsl:template match="/">
        <xsl:element name="xml">
            <xsl:element name="directory">
                <xsl:attribute name="name">PBS</xsl:attribute>
                <xsl:attribute name="globalmax">0</xsl:attribute>
                <xsl:call-template name='pbsUserConfig'/>
            </xsl:element>
        </xsl:element>
    </xsl:template>

    <xsl:template name='pbsUserConfig'>
        <xsl:for-each select='//ul[@class="subnav hide threecol"]//a[@href != "http://video.pbs.org/morevideos.html"]'>
            <xsl:element name="subDirectory">
                <xsl:attribute name="name"><xsl:value-of select="normalize-space(.)"/></xsl:attribute>
                <xsl:element name="sourceURL">
                    <xsl:attribute name="enabled">true</xsl:attribute>
                    <xsl:attribute name="name"><xsl:value-of select="normalize-space(.)"/></xsl:attribute>
                    <xsl:attribute name="type">xml</xsl:attribute>
                    <xsl:attribute name="xsltFile">pbsRSS_item</xsl:attribute>
                    <xsl:attribute name="url"><xsl:value-of select="concat(normalize-space(./@href), 'rss/')"/></xsl:attribute>
                </xsl:element>
            </xsl:element>
        </xsl:for-each>
    </xsl:template>

</xsl:stylesheet>
