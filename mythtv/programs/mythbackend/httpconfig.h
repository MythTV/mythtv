// -*- Mode: c++ -*-

#ifndef _HTTPCONFIG_H_
#define _HTTPCONFIG_H_

#include "httpserver.h"
#include "mythsettings.h"

class QTextStream;

class HttpConfig : public HttpServerExtension
{
  public:
    HttpConfig();
    virtual ~HttpConfig();

    virtual QStringList GetBasePaths();

    bool ProcessRequest(HttpWorkerThread *pThread, HTTPRequest *pRequest);

  private:
    static void PrintHeader(QBuffer&, const QString &form);
    static void PrintFooter(QBuffer&);
    static bool LoadSettings(MythSettingList&, const QString &hostname);
    static void PrintSettings(QBuffer&, const MythSettingList&);

    MythSettingList database_settings;
    MythSettingList general_settings;
};

#endif
