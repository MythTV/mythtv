// Qt
#include <QMetaProperty>
// MythTV
#include "mythlogging.h"
#include "http/mythmimedatabase.h"
#include "http/mythhttpdata.h"
#include "http/serialisers/mythxmlserialiser.h"
#include "http/serialisers/mythxmlplistserialiser.h"
#include "http/serialisers/mythjsonserialiser.h"
#include "http/serialisers/mythcborserialiser.h"
#include "http/serialisers/mythserialiser.h"

MythSerialiser::MythSerialiser()
  : m_result(MythHTTPData::Create())
{
    m_buffer.setBuffer(static_cast<QByteArray*>(m_result.get()));
    m_buffer.open(QIODevice::WriteOnly);
}

HTTPData MythSerialiser::Result()
{
    return m_result;
}

/*! \brief Serialise the given data with an encoding suggested by Accept
*/
HTTPData MythSerialiser::Serialise(const QString &Name, const QVariant& Value, const QStringList &Accept)
{
    /*
    auto types = MythMimeDatabase().AllTypes();
    QStringList names;
    for (const auto & type : types)
        names.append(type.Name());
    names.sort();
    for (const auto & name : names)
        LOG(VB_GENERAL, LOG_INFO, name);
    */

    static const MythMimeType s_xmlType = MythMimeDatabase::MimeTypeForName("application/xml");
    static const MythMimeType s_xmlPList = MythMimeDatabase::MimeTypeForName("text/x-apple-plist+xml");
    static const MythMimeType s_cbor = MythMimeDatabase::MimeTypeForName("application/cbor");

    auto WrapData = [](HTTPData Data, const MythMimeType& Mime, const QString& Alias)
    {
        Data->m_fileName = "result." + Mime.Suffix();
        Data->m_mimeType = Mime;
        Data->m_mimeType.SetAlias(Alias);
        return Data;
    };

    static const std::array<MythMimeType,2> s_jsonTypes =
    {
        MythMimeDatabase::MimeTypeForName("application/json"),
        MythMimeDatabase::MimeTypeForName("text/javascript")
    };

    static const std::array<MythMimeType,2> s_binaryPlists =
    {
        MythMimeDatabase::MimeTypeForName("application/x-plist"),
        MythMimeDatabase::MimeTypeForName("application/x-apple-binary-plist")
    };

    // Check for preformatted xml or html
    if (Value.value<QObject*>())
    {
        auto * Object = Value.value<QObject*>();
        const auto * meta = Object->metaObject();
        int index = meta->indexOfClassInfo("preformat");
        if (index >= 0
            && QString("true") == meta->classInfo(index).value())
        {
            HTTPData result = MythHTTPData::Create();
            QBuffer buffer;
            buffer.setBuffer(static_cast<QByteArray*>(result.get()));
            buffer.open(QIODevice::WriteOnly);
            QString mimetype;
            int count = meta->propertyCount();
            for (int ix = 0; ix  < count; ++ix  )
            {
                QMetaProperty metaproperty = meta->property(ix);
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
                bool user = metaproperty.isUser(Object);
#else
                bool user = metaproperty.isUser();
#endif
                if (user)
                {
                    const char *rawname = metaproperty.name();
                    QString name(rawname);
                    QVariant value(Object->property(rawname));
                    if (name.compare("mimetype") == 0)
                    {
                        mimetype = value.toString();
                        continue;
                    }
                    if (name.compare("buffer") == 0)
                    {
                        buffer.write(value.toString().toUtf8());
                        continue;
                    }
                }
            }
            MythMimeType reqType = MythMimeDatabase::MimeTypeForName(mimetype);
            auto alias = reqType.Aliases().indexOf(mimetype);
            return WrapData(result, reqType, reqType.Aliases().at(alias));
        }
    }

    for (const auto & mime : Accept)
    {
        LOG(VB_HTTP, LOG_DEBUG, QString("Looking for serialiser for '%1'").arg(mime));
        for (const auto & jsontype : s_jsonTypes)
        {
            if (const auto & alias = jsontype.Aliases().indexOf(mime); alias >= 0)
                if (MythJSONSerialiser json(Name, Value); json.Result() != nullptr)
                    return WrapData(json.Result(), jsontype, jsontype.Aliases().at(alias));
        }

        if (const auto & alias = s_xmlType.Aliases().indexOf(mime); alias >= 0)
            if (MythXMLSerialiser xml(Name, Value); xml.Result() != nullptr)
                return WrapData(xml.Result(), s_xmlType, s_xmlType.Aliases().at(alias));

        if (const auto & alias = s_xmlPList.Aliases().indexOf(mime); alias >= 0)
            if (MythXMLPListSerialiser plist(Name, Value); plist.Result() != nullptr)
                return WrapData(plist.Result(), s_xmlPList, s_xmlPList.Aliases().at(alias));

        if (const auto & alias = s_cbor.Aliases().indexOf(mime); alias >= 0)
            if (MythCBORSerialiser cbor(Name, Value); cbor.Result() != nullptr)
                return WrapData(cbor.Result(), s_cbor, s_cbor.Aliases().at(alias));
    }

    // Default to XML
    MythXMLSerialiser xml(Name, Value);
    return WrapData(xml.Result(), s_xmlType, s_xmlType.Name());
}
