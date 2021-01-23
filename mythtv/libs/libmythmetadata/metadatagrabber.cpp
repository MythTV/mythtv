// Qt headers
#include <QDateTime>
#include <QDir>
#include <QMap>
#include <QMutex>
#include <QMutexLocker>
#include <QRegExp>
#include <utility>

// MythTV headers
#include "metadatagrabber.h"
#include "metadatacommon.h"
#include "mythsystemlegacy.h"
#include "exitcodes.h"
#include "mythdate.h"
#include "mythdirs.h"
#include "mythlogging.h"
#include "mythcorecontext.h"

#define LOC QString("Metadata Grabber: ")
#define kGrabberRefresh 60

static GrabberList     grabberList;
static QMutex          grabberLock;
static QDateTime       grabberAge;

struct GrabberOpts {
    QString     m_path;
    QString     m_setting;
    QString     m_def;
};

// TODO
// it would be nice to statically compile these, but I can't manage to get it
// to compile.  apparently initializer lists are supported in QT5/CPP11 that
// will make this work.  for now, use a lock and initialize on first access.
// https://bugreports.qt-project.org/browse/QTBUG-25679
static QMap<GrabberType, GrabberOpts> grabberTypes;
static QMap<QString, GrabberType> grabberTypeStrings;
static bool initialized = false;
static QMutex typeLock;

static GrabberOpts GrabberOptsMaker(QString thepath, QString thesetting, QString thedefault)
{
    GrabberOpts opts;

    opts.m_path = std::move(thepath);
    opts.m_setting = std::move(thesetting);
    opts.m_def = std::move(thedefault);

    return opts;
}

static void InitializeStaticMaps(void)
{
    QMutexLocker lock(&typeLock);

    if (!initialized)
    {
        grabberTypes[kGrabberMovie] =
              GrabberOptsMaker ("%1metadata/Movie/",
                                "MovieGrabber",
                                "metadata/Movie/tmdb3.py" );
        grabberTypes[kGrabberTelevision] =
             GrabberOptsMaker ( "%1metadata/Television/",
                                "TelevisionGrabber",
                                "metadata/Television/ttvdb.py" );
        grabberTypes[kGrabberGame]       =
             GrabberOptsMaker ( "%1metadata/Game/",
                                "mythgame.MetadataGrabber",
                                "metadata/Game/giantbomb.py" );
        grabberTypes[kGrabberMusic]      =
             GrabberOptsMaker ( "%1metadata/Music",
                                "",
                                "" );

        grabberTypeStrings["movie"]      = kGrabberMovie;
        grabberTypeStrings["television"] = kGrabberTelevision;
        grabberTypeStrings["game"]       = kGrabberGame;
        grabberTypeStrings["music"]      = kGrabberMusic;


        initialized = true;
    }
}

GrabberList MetaGrabberScript::GetList(bool refresh)
{
    return MetaGrabberScript::GetList(kGrabberAll, refresh);
}

GrabberList MetaGrabberScript::GetList(const QString &type, bool refresh)
{
    QString tmptype = type.toLower();
    if (!grabberTypeStrings.contains(tmptype))
        // unknown type, return empty list
        return GrabberList();

    return MetaGrabberScript::GetList(grabberTypeStrings[tmptype], refresh);
}

GrabberList MetaGrabberScript::GetList(GrabberType type,
                                       bool refresh)
{
    InitializeStaticMaps();

    GrabberList tmpGrabberList;
    GrabberList retGrabberList;
    {
        QMutexLocker listLock(&grabberLock);
        QDateTime now = MythDate::current();

        // refresh grabber scripts every 60 seconds
        // this might have to be revised, or made more intelligent if
        // the delay during refreshes is too great
        if (refresh || !grabberAge.isValid() ||
            (grabberAge.secsTo(now) > kGrabberRefresh))
        {
            grabberList.clear();
            LOG(VB_GENERAL, LOG_DEBUG, LOC + "Clearing grabber cache");

            // loop through different types of grabber scripts and the 
            // directories they are stored in
            QMap<GrabberType, GrabberOpts>::const_iterator it;
            for (it = grabberTypes.begin(); it != grabberTypes.end(); ++it)
            {
                QString path = (it->m_path).arg(GetShareDir());
                QStringList scripts = QDir(path).entryList(QDir::Executable | QDir::Files);
                if (scripts.count() == 0)
                    // no scripts found
                    continue;

                // loop through discovered scripts
                foreach (auto & name, scripts)
                {
                    QString cmd = QDir(path).filePath(name);
                    MetaGrabberScript script(cmd);

                    if (script.IsValid())
                    {
                        LOG(VB_GENERAL, LOG_DEBUG, LOC + "Adding " + script.m_command);
                        grabberList.append(script);
                    }
                 }
            }

            grabberAge = now;
        }

        tmpGrabberList = grabberList;
    }

    foreach (auto & item, tmpGrabberList)
    {
        if ((type == kGrabberAll) || (item.GetType() == type))
            retGrabberList.append(item);
    }

    return retGrabberList;
}

