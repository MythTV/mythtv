// Qt headers
#include <QByteArray>
#include <QDir>
#include <QFileInfo>
#include <QTextStream>
#include <QUrl>

// MythTV headers
#include "httpconfig.h"
#include "backendutil.h"
#include "mythcontext.h"
#include "mythdb.h"
#include "mythdirs.h"
#include "storagegroup.h"
#include "mythdownloadmanager.h"
#include "mythcoreutil.h"

HttpConfig::HttpConfig() : HttpServerExtension("HttpConfig", QString())
{
}

HttpConfig::~HttpConfig()
{
}

QStringList HttpConfig::GetBasePaths()
{
    QStringList paths;
    paths << "/Config";
    paths << "/Config/Database";
    paths << "/Config/General";
    return paths;
}

bool HttpConfig::ProcessRequest(HTTPRequest *request)
{
    if (!request)
        return false;

    LOG(VB_UPNP, LOG_INFO,
        QString("HttpConfig::ProcessRequest(): m_sBaseURL: '%1',"
                "m_sMethod: '%2'")
            .arg(request->m_sBaseUrl).arg(request->m_sMethod));
    if (!request->m_sBaseUrl.startsWith("/Config"))
    {
        return false;
    }

    bool handled = false;
    if (request->m_sMethod == "Save")
    {
        {
            // DISABLE HTML SETUP SAVING
            // DISABLE HTML SETUP SAVING
            QTextStream os(&request->m_response);
            os << "<html>\r\n"
                  "  <head>\r\n"
                  "    <title>Saving is Disabled</title>\r\n"
                  "  </head>\r\n"
                  "  <body>\r\n"
                  "    <b>Saving is Disabled</b><br>\r\n"
                  "  </body>\r\n"
                  "</html>\r\n";

            request->m_eResponseType = ResponseTypeHTML;
            request->m_mapRespHeaders[ "Cache-Control" ] =
                "no-cache=\"Ext\", max-age = 0";

            return true;
            // DISABLE HTML SETUP SAVING
            // DISABLE HTML SETUP SAVING
        }

        // FIXME, this is always false, what's it for
        // JMS "fixed" by using endsWith()
        if (request->m_sBaseUrl.endsWith("config") &&
            !database_settings.empty())
        {
            QString checkResult;
            PrintHeader(request->m_response, "/Config/Database");
            check_settings(database_settings, request->m_mapParams,
                           checkResult);
            load_settings(database_settings, "");
            PrintSettings(request->m_response, database_settings);
            PrintFooter(request->m_response);
            handled = true;
        }
        else
        {
            bool okToSave = false;
            QString checkResult;
            QString fn = GetShareDir() + "backend-config/";

            if (request->m_sBaseUrl == "/Config/Database")
            {
                if (check_settings(database_settings, request->m_mapParams,
                                   checkResult))
                    okToSave = true;
            }
            else if (request->m_sBaseUrl == "/Config/General")
            {
                if (check_settings(general_settings, request->m_mapParams,
                                   checkResult))
                    okToSave = true;
            }

            if (okToSave)
                LOG(VB_UPNP, LOG_INFO, "HTTP method 'Save' called, but not handled");
#if 0
            QTextStream os(&request->m_response);
            os << "<html><body><h3>The Save function for this screen is "
               << "not hooked up yet</h3><dl>";
            QStringMap::const_iterator it = request->m_mapParams.begin();
            for (; it!=request->m_mapParams.end(); ++it)
            {
                if (it.key() == "__group__")
                    continue;

                os << "<dt>"<<it.key()<<"</dt><dd>"
                                    <<*it<<"</dd>\r\n";
            }
            os << "</dl></body></html>";
            handled = true;
#else
            QTextStream os(&request->m_response);
            os << checkResult;
            request->m_eResponseType     = ResponseTypeOther;
            request->m_sResponseTypeText = "application/json";
            request->m_mapRespHeaders[ "Cache-Control" ] =
                "no-cache=\"Ext\", max-age = 0";

            return true;
#endif
        }
    }
    else if (request->m_sMethod == "Settings")
    {
        QString result = "{ \"Error\": \"Unknown Settings List\" }";
        QString fn = GetShareDir() + "backend-config/";

        if (request->m_sBaseUrl == "/Config/Database")
        {
            fn += "config_backend_database.xml";
            parse_settings(database_settings, fn);
            result = StringMapToJSON(
                GetSettingsMap(database_settings, gCoreContext->GetHostName()));
        }
        else if (request->m_sBaseUrl == "/Config/General")
        {
            fn += "config_backend_general.xml";
            parse_settings(general_settings, fn);
            result = StringMapToJSON(
                GetSettingsMap(general_settings, gCoreContext->GetHostName()));
        }

        QTextStream os(&request->m_response);
        os << result;
        request->m_eResponseType     = ResponseTypeOther;
        request->m_sResponseTypeText = "application/json";
        request->m_mapRespHeaders[ "Cache-Control" ] =
            "no-cache=\"Ext\", max-age = 0";

        return true;
    }
    else if (request->m_sMethod == "XML")
    {
        QString fn = GetShareDir() + "backend-config/";

        if (request->m_sBaseUrl == "/Config/Database")
            fn += "config_backend_database.xml";
        else if (request->m_sBaseUrl == "/Config/General")
            fn += "config_backend_general.xml";

        request->FormatFileResponse(fn);
        return true;
    }
    else if ((request->m_sMethod == "InstallPackage") &&
             (request->m_mapParams.contains("package")))
    {
        QString package = QUrl::fromPercentEncoding(request->m_mapParams["package"].toUtf8());
        QString url = QString("http://www.mythtv.org/ftp/3rdParty/%1").arg(package);
        StorageGroup tmpGroup("Temp", gCoreContext->GetHostName());
        QString tmpFile = tmpGroup.GetFirstDir(true) + "package.zip";
        StorageGroup destGroup("3rdParty", gCoreContext->GetHostName());
        QString outDir = destGroup.GetFirstDir();

        QString result = "false";
        if ((GetMythDownloadManager()->download(url, tmpFile)) &&
            (extractZIP(tmpFile, outDir)))
        {
            result = "true";
        }

        QTextStream os(&request->m_response);
        os << StringListToJSON("Result", QStringList(result));

        request->m_eResponseType     = ResponseTypeOther;
        request->m_sResponseTypeText = "application/json";
        request->m_mapRespHeaders[ "Cache-Control" ] =
            "no-cache=\"Ext\", max-age = 0";

        return true;
    }
    else if ((request->m_sMethod == "FileBrowser") &&
             (request->m_mapParams.contains("dir")))
    {
        QString startingDir = QUrl::fromPercentEncoding(request->m_mapParams["dir"].toUtf8());
        if (startingDir.startsWith("myth://"))
        {
            QUrl qurl(startingDir);
            QString dir;

            QString host = qurl.host();
            int port = qurl.port();

            dir = qurl.path();

            QString storageGroup = qurl.userName();

            StorageGroup sgroup(storageGroup);
            QStringList entries = sgroup.GetFileInfoList(dir);

            if ((entries.size() == 1) &&
                (entries[0].startsWith("sgdir::")))
            {
                QStringList parts = entries[0].split("::");
                entries = sgroup.GetFileInfoList(parts[1]);
            }

            if (entries.size())
            {
                QTextStream os(&request->m_response);
                os << "<ul class=\"jqueryFileTree\" style=\"display: none;\">\r\n";

                for (QStringList::iterator it = entries.begin();
                     it != entries.end(); ++it)
                {
                    QString entry = *it;
                    QStringList parts = entry.split("::");
                    QFileInfo fi(parts[1]);
                    if (dir == "/")
                        dir = "";
                    QString path =
                            gCoreContext->GenMythURL(host,
                                                     port,
                                                     dir + parts[1],
                                                     storageGroup);
                    if (entry.startsWith("sgdir::"))
                    {
                        os << "    <li class=\"directory collapsed\"><a href=\"#\" rel=\""
                           << path << "/\">" << parts[1] << "</a></li>\r\n";
                    }
                    else if (entry.startsWith("dir::"))
                    {
                        os << "    <li class=\"directory collapsed\"><a href=\"#\" rel=\""
                           << path << "/\">" << fi.fileName() << "</a></li>\r\n";
                    }
                    else if (entry.startsWith("file::"))
                    {
                        os << "    <li class=\"file ext_" << fi.suffix() << "\"><a href=\"#\" rel=\""
                           << parts[3] << "\">" << fi.fileName() << "</a></li>\r\n";
                    }
                }
                os << "</ul>\r\n";

                handled = true;
            }
        } else {
            QDir dir(startingDir);
            if (dir.exists())
            {
                QTextStream os(&request->m_response);
                os << "<ul class=\"jqueryFileTree\" style=\"display: none;\">\r\n";

                QFileInfoList infoList = dir.entryInfoList();
                for (QFileInfoList::iterator it  = infoList.begin();
                                             it != infoList.end();
                                           ++it )
                {
                    QFileInfo &fi = *it;
                    if (!fi.isDir())
                        continue;
                    if (fi.fileName().startsWith("."))
                        continue;

                    os << "    <li class=\"directory collapsed\"><a href=\"#\" rel=\""
                       << fi.absoluteFilePath() << "/\">" << fi.fileName() << "</a></li>\r\n";
                }

                bool dirsOnly = true;
                if (request->m_mapParams.contains("dirsOnly"))
                    dirsOnly = request->m_mapParams["dirsOnly"].toInt();

                if (!dirsOnly)
                {
                    for (QFileInfoList::iterator it  = infoList.begin();
                                                 it != infoList.end();
                                               ++it )
                    {
                        QFileInfo &fi = *it;
                        if (fi.isDir())
                            continue;
                        if (fi.fileName().startsWith("."))
                            continue;

                        os << "    <li class=\"file ext_" << fi.suffix() << "\"><a href=\"#\" rel=\""
                           << fi.absoluteFilePath() << "\">" << fi.fileName() << "</a></li>\r\n";
                    }
                }
                os << "</ul>\r\n";

                handled = true;
            }
        }
    }
    else if ((request->m_sMethod == "GetValueList") &&
             (request->m_mapParams.contains("List")))
    {
        QString key = request->m_mapParams["List"];
        QStringList sList = GetSettingValueList(key);
        QTextStream os(&request->m_response);
        os << StringListToJSON(key, sList);

        request->m_eResponseType     = ResponseTypeOther;
        request->m_sResponseTypeText = "application/json";
        request->m_mapRespHeaders[ "Cache-Control" ] =
            "no-cache=\"Ext\", max-age = 0";

        return true;
    }
    else if ((request->m_sMethod == "Database") || (NULL == gContext))
    {
        QString fn = GetShareDir() + "backend-config/"
            "config_backend_database.xml";
        QString group;
        QString form("/Config/Database/Save");

        if (request->m_mapParams.contains("__group__"))
            group = request->m_mapParams["__group__"];

        if (group.isEmpty())
            PrintHeader(request->m_response, form);
        else
            OpenForm(request->m_response, form, group);

        parse_settings(general_settings, fn, group);
        load_settings(general_settings, gCoreContext->GetHostName());
        PrintSettings(request->m_response, general_settings);

        if (group.isEmpty())
            PrintFooter(request->m_response);
        else
            CloseForm(request->m_response, group);

        handled = true;
    }
    else if (request->m_sMethod == "General")
    {
        QString fn = GetShareDir() + "backend-config/"
            "config_backend_general.xml";
        QString group;
        QString form("/Config/General/Save");

        if (request->m_mapParams.contains("__group__"))
            group = request->m_mapParams["__group__"];

        if (group.isEmpty())
            PrintHeader(request->m_response, form);
        else
            OpenForm(request->m_response, form, group);

        parse_settings(general_settings, fn, group);
        load_settings(general_settings, gCoreContext->GetHostName());
        PrintSettings(request->m_response, general_settings);

        if (group.isEmpty())
            PrintFooter(request->m_response);
        else
            CloseForm(request->m_response, group);

        handled = true;
    }

    if (handled)
    {
        request->m_eResponseType = ResponseTypeHTML;
        request->m_mapRespHeaders[ "Cache-Control" ] =
            "no-cache=\"Ext\", max-age = 0";
    }

    return handled;
}

