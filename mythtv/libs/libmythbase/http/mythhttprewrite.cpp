// MythTV
#include "mythdirs.h"
#include "mythlogging.h"
#include "http/mythhttprewrite.h"
#include "http/mythhttpdata.h"
#include "http/mythhttpfile.h"
#include "http/mythhttprequest.h"
#include "http/mythhttpresponse.h"


/*! \brief A convenience method to seemlessly redirect requests for files to
 * a context specific file.
 *
 * e.g. we want requests for /main.js to be served from /apps/backend/main.js
 *
 * \code{.cpp}
 *    auto main_js = [](auto && PH1) { return MythHTTPRewrite::RewriteFile(std::forward<decltype(PH1)>(PH1), "apps/backend/main.js"); };
 *
 *    MythHTTPService::AddHandlers( {{"/main.js", main_js }});
 * \endcode
*/
HTTPResponse MythHTTPRewrite::RewriteFile(const HTTPRequest2& Request, const QString &File)
{
    auto result = static_cast<HTTPResponse>(nullptr);
    if (!Request)
        return result;

    LOG(VB_HTTP, LOG_INFO, QString("Rewriting request to new file '%1'").arg(File));
    if (!File.isEmpty())
    {
        Request->m_fileName = File;
        result = MythHTTPFile::ProcessFile(Request);
    }
    return result;

}

/*! \brief A convenience method to seemlessly redirect requests to
 * a Single Page web app (SPA)
 *
 * e.g. we want all requests not handled by API or static methods to
 * be sent into the web app
 *
 * \code{.cpp}
 *    auto spa_index = [](auto && PH1) { return MythHTTPRewrite::RewriteToSPA(std::forward<decltype(PH1)>(PH1), "apps/backend/index.html"); };
 *
 *    MythHTTPService::AddErrorPageHandler( {{"=404", spa_index }});
 * \endcode
*/
HTTPResponse MythHTTPRewrite::RewriteToSPA(const HTTPRequest2& Request, const QString &File)
{
    auto result = static_cast<HTTPResponse>(nullptr);
    if (!Request)
        return result;

    LOG(VB_HTTP, LOG_INFO, QString("Rewriting request to web app '%1'").arg(File));
    if (!File.isEmpty()
        && (Request->m_path.isEmpty() || Request->m_path == "/"
            || Request->m_path == "/dashboard/" || Request->m_path == "/setupwizard/"
            || Request->m_path == "/MythFE/"))
    {
        Request->m_fileName = File;
        Request->m_status = HTTPOK;
        Request->m_path = "/";
        result = MythHTTPFile::ProcessFile(Request);
    }
    return result;

}
