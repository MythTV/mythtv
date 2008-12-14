#ifndef VIDEOUTILS_H_
#define VIDEOUTILS_H_

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

bool IsDefaultCoverFile(const QString &coverfile);

class Metadata;

QStringList GetCastList(const Metadata &item);
QString GetCast(const Metadata &item, const QString &sep = ", ");

#endif // VIDEOUTILS_H_
