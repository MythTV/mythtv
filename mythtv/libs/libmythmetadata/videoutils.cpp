#include <QDir>

#include "mythcontext.h"
#include "mythdirs.h"
#include "mythmainwindow.h"
#include "mythsystem.h"
#include "mythdialogbox.h"
#include "mythuistatetype.h"
#include "mythuiimage.h"
#include "globals.h"
#include "videometadatalistmanager.h"
#include "videoutils.h"
#include "storagegroup.h"

namespace
{
    const QString VIDEO_COVERFILE_DEFAULT_OLD = QObject::tr("None");
    const QString VIDEO_COVERFILE_DEFAULT_OLD2 = QObject::tr("No Cover");

    template <typename T>
    void CopySecond(const T &src, QStringList &dest)
    {
        for (typename T::const_iterator p = src.begin(); p != src.end(); ++p)
        {
            dest.push_back((*p).second);
        }
    }
}

template <>
void CheckedSet(MythUIStateType *uiItem, const QString &state)
{
    if (uiItem)
    {
        uiItem->Reset();
        uiItem->DisplayState(state);
    }
}

void CheckedSet(MythUIType *container, const QString &itemName,
        const QString &value)
{
    if (container)
    {
        MythUIType *uit = container->GetChild(itemName);
        MythUIText *tt = dynamic_cast<MythUIText *>(uit);
        if (tt)
            CheckedSet(tt, value);
        else
        {
            MythUIStateType *st = dynamic_cast<MythUIStateType *>(uit);
            CheckedSet(st, value);
        }
    }
}

void CheckedSet(MythUIImage *uiItem, const QString &filename)
{
    if (uiItem)
    {
        uiItem->Reset();
        uiItem->SetFilename(filename);
        uiItem->Load();
    }
}

QStringList GetVideoDirsByHost(QString host)
{
    QStringList tmp;

    QStringList tmp2 = StorageGroup::getGroupDirs("Videos", host);
    for (QStringList::iterator p = tmp2.begin(); p != tmp2.end(); ++p)
        tmp.append(*p);

    if (host.isEmpty())
    {
#ifdef _WIN32
        QStringList tmp3 = gCoreContext->GetSetting("VideoStartupDir",
                    DEFAULT_VIDEOSTARTUP_DIR).split(";", QString::SkipEmptyParts);
#else
        QStringList tmp3 = gCoreContext->GetSetting("VideoStartupDir",
                    DEFAULT_VIDEOSTARTUP_DIR).split(":", QString::SkipEmptyParts);
#endif
        for (QStringList::iterator p = tmp3.begin(); p != tmp3.end(); ++p)
        {
            bool matches = false;
            QString newpath = *p;
            if (!newpath.endsWith("/"))
                newpath.append("/");

            for (QStringList::iterator q = tmp2.begin(); q != tmp2.end(); ++q)
            {
                QString comp = *q;

                if (comp.endsWith(newpath))
                {
                    matches = true;
                    break;
                }
            }
            if (!matches)
                tmp.append(QDir::cleanPath(*p));
        }
    }

    return tmp;
}

QStringList GetVideoDirs()
{
    return GetVideoDirsByHost("");
}

bool IsDefaultCoverFile(const QString &coverfile)
{
    return coverfile == VIDEO_COVERFILE_DEFAULT ||
            coverfile == VIDEO_COVERFILE_DEFAULT_OLD ||
            coverfile == VIDEO_COVERFILE_DEFAULT_OLD2 ||
            coverfile.endsWith(VIDEO_COVERFILE_DEFAULT_OLD) ||
            coverfile.endsWith(VIDEO_COVERFILE_DEFAULT_OLD2);
}

bool IsDefaultScreenshot(const QString &screenshot)
{
    return screenshot == VIDEO_SCREENSHOT_DEFAULT;
}

bool IsDefaultBanner(const QString &banner)
{
    return banner == VIDEO_BANNER_DEFAULT;
}

bool IsDefaultFanart(const QString &fanart)
{
    return fanart == VIDEO_FANART_DEFAULT;
}

QString GetDisplayUserRating(float userrating)
{
    return QString::number(userrating, 'f', 1);
}

QString GetDisplayLength(int length)
{
    return QString("%1 minutes").arg(length);
}

QString GetDisplayBrowse(bool browse)
{
    return browse ? QObject::tr("Yes") : QObject::tr("No");
}

QString GetDisplayWatched(bool watched)
{
    return watched ? QObject::tr("Yes") : QObject::tr("No");
}

QString GetDisplayProcessed(bool processed)
{
    return processed ? QObject::tr("Details Downloaded") : QObject::tr("Waiting for Detail Download");
}

QString GetDisplayYear(int year)
{
    return year == VIDEO_YEAR_DEFAULT ? "?" : QString::number(year);
}

QString GetDisplayRating(const QString &rating)
{
    if (rating == "<NULL>")
        return QObject::tr("No rating available.");
    return rating;
}

QString GetDisplayGenres(const VideoMetadata &item)
{
    QStringList ret;
    CopySecond(item.GetGenres(), ret);
    return ret.join(", ");
}

QString GetDisplayCountries(const VideoMetadata &item)
{
    QStringList ret;
    CopySecond(item.GetCountries(), ret);
    return ret.join(", ");
}

QStringList GetDisplayCast(const VideoMetadata &item)
{
    QStringList ret;
    CopySecond(item.GetCast(), ret);
    return ret;
}

QString ParentalLevelToState(const ParentalLevel &level)
{
    QString ret;
    switch (level.GetLevel())
    {
         case ParentalLevel::plLowest :
            ret = "Lowest";
            break;
        case ParentalLevel::plLow :
            ret = "Low";
            break;
        case ParentalLevel::plMedium :
            ret = "Medium";
            break;
        case ParentalLevel::plHigh :
            ret = "High";
            break;
        default:
            ret = "None";
    }

    return ret;
}

QString TrailerToState(const QString &trailerFile)
{
    QString ret;
    if (!trailerFile.isEmpty())
        ret = "hasTrailer";
    else
        ret = "None";
    return ret;
}

QString WatchedToState(bool watched)
{
    QString ret;
    if (watched)
        ret = "yes";
    else
        ret = "no";
    return ret;
}

bool isHostMaster(const QString &host)
{
    bool isMaster = false;
    QString masterIP = gCoreContext->GetSetting("MasterServerIP");
    QString hostIP;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT data FROM settings WHERE "
                  "value = 'BackendServerIP' AND "
                  "hostname = :HOSTNAME;");
    query.bindValue(":HOSTNAME", host);

    if (query.exec() && query.next())
    {
        hostIP = query.value(0).toString();
        if (hostIP == masterIP)
            isMaster = true;
    }

    return isMaster;
}
