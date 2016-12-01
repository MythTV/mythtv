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
    void lateInit();

    ChannelInfo *parseChannel(QDomElement &element, QUrl &baseUrl);
    ProgInfo *parseProgram(QDomElement &element);
    bool parseFile(QString filename, ChannelInfoList *chanlist,
                   QMap<QString, QList<ProgInfo> > *proglist);

  private:
    unsigned int current_year;
    QString _movieGrabberPath;
    QString _tvGrabberPath;
};

#endif // _XMLTVPARSER_H_
