/** -*- Mode: c++ -*-
 *  IPTVChannelInfo
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef _IPTV_CHANNELINFO_H_
#define _IPTV_CHANNELINFO_H_

#include <QString>
#include <QMap>
#include "mythlogging.h"

class IPTVChannelInfo
{
  public:
    IPTVChannelInfo() :
        m_name(QString::null),
        m_url(QString::null),
        m_xmltvid(QString::null) {}

    IPTVChannelInfo(const QString &name,
                    const QString &url,
                    const QString &xmltvid) :
        m_name(name), m_url(url), m_xmltvid(xmltvid)
    {
    }

    bool isValid(void) const
    {
        return !m_name.isEmpty() && !m_url.isEmpty();
    }
    
    QString toString(void) const
    {
        return QString("IPTVChannelInfo: Name(%1) Url(%2) xmltvid(%3")
        .arg(m_name).arg(m_url).arg(m_xmltvid);
    }

  public:
    QString m_name;
    QString m_url;
    QString m_xmltvid;
};

typedef QMap<QString,IPTVChannelInfo> fbox_chan_map_t;

#endif // _IPTV_CHANNELINFO_H_

/* vim: set expandtab tabstop=4 shiftwidth=4: */
