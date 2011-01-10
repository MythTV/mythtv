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

    bool ProcessRequest(HttpWorkerThread *pThread, HTTPRequest *pRequest);

  private:
    static void PrintHeader(QTextStream&, const QString &form);
    static void PrintFooter(QTextStream&);
    static bool LoadSettings(MythSettingList&, const QString &hostname);
    static void PrintSettings(QTextStream&, const MythSettingList&);

    MythSettingList database_settings;
    MythSettingList general_settings;
};

#endif
