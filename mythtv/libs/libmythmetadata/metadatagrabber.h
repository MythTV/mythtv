#ifndef METADATAGRABBER_H_
#define METADATAGRABBER_H_

#include <QList>
#include <QString>
#include <QVariant>
#include <QStringList>
#include <QDomDocument>

#include "mythtypes.h"
#include "mythmetaexp.h"
//#include "metadatacommon.h"
#include "referencecounterlist.h"
class MetadataLookup;
typedef RefCountedList<MetadataLookup> MetadataLookupList;

class MetaGrabberScript;
typedef QList<MetaGrabberScript> GrabberList;

enum GrabberType {
    kGrabberAll,
    kGrabberMovie,
    kGrabberTelevision,
    kGrabberMusic,
    kGrabberGame,
    kGrabberInvalid
};

class META_PUBLIC MetaGrabberScript : public QObject
{
  public:
    MetaGrabberScript();
    MetaGrabberScript(const QDomElement &dom);
    MetaGrabberScript(const QString &path);
    MetaGrabberScript(const QString &path, const QDomElement &dom);
    MetaGrabberScript(const MetaGrabberScript &other);

    MetaGrabberScript& operator=(const MetaGrabberScript &other);

    static GrabberList          GetList(bool refresh=false);
    static GrabberList          GetList(const QString &type, bool refresh=false);
    static GrabberList          GetList(GrabberType type,
                                        bool refresh=false);

    static MetaGrabberScript    GetGrabber(GrabberType defaultType,
                                           const MetadataLookup *lookup = NULL);
    static MetaGrabberScript    GetType(const QString &type);
    static MetaGrabberScript    GetType(GrabberType type);
    static MetaGrabberScript    FromTag(const QString &tag,
                                        bool absolute=false);
    static MetaGrabberScript    FromInetref(const QString &inetref,
                                            bool absolute=false);
    static QString              CleanedInetref(const QString &inetref);

    bool          IsValid(void) const         { return m_valid; }

    QString       GetCommand(void) const      { return m_command; }
    QString       GetRelPath(void) const;
    QString       GetPath(void) const         { return m_fullcommand; }

    QString       GetName(void) const         { return m_name; }
    QString       GetAuthor(void) const       { return m_author; }
    QString       GetThumbnail(void) const    { return m_thumbnail; }
    GrabberType   GetType(void) const         { return m_type; }
    QString       GetTypeString(void) const   { return m_typestring; }
    QString       GetDescription(void) const  { return m_description; }

    bool Accepts(const QString &tag) const { return m_accepts.contains(tag); }

    void          toMap(InfoMap &metadataMap) const;

    bool                Test(void);
    MetadataLookupList  Search(const QString &title, MetadataLookup *lookup, bool passseas=true);
    MetadataLookupList  SearchSubtitle(const QString &title, const QString &subtitle, MetadataLookup *lookup, bool passseas=true);
    MetadataLookupList  SearchSubtitle(const QString &inetref, const QString &title, const QString &subtitle, MetadataLookup *lookup, bool passseas=true);
    MetadataLookupList  LookupData(const QString &inetref, MetadataLookup *lookup, bool passseas=true);
    MetadataLookupList  LookupData(const QString &inetref, int season, int episode, MetadataLookup *lookup, bool passseas=true);
    MetadataLookupList  LookupCollection(const QString &collectionref, MetadataLookup *lookup, bool passseas=true);

  private:
    QString m_name;
    QString m_author;
    QString m_thumbnail;
    QString m_fullcommand;
    QString m_command;
    GrabberType m_type;
    QString m_typestring;
    QString m_description;
    QStringList m_accepts;
    float m_version;
    bool m_valid;

    void ParseGrabberVersion(const QDomElement &item);
    MetadataLookupList RunGrabber(const QStringList &args, MetadataLookup *lookup, bool passseas);
    void SetDefaultArgs(QStringList &args);
};

Q_DECLARE_METATYPE(MetaGrabberScript*)

#endif // METADATAGRABBER_H_
