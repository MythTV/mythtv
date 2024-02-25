
#include "videoutils.h"

#include <QDir>
#include <QCoreApplication>

// mythtv
#include "libmyth/mythcontext.h"
#include "libmythbase/mythdirs.h"
#include "libmythbase/mythsystemlegacy.h"
#include "libmythbase/storagegroup.h"
#include "libmythui/mythdialogbox.h"
#include "libmythui/mythmainwindow.h"
#include "libmythui/mythuiimage.h"
#include "libmythui/mythuistatetype.h"
#include "libmythui/mythuitext.h"

// libmythmetadata
#include "globals.h"
#include "videometadatalistmanager.h"

namespace
{
    const QString VIDEO_COVERFILE_DEFAULT_OLD = 
        QCoreApplication::translate("(VideoUtils)", "None", "No cover");
    const QString VIDEO_COVERFILE_DEFAULT_OLD2 = 
        QCoreApplication::translate("(VideoUtils)", "No Cover");

    template <typename T>
    void CopySecond(const T &src, QStringList &dest)
    {
        for (auto p = src.cbegin(); p != src.cend(); ++p)
        {
            dest.push_back((*p).second);
        }
    }
}

template <>
void CheckedSet(MythUIStateType *uiItem, const QString &value)
{
    if (uiItem)
    {
        uiItem->Reset();
        uiItem->DisplayState(value);
    }
}

void CheckedSet(MythUIType *container, const QString &itemName,
        const QString &value)
{
    if (container)
    {
        MythUIType *uit = container->GetChild(itemName);
        auto *tt = dynamic_cast<MythUIText *>(uit);
        if (tt)
            CheckedSet(tt, value);
        else
        {
            auto *st = dynamic_cast<MythUIStateType *>(uit);
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

QStringList GetVideoDirsByHost(const QString& host)
{
    QStringList tmp;

    QStringList tmp2 = StorageGroup::getGroupDirs("Videos", host);
    for (const auto& dir : std::as_const(tmp2))
        tmp.append(dir);

    if (host.isEmpty())
    {
#ifdef _WIN32
        QString seperator = ";";
#else
        QString seperator = ":";
#endif
        QStringList tmp3 = gCoreContext->GetSetting("VideoStartupDir",
                    DEFAULT_VIDEOSTARTUP_DIR).split(seperator, Qt::SkipEmptyParts);
        for (const auto& dir : std::as_const(tmp3))
        {
            bool matches = false;
            QString newpath = dir;
            if (!newpath.endsWith("/"))
                newpath.append("/");

            for (const auto& comp : std::as_const(tmp2))
            {
                if (comp.endsWith(newpath))
                {
                    matches = true;
                    break;
                }
            }
            if (!matches)
                tmp.append(QDir::cleanPath(dir));
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

QString GetDisplayLength(std::chrono::minutes length)
{
    // The disambiguation string must be an empty string and not a 
    // nullptr to get extracted by the Qt tools.
    return QCoreApplication::translate("(Common)", "%n minute(s)", "", 
               length.count());
}

QString GetDisplayBrowse(bool browse)
{
    QString ret;

    if (browse)
        ret = QCoreApplication::translate("(Common)", "Yes");
    else
        ret = QCoreApplication::translate("(Common)", "No");

    return ret;
}

QString GetDisplayWatched(bool watched)
{
    QString ret;

    if (watched)
        ret = QCoreApplication::translate("(Common)", "Yes");
    else
        ret = QCoreApplication::translate("(Common)", "No");

    return ret;
}

QString GetDisplayProcessed(bool processed)
{
    QString ret;

    if (processed)
    {
        ret = QCoreApplication::translate("(VideoUtils)", 
                                           "Details Downloaded");
    }
    else
    {
        ret = QCoreApplication::translate("(VideoUtils)", 
                                           "Waiting for Detail Download");
    }

    return ret;
}

QString GetDisplayYear(int year)
{
    return year == VIDEO_YEAR_DEFAULT ? "?" : QString::number(year);
}

QString GetDisplayRating(const QString &rating)
{
    if (rating == "<NULL>")
        return QCoreApplication::translate("(VideoUtils)", "No rating available.");
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

VideoContentType ContentTypeFromString(const QString &type)
{
    VideoContentType ret = kContentUnknown;

    if (type == "MOVIE")
        ret = kContentMovie;
    else if (type == "TELEVISION")
        ret = kContentTelevision;
    else if (type == "ADULT")
        ret = kContentAdult;
    else if (type == "MUSICVIDEO")
        ret = kContentMusicVideo;
    else if (type == "HOMEVIDEO")
        ret = kContentHomeMovie;

    return ret;
}

QString ContentTypeToString(VideoContentType type)
{
    QString ret = "UNKNOWN";

    if (type == kContentMovie)
        ret = "MOVIE";
    else if (type == kContentTelevision)
        ret = "TELEVISION";
    else if (type == kContentAdult)
        ret = "ADULT";
    else if (type == kContentMusicVideo)
        ret = "MUSICVIDEO";
    else if (type == kContentHomeMovie)
        ret = "HOMEVIDEO";

    return ret;
}

