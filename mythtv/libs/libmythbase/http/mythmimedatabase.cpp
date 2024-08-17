// Qt
#include <QBuffer>
#include <QIODevice>
#include <QGlobalStatic>
#include <QMimeDatabase>
#include <QStandardPaths>

// MythTV
#include "mythlogging.h"
#include "http/mythmimedatabase.h"

#define LOC QString("MimeDB: ")
static constexpr qint64 PEEK_SIZE { 100 };

/*! \class MythMimeDatabasePriv
 * \brief A private, internal class that holds custom mime types.
 *
 * \note This is created as a static singleton on first use.
*/
class MythMimeDatabasePriv
{
    using MimeDesc = std::tuple<QString,QString,MimeMagic,uint,QStringList>;

  public:
    MythMimeDatabasePriv()
    {
        auto paths =
            QStandardPaths::locateAll(QStandardPaths::GenericDataLocation,
                                      QLatin1String("mime/packages"),
                                      QStandardPaths::LocateDirectory);
        LOG(VB_GENERAL, LOG_DEBUG, LOC +
            QString("Standard paths: %1").arg(paths.join(", ")));

        static const std::vector<MimeDesc> s_types =
        {
            { "application/cbor", ".cbor", { std::monostate() }, 90, {}},
            { "application/x-www-form-urlencoded", ".json",  { std::monostate() }, 10, { "text/plain" }},
            { "application/x-plist",               ".plist", { QByteArrayLiteral("bplist00") }, 50, {}},
            { "application/x-apple-binary-plist",  ".plist", { QByteArrayLiteral("bplist00") }, 50, {}},
            { "text/x-apple-plist+xml",            ".plist", { QStringLiteral("http://www.apple.com/DTDs/PropertyList-1.0.dtd") },
              50, { "text/plain", "application/xml"}}
        };

        LOG(VB_GENERAL, LOG_DEBUG, LOC +
            QString("Custom entries: %1").arg(s_types.size()));
        // cppcheck-suppress unassignedVariable
        for (const auto & [name, suffix, magic, weight, inherits] : s_types)
            m_mimes.push_back({name, suffix, magic, weight, inherits});
    }

    const MythMimeTypes& AllTypes() const
    {
        return m_mimes;
    }

    MythMimeType MimeTypeForName(const QString& Name) const
    {
        auto found = std::find_if(m_mimes.cbegin(), m_mimes.cend(),
            [&Name](const auto & Mime) { return Name.compare(Mime.Name(), Qt::CaseInsensitive) == 0; });
        if (found != m_mimes.cend())
            return *found;
        return MythMimeType {};
    }

    MythMimeTypes MimeTypesForFileName(const QString &FileName) const
    {
        MythMimeTypes result;
        auto name = FileName.trimmed().toLower();
        for (const auto & mime : m_mimes)
            if (name.endsWith(mime.m_suffix))
                result.push_back(mime);
        return result;
    }

    QString SuffixForFileName(const QString &FileName) const
    {
        auto name = FileName.trimmed().toLower();
        for (const auto & mime : m_mimes)
            if (name.endsWith(mime.m_suffix))
                return mime.Suffix();
        return ".";
    }

    static MythMimeType MagicSearch(const MythMimeTypes& Types, const QByteArray& Data)
    {
        MythMimeType result;
        for (const auto & mime : Types)
        {
            if (const auto * bytes = std::get_if<QByteArray>(&mime.m_magic); bytes)
            {
                if (Data.contains(*bytes) && (mime.m_weight > result.m_weight))
                    result = mime;
            }
            else if (const auto * string = std::get_if<QString>(&mime.m_magic); string)
            {
                if (QString(Data.constData()).contains(*string, Qt::CaseInsensitive) && (mime.m_weight > result.m_weight))
                    result = mime;
            }
        }
        return result;
    }

    MythMimeType MimeTypeForFileNameAndData(const QString& FileName, QIODevice* Device) const
    {
        // Try and match by extension
        auto filtered = MimeTypesForFileName(FileName);

        // If we have a single extension match, return it
        if (filtered.size() == 1)
            return filtered.front();

        // If device is not opened, open it
        bool wasopen = Device->isOpen();
        if (!wasopen)
            Device->open(QIODevice::ReadOnly);

        // Peek at the contents
        auto peek = Device->peek(PEEK_SIZE);

        // Use matched types or fallback to all for magic search
        auto result = MagicSearch(filtered.empty() ? AllTypes() : filtered, peek);

        // Close again if necessary
        if (!wasopen)
            Device->close();

        return result;
    }

