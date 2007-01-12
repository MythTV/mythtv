#ifndef _XMLTVPARSER_H_
#define _XMLTVPARSER_H_

// Qt headers
#include <qvaluelist.h>
#include <qstring.h>
#include <qurl.h>
#include <qdom.h>
#include <qmap.h>

class ProgInfo;
class ChanInfo;

class XMLTVParser
{
  public:
    XMLTVParser() : isJapan(false) {}

    ChanInfo *parseChannel(QDomElement &element, QUrl baseUrl);
    ProgInfo *parseProgram(QDomElement &element, int localTimezoneOffset);
    bool parseFile(
        QString filename, QValueList<ChanInfo> *chanlist,
        QMap<QString, QValueList<ProgInfo> > *proglist);


  public:
    bool isJapan;
};

#endif // _XMLTVPARSER_H_
