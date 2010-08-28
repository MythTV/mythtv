#ifndef VIDEOUTILS_H_
#define VIDEOUTILS_H_

#include "parentalcontrols.h"
#include "mythexp.h"

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
MPUBLIC void CheckedSet(class MythUIStateType *uiItem, const QString &state);

MPUBLIC void CheckedSet(class MythUIType *container, const QString &itemName,
        const QString &value);

MPUBLIC void CheckedSet(class MythUIImage *uiItem, const QString &filename);

MPUBLIC QStringList GetVideoDirsByHost(QString host);
MPUBLIC QStringList GetVideoDirs();

MPUBLIC bool IsDefaultCoverFile(const QString &coverfile);
MPUBLIC bool IsDefaultScreenshot(const QString &screenshot);
MPUBLIC bool IsDefaultBanner(const QString &banner);
MPUBLIC bool IsDefaultFanart(const QString &fanart);

class Metadata;

MPUBLIC QString GetDisplayUserRating(float userrating);
MPUBLIC QString GetDisplayLength(int length);
MPUBLIC QString GetDisplaySeasonEpisode(int seasEp, int digits);
MPUBLIC QString GetDisplayBrowse(bool browse);
MPUBLIC QString GetDisplayWatched(bool watched);
MPUBLIC QString GetDisplayProcessed(bool processed);
MPUBLIC QString GetDisplayYear(int year);
MPUBLIC QString GetDisplayRating(const QString &rating);

MPUBLIC QString GetDisplayGenres(const Metadata &item);
MPUBLIC QString GetDisplayCountries(const Metadata &item);
MPUBLIC QStringList GetDisplayCast(const Metadata &item);

MPUBLIC QString TrailerToState(const QString &trailerFile);
MPUBLIC QString ParentalLevelToState(const ParentalLevel &level);
MPUBLIC QString WatchedToState(bool watched);

// this needs to be an inline and pull in the storage group and context
// headers since it this used in dbcheck.cpp.
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
