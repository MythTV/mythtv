#ifndef VIDEOUTILS_H_
#define VIDEOUTILS_H_

#include "parentalcontrols.h"

template <typename T>
inline void CheckedSet(T *uiItem, const QString &value)
{
    if (uiItem)
    {
        if (!value.isEmpty())
            uiItem->SetText(value);
        else
            uiItem->Reset();
    }
}

template <>
void CheckedSet(class MythUIStateType *uiItem, const QString &state);

void CheckedSet(class MythUIType *container, const QString &itemName,
        const QString &value);

void CheckedSet(class MythUIImage *uiItem, const QString &filename);

QStringList GetVideoDirsByHost(QString host);
QStringList GetVideoDirs();

bool IsDefaultCoverFile(const QString &coverfile);
bool IsDefaultScreenshot(const QString &screenshot);
bool IsDefaultBanner(const QString &banner);
bool IsDefaultFanart(const QString &fanart);

class Metadata;

QString GetDisplayUserRating(float userrating);
QString GetDisplayLength(int length);
QString GetDisplaySeasonEpisode(int seasEp, int digits);
QString GetDisplayBrowse(bool browse);
QString GetDisplayWatched(bool watched);
QString GetDisplayProcessed(bool processed);
QString GetDisplayYear(int year);
QString GetDisplayRating(const QString &rating);

QString GetDisplayGenres(const Metadata &item);
QString GetDisplayCountries(const Metadata &item);
QStringList GetDisplayCast(const Metadata &item);

QString TrailerToState(const QString &trailerFile);
QString ParentalLevelToState(const ParentalLevel &level);
QString WatchedToState(bool watched);

// this needs to be an inline and pull in the storage group and context
// headers since it this used in dbcheck.cpp, and it is pulled into mtd.
#include <storagegroup.h>
#include <mythcorecontext.h>
inline QString generate_file_url(
    const QString &storage_group, const QString &host, const QString &path)
{
    QString ip = gCoreContext->GetSettingOnHost("BackendServerIP", host);
    uint port = gCoreContext->GetSettingOnHost("BackendServerPort",
                                               host).toUInt();

    return QString("myth://%1@%2:%3/%4")
        .arg(StorageGroup::GetGroupToUse(host, storage_group))
        .arg(ip).arg(port).arg(path);
}

#endif // VIDEOUTILS_H_
