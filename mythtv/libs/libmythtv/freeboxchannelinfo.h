#ifndef _FREEBOXCHANNELINFO_H_
#define _FREEBOXCHANNELINFO_H_

#include <qmap.h>
#include <qstring.h>

class FreeboxChannelInfo
{
  public:
    FreeboxChannelInfo() :
        m_name(QString::null),
        m_url(QString::null),
        m_xmltvid(QString::null) {}

    FreeboxChannelInfo(const QString &name,
                       const QString &url,
                       const QString &xmltvid) :
        m_name(name), m_url(url), m_xmltvid(xmltvid) {}

    bool isValid(void) const
    {
        return !m_name.isEmpty() && !m_url.isEmpty();
    }

  public:
    QString m_name;
    QString m_url;
    QString m_xmltvid;
};

typedef QMap<QString,FreeboxChannelInfo> fbox_chan_map_t;

#endif //_FREEBOXCHANNELINFO_H_

/* vim: set expandtab tabstop=4 shiftwidth=4: */
