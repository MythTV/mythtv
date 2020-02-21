#ifndef _XMLTVPARSER_H_
#define _XMLTVPARSER_H_

// Qt headers
#include <QMap>
#include <QList>
#include <QString>

// libmythtv
#include "channelinfo.h"

class ProgInfo;
class QUrl;
class QDomElement;

class XMLTVParser
{
  public:
    XMLTVParser();
    bool parseFile(const QString& filename, ChannelInfoList *chanlist,
                   QMap<QString, QList<ProgInfo> > *proglist);

  private:
    unsigned int m_currentYear {0};
    QString m_movieGrabberPath;
    QString m_tvGrabberPath;
};

#endif // _XMLTVPARSER_H_
