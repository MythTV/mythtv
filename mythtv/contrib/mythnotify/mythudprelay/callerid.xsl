<?xml version="1.0" encoding="ISO-8859-1"?>

<xsl:stylesheet version="1.0"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

  <xsl:template match="/">
    <xsl:apply-templates select="telephone-event[@type='callerID']" />
  </xsl:template>
  
  <xsl:template match="telephone-event"> 
  <mythnotify version="1">
    <container name="notify_cid_info">
      <textarea name="notify_cid_line">
        <value>LINE #<xsl:value-of select="ring-type"/></value>
      </textarea>
      <textarea name="notify_cid_name">
        <value>NAME: <xsl:value-of select="from/name"/></value>
      </textarea>
      <textarea name="notify_cid_num">
        <value>NUM : <xsl:variable name="phone" select="from/number/text()"/>
<xsl:choose><xsl:when test="number($phone) = $phone"><xsl:value-of select="concat('(',substring($phone,1,3),') ',substring($phone,4,3),'-',substring($phone,7,4))"/></xsl:when><xsl:otherwise><xsl:value-of select="$phone"/></xsl:otherwise></xsl:choose>
</value>
      </textarea>
      <textarea name="notify_cid_dt">
        <value>DATE: <xsl:value-of select="time/@month"/>/<xsl:value-of select="time/@day"/> TIME: <xsl:value-of select="time/@hour"/>:<xsl:value-of select="time/@minute"/> </value>
      </textarea>
    </container>
  </mythnotify>
</xsl:template>

</xsl:stylesheet>
