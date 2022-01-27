// MythTV
#include "mythdirs.h"
#include "http/mythhttproot.h"
#include "http/mythhttpdata.h"
#include "http/mythhttpfile.h"
#include "http/mythhttprequest.h"
#include "http/mythhttpresponse.h"

#define INDEX QStringLiteral("index.html")

/*! \brief A convenience method to seemlessly redirect requests for index.html to
 * a context specific file.
 *
 * e.g. assuming 'mythfrontend.html' points to a valid file:
 *
 * \code{.cpp}
 *    auto frontend = [](HTTPRequest2 Request, const QString& Root)
 *    {
 *        return MythHTTPRoot::RedirectRoot(Request, Root, "mythfrontend.html");
 *    };
 *    MythHTTPService::AddHandlers( {{"/", frontend }});
 * \endcode
*/
HTTPResponse MythHTTPRoot::RedirectRoot(const HTTPRequest2& Request, const QString &File)
{
    auto result = static_cast<HTTPResponse>(nullptr);
    if (!Request)
        return result;

    // this is the top level handler. We deal with the empty root request
    // and index.html
    if (Request->m_fileName.isEmpty())
        Request->m_fileName = INDEX;
    if (Request->m_fileName != INDEX)
        return result;
    Request->m_allowed = HTTP_DEFAULT_ALLOWED | HTTPPut | HTTPDelete;

    result = MythHTTPResponse::HandleOptions(Request);
    if (result)
        return result;

    if (!File.isEmpty())
    {
        Request->m_fileName = File;
        result = MythHTTPFile::ProcessFile(Request);
        // Rename the file
        if (auto * file = std::get_if<HTTPFile>(&result->m_response))
            (*file)->m_fileName = INDEX;
    }
    else
    {
        auto data = MythHTTPData::Create("index.html", s_defaultHTTPPage.arg("MythTV").toUtf8().constData());
        result = MythHTTPResponse::DataResponse(Request, data);
    }
    return result;

}
