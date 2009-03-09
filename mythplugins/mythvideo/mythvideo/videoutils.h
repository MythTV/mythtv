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

bool GetRemoteFileList(QString host, QString path, QStringList* list, QString sgroup);
QString GenRemoteFileURL(QString sgroup, QString host, QString path);
QStringList GetVideoDirsByHost(QString host);
QStringList GetVideoDirs();

bool IsDefaultCoverFile(const QString &coverfile);
bool IsDefaultScreenshot(const QString &screenshot);
bool IsDefaultBanner(const QString &banner);
bool IsDefaultFanart(const QString &fanart);

class Metadata;

QString GetDisplayUserRating(float userrating);
QString GetDisplayLength(int length);
QString GetDisplayBrowse(bool browse);
QString GetDisplayYear(int year);
QString GetDisplayRating(const QString &rating);

QString GetDisplayGenres(const Metadata &item);
QString GetDisplayCountries(const Metadata &item);
QStringList GetDisplayCast(const Metadata &item);

#endif // VIDEOUTILS_H_
