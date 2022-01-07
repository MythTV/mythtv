// MythTV
#include "http/mythmimetype.h"

MythMimeType::MythMimeType(const QMimeType& MimeType)
  : m_valid(MimeType.isValid()),
    m_qtType(true),
    m_mime(MimeType)
{
}
MythMimeType::MythMimeType(QString Name, QString Suffix,
                           MimeMagic Magic, uint Weight, QStringList Inherits)
  : m_valid(!Name.isEmpty()),
    m_name(std::move(Name)),
    m_suffix(std::move(Suffix)),
    m_magic(std::move(Magic)),
    m_weight(Weight),
    m_inherits(std::move(Inherits))
{
}

bool MythMimeType::operator==(const MythMimeType& Other) const
{
    return this->m_name == Other.Name();
}

bool MythMimeType::IsValid() const
{
    return m_valid;
}

QString MythMimeType::Name() const
{
    if (!m_alias.isEmpty())
        return m_alias;
    if (m_qtType)
        return m_mime.name();
    return m_name;
}

void MythMimeType::SetAlias(const QString &Name)
{
    m_alias = Name;
}

/*! \brief Return the preferred filename suffix used by this type.
 *
 * \note For consistency with QMimeType this returns the suffix alone (e.g. 'png')
 * but internally it is is stored as '.png' to simplify matching.
 * \note We always use a lowercase suffix.
*/
QString MythMimeType::Suffix() const
{
    if (m_qtType)
        return m_mime.preferredSuffix();
    return m_suffix.mid(1);
}

QStringList MythMimeType::Aliases() const
{
    if (m_qtType)
        return { m_mime.aliases() << m_mime.name() };
    return { m_name };
}

/*! \brief Does this type inherit from the given type.
 *
 * This is a simple string match (i.e. much simpler that the Qt version).
*/
bool MythMimeType::Inherits(const QString &Name) const
{
    if (m_qtType)
        return m_mime.inherits(Name);
    return m_inherits.contains(Name);
}
