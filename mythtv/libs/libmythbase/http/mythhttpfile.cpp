// Qt
#include <QFile>
#include <QFileInfo>

// MythTV
#include "mythlogging.h"
#include "mythdate.h"
#include "http/mythhttpresponse.h"
#include "http/mythhttprequest.h"
#include "http/mythhttpfile.h"

#define LOC QString("HTTPFile: ")

HTTPFile MythHTTPFile::Create(const QString &ShortName, const QString& FullName)
{
    return std::shared_ptr<MythHTTPFile>(new MythHTTPFile(ShortName, FullName));
}

/*! \class MythHTTPfile
 * \brief A simple wrapper around QFile
*/

/*! \brief Default constructor
 *
 * \param ShortName The filename will be shown in the 'Content-Disposition' header
 * (i.e. just the name and extension with all path elements removed).
 * \param FullName The full path to the file on disk.
*/
MythHTTPFile::MythHTTPFile(const QString& ShortName, const QString& FullName)
  : QFile(FullName),
    MythHTTPContent(ShortName)
{
}

HTTPResponse MythHTTPFile::ProcessFile(HTTPRequest2 Request)
{
    // Build full path
    QString file = Request->m_root + Request->m_path + Request->m_fileName;

    // Process options requests
    auto response = MythHTTPResponse::HandleOptions(Request);
    if (response)
        return response;

    // Ensure the file exists
    LOG(VB_HTTP, LOG_INFO, LOC + QString("Looking for '%1'").arg(file));

    if (!QFileInfo::exists(file))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + QString("Failed to find '%1'").arg(file));
        Request->m_status = HTTPNotFound;
        return MythHTTPResponse::ErrorResponse(Request);
    }

    // Try and open
    auto httpfile = MythHTTPFile::Create(Request->m_fileName, file);
    if (!httpfile->open(QIODevice::ReadOnly))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + QString("Failed to open '%1'").arg(file));
        Request->m_status = HTTPNotFound;
        return MythHTTPResponse::ErrorResponse(Request);
    }
    httpfile->m_lastModified = QFileInfo(file).lastModified();
    httpfile->m_cacheType = HTTPLastModified | HTTPLongLife;

    LOG(VB_HTTP, LOG_DEBUG, LOC + QString("Last modified: %2")
        .arg(MythDate::toString(httpfile->m_lastModified, MythDate::kOverrideUTC | MythDate::kRFC822)));

    // Create our response
    response = MythHTTPResponse::FileResponse(Request, httpfile);
    // Assume static content
    return response;
}
