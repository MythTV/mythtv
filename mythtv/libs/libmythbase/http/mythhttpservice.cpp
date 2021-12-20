#include <QFileInfo>

// MythTV
#include "mythlogging.h"
#include "mythdate.h"
#include "http/mythwsdl.h"
#include "http/mythhttpservice.h"
#include "http/mythhttprequest.h"
#include "http/mythhttpresponse.h"
#include "http/serialisers/mythserialiser.h"
#include "http/mythhttpencoding.h"
#include "http/mythhttpmetaservice.h"

#define LOC QString("HTTPService: ")

MythHTTPService::MythHTTPService(MythHTTPMetaService *MetaService)
  : m_name(MetaService->m_name),
    m_staticMetaService(MetaService)
{
}

/*! \brief Respond to a valid HTTPRequest
 *
 * \todo Error message always send an HTML version of the error message. This
 * should probably be context specific.
*/
HTTPResponse MythHTTPService::HTTPRequest(HTTPRequest2 Request)
{
    QString& method = Request->m_fileName;
    if (method.isEmpty())
        return nullptr;
    m_request = Request;
    // WSDL
    if (method == "wsdl") {
        MythWSDL wsdl( m_staticMetaService );
        return wsdl.GetWSDL( Request );
    }
    if ( method == "xsd" )
    {
        MythXSD xsd;
        if (Request->m_queries.contains( "type" ))
            return xsd.GetXSD( Request, Request->m_queries.value("type"));
        // The xsd for enums does not work, so it is commented for now.
        // else
        //     return xsd.GetEnumXSD( Request, Request->m_queries.value("enum"));
    }

    // Find the method
    LOG(VB_HTTP, LOG_DEBUG, LOC + QString("Looking for method '%1'").arg(method));
    HTTPMethodPtr handler = nullptr;
    for (auto & [path, handle] : m_staticMetaService->m_slots)
        if (path == method) { handler = handle; break; }

    if (handler == nullptr)
    {
        // Should we just return not found here rather than falling through
        // to all of the other handlers? Do we need other handlers?
        LOG(VB_HTTP, LOG_DEBUG, LOC + "Failed to find method");
        return nullptr;
    }

    // Authentication required
    if (handler->m_protected)
    {
        LOG(VB_HTTP, LOG_INFO, LOC + "Authentication required for this call");
    }

    // Sanity check type count (handler should have the return type at least)
    if (handler->m_types.empty())
        return nullptr;

    // Handle options
    Request->m_allowed = handler->m_requestTypes;
    if (HTTPResponse options = MythHTTPResponse::HandleOptions(Request))
        return options;

    // Parse the parameters and match against those expected by the method.
    // As for the old code, this allows parameters to be missing and they will
    // thus be allocated a default/null/value.
    size_t typecount = std::min(handler->m_types.size(), static_cast<size_t>(100));

    // Build parameters list
    // Note: We allow up to 100 args but anything above Q_METAMETHOD_INVOKE_MAX_ARGS
    // will be ignored
    std::array<void*, 100> param { nullptr};
    std::array<int,   100> types { 0 };

    // Return type
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    param[0] = handler->m_types[0] == 0 ? nullptr : QMetaType::create(handler->m_types[0]);
#else
    param[0] = handler->m_types[0] == 0 ? nullptr : QMetaType(handler->m_types[0]).create();
#endif
    types[0] = handler->m_types[0];

    // Parameters
    // Iterate over the method's parameters and search for the incoming values...
    size_t count = 1;
    QString error;
    while (count < typecount)
    {
        auto name  = handler->m_names[count];
        auto value = Request->m_queries.value(name.toLower(), "");
        auto type  = handler->m_types[count];
        types[count] = type;
        // These should be filtered out in MythHTTPMetaMethod
        if (type == 0)
        {
            error = QString("Unknown parameter type '%1'").arg(name);
            break;
        }

#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
        auto * newparam = QMetaType::create(type);
#else
        auto * newparam = QMetaType(type).create();
#endif
        param[count] = handler->CreateParameter(newparam, type, value);
        count++;
    }

    HTTPResponse result = nullptr;
    if (count == typecount)
    {
        // Invoke
        QVariant returnvalue;
        try {
            if (qt_metacall(QMetaObject::InvokeMetaMethod, handler->m_index, param.data()) >= 0)
                LOG(VB_GENERAL, LOG_ERR, "qt_metacall error");
            else
            {
                // Retrieve result
                returnvalue = handler->CreateReturnValue(types[0], param[0]);
            }
        }
        catch( QString &msg ) {
            LOG(VB_GENERAL, LOG_ERR, "Service Exception: " + msg);
            Request->m_status = HTTPBadRequest;
            result = MythHTTPResponse::ErrorResponse(Request, msg);
        }
        catch (V2HttpRedirectException &ex) {
            result = MythHTTPResponse::RedirectionResponse(Request, ex.m_hostName);
        }

        if (!returnvalue.isValid())
        {
            if (!result)
                result = MythHTTPResponse::ErrorResponse(Request, "Unknown Failure");
        }
        else if (returnvalue.canConvert<QFileInfo>())
        {
            if (!result)
            {
                auto info = returnvalue.value<QFileInfo>();
                QString file = info.absoluteFilePath();
                if (file.size() == 0)
                {
                    LOG(VB_GENERAL, LOG_WARNING, LOC + QString("Invalid request for unknown file"));
                    Request->m_status = HTTPNotFound;
                    result =  MythHTTPResponse::ErrorResponse(Request);
                }
                else
                {
                    HTTPFile httpfile = MythHTTPFile::Create(info.fileName(),file);
                    if (!httpfile->open(QIODevice::ReadOnly))
                    {
                        LOG(VB_GENERAL, LOG_WARNING, LOC + QString("Failed to open '%1'").arg(file));
                        Request->m_status = HTTPNotFound;
                        result =  MythHTTPResponse::ErrorResponse(Request);
                    }
                    else
                    {
                        httpfile->m_lastModified = info.lastModified();
                        httpfile->m_cacheType = HTTPLastModified | HTTPLongLife;
                        LOG(VB_HTTP, LOG_DEBUG, LOC + QString("Last modified: %2")
                            .arg(MythDate::toString(httpfile->m_lastModified, MythDate::kOverrideUTC | MythDate::kRFC822)));
                        // Create our response
                        result = MythHTTPResponse::FileResponse(Request, httpfile);
                    }
                }
            }
        }
        else
        {
            auto accept = MythHTTPEncoding::GetMimeTypes(MythHTTP::GetHeader(Request->m_headers, "accept"));
            HTTPData content = MythSerialiser::Serialise(handler->m_returnTypeName, returnvalue, accept);
            content->m_cacheType = HTTPETag | HTTPShortLife;
            result = MythHTTPResponse::DataResponse(Request, content);

            // If the return type is QObject* we need to cleanup
            if (returnvalue.canConvert<QObject*>())
            {
                LOG(VB_HTTP, LOG_DEBUG, LOC + "Deleting object");
                auto * object = returnvalue.value<QObject*>();
                delete object;
            }
        }
    }

    // Cleanup
    for (size_t i = 0; i < typecount; ++i)
    {
        if ((param[i] != nullptr) && (types[i] != 0))
        {
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
            QMetaType::destroy(types[i], param[i]);
#else
            QMetaType(types[i]).destroy(param[i]);
#endif
        }
    }

    // Return the previous error
    if (count != typecount)
    {
        LOG(VB_HTTP, LOG_ERR, LOC + error);
        Request->m_status = HTTPBadRequest;
        return MythHTTPResponse::ErrorResponse(Request, error);
    }

    // Valid result...
    return result;
}


QString& MythHTTPService::Name()
{
    return m_name;
}
