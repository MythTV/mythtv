// MythTV
#include "mythlogging.h"
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

MythHTTPService::~MythHTTPService()
{
    // TODO Signal to clients that the service is closing?
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
    if (method == "wsdl")
        return MythWSDL::GetWSDL(Request, m_staticMetaService);

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

    // Sanity check type count (handler should have the return type at least)
    if (handler->m_types.size() < 1)
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
    param[0] = handler->m_types[0] == 0 ? nullptr : QMetaType::create(handler->m_types[0]);
    types[0] = handler->m_types[0];

    // Parameters
    // Iterate over the method's parameters and search for the incoming values...
    size_t count = 1;
    QString error;
    while (count < typecount)
    {
        auto name  = handler->m_names[count];
        auto value = Request->m_queries.value(name, "");
        auto type  = handler->m_types[count];
        types[count] = type;
        // These should be filtered out in MythHTTPMetaMethod
        if (type == 0)
        {
            error = QString("Unknown parameter type '%1'").arg(name);
            break;
        }

        auto newparam = QMetaType::create(type);
        param[count] = handler->CreateParameter(newparam, type, value);
        count++;
    }

    HTTPResponse result = nullptr;
    if (count == typecount)
    {
        // Invoke
        if (qt_metacall(QMetaObject::InvokeMetaMethod, handler->m_index, param.data()) >= 0)
            LOG(VB_GENERAL, LOG_WARNING, "qt_metacall error");

        // Retrieve result
        QVariant returnvalue = handler->CreateReturnValue(types[0], param[0]);
        if (!returnvalue.isValid())
        {
            result = MythHTTPResponse::EmptyResponse(Request);
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
                LOG(VB_GENERAL, LOG_INFO, LOC + "Deleting object");
                auto * object = returnvalue.value<QObject*>();
                delete object;
            }
        }
    }

    // Cleanup
    for (size_t i = 0; i < typecount; ++i)
        if ((param[i] != nullptr) && (types[i] != 0))
            QMetaType::destroy(types[i], param[i]);

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

bool MythHTTPService::Subscribe()
{
    return false;
}

void MythHTTPService::Unsubscribe()
{
}

QString& MythHTTPService::Name()
{
    return m_name;
}

QVariantMap MythHTTPService::GetServiceDescription()
{
    return {};
}