MetaGrabberScript MetaGrabberScript::GetGrabber(GrabberType defaultType,
                                                const MetadataLookup *lookup)
{
    if (!lookup)
    {
        return GetType(defaultType);
    }

    if (!lookup->GetInetref().isEmpty() &&
        lookup->GetInetref() != "00000000")
    {
        // inetref is defined, see if we have a pre-defined grabber
        MetaGrabberScript grabber = FromInetref(lookup->GetInetref());

        if (grabber.IsValid())
        {
            return grabber;
        }
        // matching grabber was not found, just use the default
        // fall through
    }

    return GetType(defaultType);
}

MetaGrabberScript MetaGrabberScript::GetType(const QString &type)
{
    QString tmptype = type.toLower();
    if (!grabberTypeStrings.contains(tmptype))
        // unknown type, return empty grabber
        return MetaGrabberScript();

    return MetaGrabberScript::GetType(grabberTypeStrings[tmptype]);
}

MetaGrabberScript MetaGrabberScript::GetType(const GrabberType type)
{
    InitializeStaticMaps();

    QString cmd = gCoreContext->GetSetting(grabberTypes[type].m_setting,
                                           grabberTypes[type].m_def);

    if (cmd.isEmpty())
    {
        // should the python bindings had not been installed at any stage
        // the settings could have been set to an empty string, so use default
        cmd = grabberTypes[type].m_def;
    }

    if (grabberAge.isValid() && grabberAge.secsTo(MythDate::current()) <= kGrabberRefresh)
    {
        // just pull it from the cache
        GrabberList list = GetList();
        foreach (auto & item, list)
            if (item.GetPath().endsWith(cmd))
                return item;
    }

    // polling the cache will cause a refresh, so lets just grab and
    // process the script directly
    QString fullcmd = QString("%1%2").arg(GetShareDir()).arg(cmd);
    MetaGrabberScript script(fullcmd);

    if (script.IsValid())
    {
        return script;
    }

    return MetaGrabberScript();
}

MetaGrabberScript MetaGrabberScript::FromTag(const QString &tag,
                                            bool absolute)
{
    GrabberList list = GetList();

    // search for direct match on tag
    foreach (auto & item, list)
    {
        if (item.GetCommand() == tag)
        {
            return item;
        }
    }

    // no direct match. do we require a direct match? search for one that works
    if (!absolute)
    {
        foreach (auto & item, list)
        {
            if (item.Accepts(tag))
            {
                return item;
            }
        }
    }

    // no working match. return a blank
    return MetaGrabberScript();
}

MetaGrabberScript MetaGrabberScript::FromInetref(const QString &inetref,
                                                 bool absolute)
{
    static QRegExp s_retagref("^([a-zA-Z0-9_\\-\\.]+\\.[a-zA-Z0-9]{1,3})_(.*)");
    static QRegExp s_retagref2("^([a-zA-Z0-9_\\-\\.]+\\.[a-zA-Z0-9]{1,3}):(.*)");
    static QMutex s_reLock;
    QMutexLocker lock(&s_reLock);
    QString tag;

    if (s_retagref.indexIn(inetref) > -1)
    {
        tag = s_retagref.cap(1);
    }
    else if (s_retagref2.indexIn(inetref) > -1)
    {
        tag = s_retagref2.cap(1);
    }
    if (!tag.isEmpty())
    {
        // match found, pull out the grabber
        MetaGrabberScript script = MetaGrabberScript::FromTag(tag, absolute);
        if (script.IsValid())
            return script;
    }

    // no working match, return a blank
    return MetaGrabberScript();
}

