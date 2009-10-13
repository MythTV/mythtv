#ifndef VIDEOUTILS_H_
#define VIDEOUTILS_H_

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
QString GetDisplayYear(int year);
QString GetDisplayRating(const QString &rating);

QString GetDisplayGenres(const Metadata &item);
QString GetDisplayCountries(const Metadata &item);
QStringList GetDisplayCast(const Metadata &item);

#endif // VIDEOUTILS_H_
