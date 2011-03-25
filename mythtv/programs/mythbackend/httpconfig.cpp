// Qt headers
#include <QTextStream>

// MythTV headers
#include "httpconfig.h"
#include "backendutil.h"
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

    if (!request->m_sBaseUrl.startsWith("/Config"))
    {
        return false;
    }

    // Temporary until we get authentication enabled
    if (!getenv("MYTHHTMLSETUP"))
    {
        QTextStream os(&request->m_response);
        os << "<html><body><h3>Web-based Setup is currently disabled.</h3>"
              "</body></html>";

        request->m_eResponseType = ResponseTypeHTML;
        request->m_mapRespHeaders[ "Cache-Control" ] =
            "no-cache=\"Ext\", max-age = 0";

        VERBOSE(VB_IMPORTANT, QString("WARNING: Attempt to access "
                "Web-based Setup which is currently disabled. URL: %1")
                .arg(request->m_sResourceUrl));

        return true;
    }

    bool handled = false;
    if (request->m_sMethod == "Save")
    {
        // FIXME, this is always false, what's it for
        if (request->m_sBaseUrl.right(7) == "config" &&
            !database_settings.empty())
        {
            PrintHeader(request->m_response, "Config/Database");
            check_settings(database_settings, request->m_mapParams);
            load_settings(database_settings, "");
            PrintSettings(request->m_response, database_settings);
            PrintFooter(request->m_response);
            handled = true;
        }
        else
        {
            QTextStream os(&request->m_response);
            os << "<html><body><h3>The Save function for this screen is "
               << "not hooked up yet</h3><dl>";
            QStringMap::const_iterator it = request->m_mapParams.begin();
            for (; it!=request->m_mapParams.end(); ++it)
            {
                os << "<dt>"<<it.key()<<"</dt><dd>"
                                    <<*it<<"</dd>\r\n";
            }
            os << "</dl></body></html>";
            handled = true;
        }
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
    else if ((request->m_sMethod == "Database") || (NULL == gContext))
    {
        PrintHeader(request->m_response, "Config/Database");
        QString fn = GetShareDir() + "backend-config/"
            "config_backend_database.xml";
        parse_settings(database_settings, fn);
        load_settings(database_settings, "");
        PrintSettings(request->m_response, database_settings);
        PrintFooter(request->m_response);
        handled = true;
    }
    else if (request->m_sMethod == "General")
    {
        PrintHeader(request->m_response, "Config/General");
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

void HttpConfig::PrintHeader(QBuffer &buffer, const QString &form)
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
       << "  <h1 class=\"config\">MythTV Configuration</h1>\r\n"
       << "  <form id=\"config_form\" action=\"/" << form << "/Save\" method=\"POST\">\r\n"
       << "    <div class=\"form_buttons_top\"\r\n"
       << "         id=\"form_buttons_top\">\r\n"
       << "      <input type=\"submit\" value=\"Save Changes\" />\r\n"
       << "    </div>\r\n";
}

void HttpConfig::PrintFooter(QBuffer &buffer)
{
    QTextStream os(&buffer);

    os << "    <div class=\"form_buttons_bottom\"\r\n"
       << "         id=\"form_buttons_bottom\">\r\n"
       << "      <input type=\"submit\" value=\"Save Changes\" />\r\n"
       << "    </div>\r\n"
       << "  </form>\r\n"
       << "</div>\r\n"
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
