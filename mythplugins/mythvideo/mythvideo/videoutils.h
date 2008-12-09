#ifndef VIDEOUTILS_H_
#define VIDEOUTILS_H_

class Metadata;
class MetadataListManager;

typedef QMap<QString, QString> SearchListResults;

void PlayVideo(const QString &filename, const MetadataListManager &video_list);

template <typename T>
inline void CheckedSet(T *ui_item, const QString &text)
{
    if (ui_item)
    {
        if (!text.isEmpty())
            ui_item->SetText(text);
        else
            ui_item->Reset();
    }

}

template <>
void CheckedSet(class MythUIStateType *ui_item, const QString &state);

void CheckedSet(class MythUIType *container, const QString &itemName, const QString &text);

QStringList GetVideoDirs();

QString getDisplayYear(int year);
QString getDisplayRating(const QString &rating);
QString getDisplayUserRating(float userrating);
QString getDisplayLength(int length);
QString getDisplayBrowse(bool browse);

bool isDefaultCoverFile(const QString &coverfile);
bool GetLocalVideoPoster(const QString &video_uid, const QString &filename,
                         const QStringList &in_dirs, QString &poster);

QStringList GetCastList(const Metadata &item);
QString GetCast(const Metadata &item, const QString &sep = ", ");

#endif // VIDEOUTILS_H_
