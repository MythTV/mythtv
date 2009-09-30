#include <QDir>

#include <mythtv/mythcontext.h>
#include <mythtv/mythdirs.h>

#include <mythtv/libmythui/mythmainwindow.h>
#include <mythtv/libmythui/mythsystem.h>
#include <mythtv/libmythui/mythdialogbox.h>
#include <mythtv/libmythui/mythuistatetype.h>
#include <mythtv/libmythui/mythuiimage.h>

#include "globals.h"
#include "metadatalistmanager.h"
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

static QMutex cacheLock;
static QHash <QString, QString>sgroupMap;

void ClearRemoteSGMap(void)
{
    QMutexLocker locker(&cacheLock);
    sgroupMap.clear();
}

QString GetHostSGToUse(QString host, QString sgroup)
{
    QString tmpGroup = sgroup;
    QString groupKey = QString("%1:%2").arg(sgroup, host);

    QMutexLocker locker(&cacheLock);

    if (sgroupMap.contains(groupKey))
    {
        tmpGroup = sgroupMap[groupKey];
    }
    else
    {
        if (StorageGroup::FindDirs(sgroup, host))
        {
            sgroupMap[groupKey] = sgroup;
        }
        else
        {
            VERBOSE(VB_FILE+VB_EXTRA, QString("GetHostSGToUse(): "
                    "falling back to Videos Storage Group for host %1 "
                    "since it does not have a %2 Storage Group.")
                    .arg(host).arg(sgroup));

            tmpGroup = "Videos";
            sgroupMap[groupKey] = tmpGroup;
        }
    }

    return tmpGroup;
}

bool GetRemoteFileList(QString host, QString path, QStringList* list,
                       QString sgroup, bool fileNamesOnly)
{

    // Make sure the list is empty when we get started
    list->clear();

    if (sgroup.isEmpty())
        sgroup = "Videos";

    *list << "QUERY_SG_GETFILELIST";
    *list << host;
    *list << GetHostSGToUse(host, sgroup);
    *list << path;
    *list << QString::number(fileNamesOnly);

    bool ok = gContext->SendReceiveStringList(*list);

// Should the SLAVE UNREACH test be here ?
    return ok;
}

QString GenRemoteFileURL(QString sgroup, QString host, QString path)
{
    return QString("myth://%1@").arg(GetHostSGToUse(host, sgroup)) +
              gContext->GetSettingOnHost("BackendServerIP", host) + ":" +
              gContext->GetSettingOnHost("BackendServerPort", host) + "/" +
              path;

}

QStringList GetVideoDirsByHost(QString host)
{
    QStringList tmp;

    QStringList tmp2 = StorageGroup::getGroupDirs("Videos", host); 
    for (QStringList::iterator p = tmp2.begin(); p != tmp2.end(); ++p) 
        tmp.append(*p); 

    if (host.isEmpty())
    {
        QStringList tmp3 = gContext->GetSetting("VideoStartupDir",
                    DEFAULT_VIDEOSTARTUP_DIR).split(":", QString::SkipEmptyParts);
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

QString GetDisplaySeasonEpisode(int seasEp, int digits)
{
    QString seasEpNum = QString::number(seasEp);

    if (digits == 2 && seasEpNum.size() < 2)
        seasEpNum.prepend("0");
        
    return seasEpNum;
}

QString GetDisplayBrowse(bool browse)
{
    return browse ? QObject::tr("Yes") : QObject::tr("No");
}

QString GetDisplayWatched(bool watched)
{
    return watched ? QObject::tr("Yes") : QObject::tr("No");
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

QString GetDisplayGenres(const Metadata &item)
{
    QStringList ret;
    CopySecond(item.GetGenres(), ret);
    return ret.join(", ");
}

QString GetDisplayCountries(const Metadata &item)
{
    QStringList ret;
    CopySecond(item.GetCountries(), ret);
    return ret.join(", ");
}

QStringList GetDisplayCast(const Metadata &item)
{
    QStringList ret;
    CopySecond(item.GetCast(), ret);
    return ret;
}
