#ifndef XMLTVPARSER_H
#define XMLTVPARSER_H

// Qt headers
#include <QMap>
#include <QList>
#include <QString>

// MythTV
#include "libmythtv/channelinfo.h"

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

#endif // XMLTVPARSER_H
