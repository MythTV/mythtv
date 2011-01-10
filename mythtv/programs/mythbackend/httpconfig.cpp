// Qt headers
#include <QTextStream>

// MythTV headers
#include "httpconfig.h"
#include "backendutil.h"
#include "mythxml.h"
#include "mythcontext.h"
#include "mythdb.h"
#include "mythdirs.h"

HttpConfig::HttpConfig() : HttpServerExtension("HttpConfig", QString())
{
}

HttpConfig::~HttpConfig()
{
}

bool HttpConfig::ProcessRequest(HttpWorkerThread*, HTTPRequest *request)
{
    if (!request)
        return false;

    VERBOSE(VB_IMPORTANT, QString("ProcessRequest '%1' '%2'")
            .arg(request->m_sBaseUrl).arg(request->m_sMethod));

    if (request->m_sBaseUrl != "/" &&
        request->m_sBaseUrl.left(7) != "/config")
    {
        return false;
    }

    bool handled = false;
    if (request->m_sMethod.toLower() == "save")
    {
        if (request->m_sBaseUrl.right(7) == "config" &&
            !database_settings.empty())
        {
            PrintHeader(request->m_response, "config");
            check_settings(database_settings, request->m_mapParams);
            load_settings(database_settings, "");
            PrintSettings(request->m_response, database_settings);
            PrintFooter(request->m_response);
            handled = true;
        }
        else
        {
            request->m_response << "<html><body><dl>";
            QStringMap::const_iterator it = request->m_mapParams.begin();
            for (; it!=request->m_mapParams.end(); ++it)
            {
                request->m_response << "<dt>"<<it.key()<<"</dt><dd>"
                                    <<*it<<"</dd>\r\n";
            }
            request->m_response << "</dl></body></html>";
            handled = true;
        }
    }

    if ((request->m_sMethod.toLower() == "config") || (NULL == gContext))
    {
        PrintHeader(request->m_response, "config");
        QString fn = GetShareDir() + "backend-config/"
            "config_backend_database.xml";
        parse_settings(database_settings, fn);
        load_settings(database_settings, "");
        PrintSettings(request->m_response, database_settings);
        PrintFooter(request->m_response);
        handled = true;
    }
    else if (request->m_sMethod.toLower() == "general")
    {
        PrintHeader(request->m_response, "config/general");
        QString fn = GetShareDir() + "backend-config/"
            "config_backend_general.xml";
        parse_settings(general_settings, fn);
        load_settings(general_settings, gCoreContext->GetHostName());
        PrintSettings(request->m_response, general_settings);
        PrintFooter(request->m_response);
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

void HttpConfig::PrintHeader(QTextStream &os, const QString &form)
{
    os.setCodec("UTF-8");

    os << "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" "
       << "\"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">\r\n"
       << "<html xmlns=\"http://www.w3.org/1999/xhtml\""
       << " xml:lang=\"en\" lang=\"en\">\r\n"
       << "<head>\r\n"
       << "  <meta http-equiv=\"Content-Type\"\r\n"
       << "        content=\"text/html; charset=UTF-8\" />\r\n"
       << "  <style type=\"text/css\" title=\"Default\" media=\"all\">\r\n"
       << "  body {\r\n"
       << "    background-color:#fff;\r\n"
       << "    font:11px verdana, arial, helvetica, sans-serif;\r\n"
       << "    margin:20px;\r\n"
       << "  }\r\n"
       << "  h1 {\r\n"
       << "    font-size:28px;\r\n"
       << "    font-weight:900;\r\n"
       << "    color:#ccc;\r\n"
       << "    letter-spacing:0.5em;\r\n"
       << "    margin-bottom:30px;\r\n"
       << "    width:650px;\r\n"
       << "    text-align:center;\r\n"
       << "  }\r\n"
       << "  h2 {\r\n"
       << "    font-size:18px;\r\n"
       << "    font-weight:800;\r\n"
       << "    color:#360;\r\n"
       << "    border:none;\r\n"
       << "    letter-spacing:0.3em;\r\n"
       << "    padding:0px;\r\n"
       << "    margin-bottom:10px;\r\n"
       << "    margin-top:0px;\r\n"
       << "  }\r\n"
       << "  h3 {\r\n"
       << "    font-size:14px;\r\n"
       << "    font-weight:800;\r\n"
       << "    color:#360;\r\n"
       << "    border:none;\r\n"
       << "    letter-spacing:0.3em;\r\n"
       << "    padding:0px;\r\n"
       << "    margin-bottom:10px;\r\n"
       << "    margin-top:0px;\r\n"
       << "  }\r\n"
       << "  </style>\r\n"
       << "  <title>MythTV Config</title>"
       << "</head>\r\n"
       << "<body>\r\n\r\n"
       << "  <h1>MythTV Configuration</h1>\r\n"
       << "  <form action=\"/" << form << "/save\" method=\"POST\">\r\n"
       << "    <div class=\"form_buttons_top\"\r\n"
       << "         id=\"form_buttons_top\">\r\n"
       << "      <input type=\"submit\" value=\"Save Changes\" />\r\n"
       << "    </div>\r\n";
}

void HttpConfig::PrintFooter(QTextStream &os)
{
    os << "    <div class=\"form_buttons_bottom\"\r\n"
       << "         id=\"form_buttons_bottom\">\r\n"
       << "      <input type=\"submit\" value=\"Save Changes\" />\r\n"
       << "    </div>\r\n"
       << "  </form>\r\n"
       << "</body>\r\n"
       << "</html>\r\n";
}

void HttpConfig::PrintSettings(QTextStream &os, const MythSettingList &settings)
{
    MythSettingList::const_iterator it = settings.begin();
    for (; it != settings.end(); ++it)
        os << (*it)->ToHTML(1);
}
