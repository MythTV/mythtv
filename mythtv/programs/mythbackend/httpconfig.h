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
    virtual ~HttpConfig() = default;

    QStringList GetBasePaths() override; // HttpServerExtension

    bool ProcessRequest(HTTPRequest *pRequest) override; // HttpServerExtension

  private:
    static void PrintHeader(QBuffer&, const QString &form,
                            const QString &group = "");
    static void OpenForm(QBuffer&, const QString &form,
                         const QString &group = "");
    static void CloseForm(QBuffer&,
                          const QString &group = "");
    static void PrintFooter(QBuffer&,
                            const QString &group = "");
    static bool LoadSettings(MythSettingList&, const QString &hostname);
    static void PrintSettings(QBuffer&, const MythSettingList&);

    MythSettingList m_database_settings;
    MythSettingList m_general_settings;
};

#endif
