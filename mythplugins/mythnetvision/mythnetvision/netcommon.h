#ifndef NETCOMMON_H_
#define NETCOMMON_H_

#include <QDomDocument>
#include <QString>
#include <QUrl>

#include <libmythbase/netutils.h>
#include <libmythui/mythscreentype.h>

// Generic Data Formatting
QString GetThumbnailFilename(const QString& url, const QString& title);
QString GetMythXMLURL(void);
QUrl GetMythXMLSearch(const QString& url, const QString& query,
                      const QString& grabber, const QString& pagenum);

#endif // NETCOMMON_H_
