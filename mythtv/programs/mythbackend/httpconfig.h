// -*- Mode: c++ -*-

#ifndef HTTPCONFIG_H
#define HTTPCONFIG_H

#include "libmythupnp/httpserver.h"
#include "mythsettings.h"

class QTextStream;

class HttpConfig : public HttpServerExtension
{
  public:
    HttpConfig();
    ~HttpConfig() override = default;

    QStringList GetBasePaths() override; // HttpServerExtension

    bool ProcessRequest(HTTPRequest *pRequest) override; // HttpServerExtension

  private:
    static void PrintHeader(QBuffer &buffer, const QString &form,
                            const QString &group = "");
    static void OpenForm(QBuffer &buffer, const QString &form,
                         const QString &group = "");
    static void CloseForm(QBuffer &buffer,
                          const QString &group = "");
    static void PrintFooter(QBuffer &buffer,
                            const QString &group = "");
    static bool LoadSettings(MythSettingList&, const QString &hostname);
    static void PrintSettings(QBuffer &buffer, const MythSettingList &settings);

    MythSettingList m_databaseSettings;
    MythSettingList m_generalSettings;
};

#endif // HTTPCONFIG_H
