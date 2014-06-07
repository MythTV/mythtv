#ifndef NETCOMMON_H_
#define NETCOMMON_H_

#include <QString>
#include <QDomDocument>
#include <QUrl>

#include <netutils.h>
#include <mythscreentype.h>

// Generic Data Formatting
QString GetThumbnailFilename(QString url, QString title);
QString GetMythXMLURL(void);
QUrl GetMythXMLSearch(QString url, QString query,
                      QString grabber, uint pagenum);

#endif // NETCOMMON_H_