void HttpConfig::PrintHeader(QBuffer &buffer, const QString &form,
                             const QString &group)
{
    QTextStream os(&buffer);

    os.setCodec("UTF-8");

    os << "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" "
       << "\"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">\r\n"
       << "<html xmlns=\"http://www.w3.org/1999/xhtml\""
       << " xml:lang=\"en\" lang=\"en\">\r\n"
       << "<head>\r\n"
       << "  <meta http-equiv=\"Content-Type\"\r\n"
       << "        content=\"text/html; charset=UTF-8\" />\r\n"
       << "  <link rel=\"stylesheet\" href=\"/setup/css/Config.css\" type=\"text/css\">\r\n"
       << "  <title>MythTV Config</title>"
       << "</head>\r\n"
       << "<body>\r\n\r\n"
       << "<div class=\"config\">\r\n"
       << "  <h1 class=\"config\">MythTV Configuration</h1>\r\n";

    OpenForm(buffer, form, group);
}

void HttpConfig::OpenForm(QBuffer &buffer, const QString &form,
                          const QString &group)
{
    QTextStream os(&buffer);

    os.setCodec("UTF-8");

    os << "  <form id=\"config_form_" << group << "\">\r\n"
       << "    <input type=\"hidden\" id=\"__config_form_action__\" value=\"" << form << "\" />\r\n"
       << "    <input type=\"hidden\" id=\"__group__\" value=\"" << group << "\" />\r\n";
}

void HttpConfig::CloseForm(QBuffer &buffer, const QString &group)
{
    QTextStream os(&buffer);

//    os << "    <div class=\"config_form_submit\"\r\n"
//       << "         id=\"config_form_submit\">\r\n";
    os << "      <input type=\"button\" value=\"Save Changes\" onClick=\"javascript:submitConfigForm('" << group << "')\" />\r\n"
//       << "    </div>\r\n"
       << "  </form>\r\n";
}

void HttpConfig::PrintFooter(QBuffer &buffer, const QString &group)
{
    CloseForm(buffer, group);

    QTextStream os(&buffer);

    os << "</div>\r\n"
       << "</body>\r\n"
       << "</html>\r\n";
}

void HttpConfig::PrintSettings(QBuffer &buffer, const MythSettingList &settings)
{
    QTextStream os(&buffer);

    MythSettingList::const_iterator it = settings.begin();
    for (; it != settings.end(); ++it)
        os << (*it)->ToHTML(1);
}