    static MythMimeTypes ToMythMimeTypes(const QList<QMimeType>& Types)
    {
        MythMimeTypes result;
        std::transform(Types.cbegin(), Types.cend(), std::back_inserter(result),
                       [](const auto& Type) { return MythMimeType { Type }; });
        return result;
    }

  private:
    MythMimeTypes m_mimes;
};

//NOLINTNEXTLINE(readability-redundant-member-init)
Q_GLOBAL_STATIC(MythMimeDatabasePriv, s_mimeDB)

/*! \class MythMimeDatabase
 * \brief A wrapper around QMimeDatabase that supports additional mime types.
 *
 * QMimeDatabase by default uses a list of MIME types provided by Freedesktop.org.
 * Unfortunately it does not include a few MIME types (most notably the Apple
 * plist types) and we cannot inherit from either QMimeDatabase or QMimeType to
 * add new types. Qt does have a mechanism for adding additional types by extending
 * the search path it uses. So for example, on unix systems, we could add our
 * custom types to /usr/local/share/mythtv/mime/packages and add that path to
 * XDG_DATA_DIRS (and this does work). It is not clear however how this would work
 * on other platforms. On Android, for example, the 'GenericDataLocation' in
 * QStandardPaths defaults to the USER directory.
*/
MythMimeDatabase::MythMimeDatabase()
  : m_priv(s_mimeDB)
{
}

/*! \brief Return a vector containing all of the known types (both Qt and MythTV)
*/
MythMimeTypes MythMimeDatabase::AllTypes()
{
    auto all = MythMimeDatabasePriv::ToMythMimeTypes(QMimeDatabase().allMimeTypes());
    const auto & mythtv = s_mimeDB->AllTypes();
    all.insert(all.cend(), mythtv.cbegin(), mythtv.cend());
    return all;
}

/*! \brief Return a vector of mime types that match the given filename.
*/
MythMimeTypes MythMimeDatabase::MimeTypesForFileName(const QString &FileName)
{
    auto result = MythMimeDatabasePriv::ToMythMimeTypes(QMimeDatabase().mimeTypesForFileName(FileName));
    const auto mythtv = s_mimeDB->MimeTypesForFileName(FileName);
    result.insert(result.cend(), mythtv.cbegin(), mythtv.cend());
    return result;
}

/*! \brief Return the preferred suffix for the given filename.
 *
 * \note The suffix does not include any 'dots' and is always lower case.
*/
QString MythMimeDatabase::SuffixForFileName(const QString &FileName)
{
    if (auto result = QMimeDatabase().suffixForFileName(FileName); !result.isEmpty())
        return result;
    return s_mimeDB->SuffixForFileName(FileName);
}

/*! \brief Return a mime type that matches the given name.
 *
 * \note We prioritise internal types over Qt types as our internal types
 * are either non-ambiguous (e.g. cbor) or are generated internally (and we thus
 * know their type).
 * \note If a suitable mime type is not found, an invalid type is returned.
*/
MythMimeType MythMimeDatabase::MimeTypeForName(const QString &Name)
{
    if (auto result = s_mimeDB->MimeTypeForName(Name); result.IsValid())
        return result;
    return QMimeDatabase().mimeTypeForName(Name);
}

/*! \brief Return a mime type for the given FileName and data.
 *
 * As for QMimeDatabase, this method will default to using the file name where
 * it is unambiguous and only probe the given data when further help is needed.
 *
 * MythTV types are probed first.
 *
 * \note A valid mimetype is always returned (the default being 'application/octet-stream')
*/
MythMimeType MythMimeDatabase::MimeTypeForFileNameAndData(const QString& FileName, const QByteArray& Data)
{
    if (auto result = QMimeDatabase().mimeTypeForFileNameAndData(FileName, Data); result.isValid())
        return result;
    QBuffer buffer(const_cast<QByteArray*>(&Data));
    return s_mimeDB->MimeTypeForFileNameAndData(FileName, &buffer);
}

MythMimeType MythMimeDatabase::MimeTypeForFileNameAndData(const QString& FileName, QIODevice* Device)
{
    if (auto result = QMimeDatabase().mimeTypeForFileNameAndData(FileName, Device); result.isValid())
        return result;
    return s_mimeDB->MimeTypeForFileNameAndData(FileName, Device);
}