QString MetaGrabberScript::CleanedInetref(const QString &inetref)
{
    static QRegExp s_retagref("^([a-zA-Z0-9_\\-\\.]+\\.[a-zA-Z0-9]{1,3})_(.*)");
    static QRegExp s_retagref2("^([a-zA-Z0-9_\\-\\.]+\\.[a-zA-Z0-9]{1,3}):(.*)");
    static QMutex s_reLock;
    QMutexLocker lock(&s_reLock);

    // try to strip grabber tag from inetref
    if (s_retagref.indexIn(inetref) > -1)
        return s_retagref.cap(2);
    if (s_retagref2.indexIn(inetref) > -1)
        return s_retagref2.cap(2);

    return inetref;
}

MetaGrabberScript::MetaGrabberScript(QString path, const QDomElement &dom) :
    m_fullcommand(std::move(path))
{
    ParseGrabberVersion(dom);
}

MetaGrabberScript::MetaGrabberScript(const QDomElement &dom)
{
    ParseGrabberVersion(dom);
}

MetaGrabberScript::MetaGrabberScript(const QString &path)
{
    if (path.isEmpty())
        return;
    m_fullcommand = path;
    if (path[0] != '/')
        m_fullcommand.prepend(QString("%1metadata").arg(GetShareDir()));

    MythSystemLegacy grabber(path, QStringList() << "-v",
                             kMSRunShell | kMSStdOut);
    grabber.Run();
    if (grabber.Wait() != GENERIC_EXIT_OK)
        // script failed
        return;

    QByteArray result = grabber.ReadAll();
    if (result.isEmpty())
        // no output
        return;

    QDomDocument doc;
    doc.setContent(result, true);
    QDomElement root = doc.documentElement();
    if (root.isNull())
        // no valid XML
        return;

    ParseGrabberVersion(root);
    if (m_name.isEmpty())
        // XML not processed correctly
        return;

    m_valid = true;
}

MetaGrabberScript& MetaGrabberScript::operator=(const MetaGrabberScript &other)
{
    if (this != &other)
    {
        m_name = other.m_name;
        m_author = other.m_author;
        m_thumbnail = other.m_thumbnail;
        m_command = other.m_command;
        m_fullcommand = other.m_fullcommand;
        m_type = other.m_type;
        m_typestring = other.m_typestring;
        m_description = other.m_description;
        m_accepts = other.m_accepts;
        m_version = other.m_version;
        m_valid = other.m_valid;
    }

    return *this;
}


void MetaGrabberScript::ParseGrabberVersion(const QDomElement &item)
{
    m_name          = item.firstChildElement("name").text();
    m_author        = item.firstChildElement("author").text();
    m_thumbnail     = item.firstChildElement("thumbnail").text();
    m_command       = item.firstChildElement("command").text();
    m_description   = item.firstChildElement("description").text();
    m_version       = item.firstChildElement("version").text().toFloat();
    m_typestring    = item.firstChildElement("type").text().toLower();

    if (!m_typestring.isEmpty() && grabberTypeStrings.contains(m_typestring))
        m_type = grabberTypeStrings[m_typestring];
    else
        m_type = kGrabberInvalid;

    QDomElement accepts = item.firstChildElement("accepts");
    if (!accepts.isNull())
    {
        while (!accepts.isNull())
        {
            m_accepts.append(accepts.text());
            accepts = accepts.nextSiblingElement("accepts");
        }
    }
}

bool MetaGrabberScript::Test(void)
{
    if (!m_valid || m_fullcommand.isEmpty())
        return false;

    QStringList args; args << "-t";
    MythSystemLegacy grabber(m_fullcommand, args, kMSStdOut);

    grabber.Run();
    return grabber.Wait() == GENERIC_EXIT_OK;
}

