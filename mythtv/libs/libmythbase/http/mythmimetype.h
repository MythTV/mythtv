#ifndef MYTHMIMETYPE_H
#define MYTHMIMETYPE_H

// Qt
#include <QMimeType>
#include <QStringList>

// Std
#include <variant>
#include <vector>

class MythMimeType;
using MythMimeTypes = std::vector<MythMimeType>;
using MimeMagic = std::variant<std::monostate,QByteArray,QString>;

class MythMimeType
{
    friend class MythMimeDatabase;
    friend class MythMimeDatabasePriv;

  public:
    MythMimeType() = default;
    bool operator==(const MythMimeType& Other) const;

    bool    IsValid() const;
    QString Name() const;
    QString Suffix() const;
    QStringList Aliases() const;
    bool    Inherits(const QString& Name) const;
    void    SetAlias(const QString& Name);

  protected:
    MythMimeType(const QMimeType& MimeType);
    MythMimeType(QString Name, QString Suffix,
                 MimeMagic Magic, uint Weight, QStringList Inherits);

    bool        m_valid  { false };
    QString     m_name;
    QString     m_alias;
    QString     m_suffix;
    MimeMagic   m_magic  { std::monostate() };
    uint        m_weight { 0 };
    QStringList m_inherits;
    bool        m_qtType { false };
    QMimeType   m_mime;
};

#endif
