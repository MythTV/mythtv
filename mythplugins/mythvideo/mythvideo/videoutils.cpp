#include <QDir>

#include <mythtv/mythcontext.h>
#include <mythtv/mythdirs.h>

#include <mythtv/libmythui/mythmainwindow.h>
#include <mythtv/libmythui/mythsystem.h>
#include <mythtv/libmythui/mythdialogbox.h>
#include <mythtv/libmythui/mythuistatetype.h>

#include "globals.h"
#include "metadatalistmanager.h"
#include "videoutils.h"
#include "storagegroup.h"

namespace
{
    const QString VIDEO_COVERFILE_DEFAULT_OLD = QObject::tr("None");

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

bool GetRemoteFileList(QString host, QString path, QStringList* list, QString sgroup)
{

    // Make sure the list is empty when we get started
    list->clear();

    if (sgroup.isEmpty())
        sgroup = "Videos";

    *list << "QUERY_SG_GETFILELIST";
    *list << host;
    *list << sgroup;
    *list << path;

    bool ok = gContext->SendReceiveStringList(*list);

// Should the SLAVE UNREACH test be here ?
    return ok;
}

QString GenRemoteFileURL(QString sgroup, QString host, QString path)
{
    return QString("myth://%1@").arg(sgroup) +
              gContext->GetSettingOnHost("BackendServerIP", host) + ":" +
              gContext->GetSettingOnHost("BackendServerPort", host) + "/" +
              path;

}

QStringList GetVideoDirsByHost(QString host)
{
    QStringList tmp;

    if (host.isEmpty()) {
        tmp = gContext->GetSetting("VideoStartupDir",
                    DEFAULT_VIDEOSTARTUP_DIR).split(":", QString::SkipEmptyParts);
        for (QStringList::iterator p = tmp.begin(); p != tmp.end(); ++p)
        {
            *p = QDir::cleanPath(*p);
        }
    }

    QStringList tmp2 = StorageGroup::getGroupDirs("Videos", host);
    for (QStringList::iterator p = tmp2.begin(); p != tmp2.end(); ++p)
        tmp.append(*p);

    return tmp;
}

QStringList GetVideoDirs()
{
    return GetVideoDirsByHost("");
}

bool IsDefaultCoverFile(const QString &coverfile)
{
    return coverfile == VIDEO_COVERFILE_DEFAULT ||
            coverfile == VIDEO_COVERFILE_DEFAULT_OLD;
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