// TODO
// using the MetadataLookup object as both argument input, and parsed output,
// is clumsy. break the inputs out into a separate object, and spawn a new
// MetadataLookup object in ParseMetadataItem, rather than requiring an
// existing one to reuse.
MetadataLookupList MetaGrabberScript::RunGrabber(const QStringList &args,
                        MetadataLookup *lookup, bool passseas)
{
    MythSystemLegacy grabber(m_fullcommand, args, kMSStdOut);
    MetadataLookupList list;

    LOG(VB_GENERAL, LOG_INFO, QString("Running Grabber: %1 %2")
        .arg(m_fullcommand).arg(args.join(" ")));

    grabber.Run();
    if (grabber.Wait(180) != GENERIC_EXIT_OK)
        return list;

    QByteArray result = grabber.ReadAll();
    if (!result.isEmpty())
    {
        QDomDocument doc;
        doc.setContent(result, true);
        QDomElement root = doc.documentElement();
        QDomElement item = root.firstChildElement("item");

        while (!item.isNull())
        {
            MetadataLookup *tmp = ParseMetadataItem(item, lookup, passseas);
            tmp->SetInetref(QString("%1_%2").arg(m_command)
                                            .arg(tmp->GetInetref()));
            if (!tmp->GetCollectionref().isEmpty())
            {
                tmp->SetCollectionref(QString("%1_%2").arg(m_command)
                                .arg(tmp->GetCollectionref()));
            }
            list.append(tmp);
            // MetadataLookup is to be owned by the list
            tmp->DecrRef();
            item = item.nextSiblingElement("item");
        }
    }
    return list;
}

QString MetaGrabberScript::GetRelPath(void) const
{
    QString share = GetShareDir();
    if (m_fullcommand.startsWith(share))
        return m_fullcommand.right(m_fullcommand.size() - share.size());

    return QString();
}

void MetaGrabberScript::toMap(InfoMap &metadataMap) const
{
    metadataMap["name"] = m_name;
    metadataMap["author"] = m_author;
    metadataMap["thumbnailfilename"] = m_thumbnail;
    metadataMap["command"] = m_command;
    metadataMap["description"] = m_description;
    metadataMap["version"] = QString::number(m_version);
    metadataMap["type"] = m_typestring;
}

void MetaGrabberScript::SetDefaultArgs(QStringList &args)
{
    args << "-l"
         << gCoreContext->GetLanguage()
         << "-a"
         << gCoreContext->GetLocale()->GetCountryCode();
}

MetadataLookupList MetaGrabberScript::Search(const QString &title,
                        MetadataLookup *lookup, bool passseas)
{
    QStringList args;
    SetDefaultArgs(args);

    args << "-M"
         << title;

    return RunGrabber(args, lookup, passseas);
}

MetadataLookupList MetaGrabberScript::SearchSubtitle(const QString &title,
                        const QString &subtitle, MetadataLookup *lookup,
                        bool passseas)
{
    QStringList args;
    SetDefaultArgs(args);

    args << "-N"
         << title
         << subtitle;

    return RunGrabber(args, lookup, passseas);
}

MetadataLookupList MetaGrabberScript::SearchSubtitle(const QString &inetref,
                        const QString &title, const QString &subtitle,
                        MetadataLookup *lookup, bool passseas)
{
    (void)title;
    QStringList args;
    SetDefaultArgs(args);

    args << "-N"
         << CleanedInetref(inetref)
         << subtitle;

    return RunGrabber(args, lookup, passseas);
}

MetadataLookupList MetaGrabberScript::LookupData(const QString &inetref,
                        MetadataLookup *lookup, bool passseas)
{
    QStringList args;
    SetDefaultArgs(args);

    args << "-D"
         << CleanedInetref(inetref);

    return RunGrabber(args, lookup, passseas);
}

MetadataLookupList MetaGrabberScript::LookupData(const QString &inetref,
                        int season, int episode, MetadataLookup *lookup,
                        bool passseas)
{
    QStringList args;
    SetDefaultArgs(args);

    args << "-D"
         << CleanedInetref(inetref)
         << QString::number(season)
         << QString::number(episode);

    return RunGrabber(args, lookup, passseas);
}

MetadataLookupList MetaGrabberScript::LookupCollection(
                        const QString &collectionref, MetadataLookup *lookup,
                        bool passseas)
{
    QStringList args;
    SetDefaultArgs(args);

    args << "-C"
         << CleanedInetref(collectionref);

    return RunGrabber(args, lookup, passseas);
}
