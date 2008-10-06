#ifndef VIDEOUTILS_H_
#define VIDEOUTILS_H_

class Metadata;
class MetadataListManager;

typedef QMap<QString, QString> SearchListResults;

void PlayVideo(const QString &filename, const MetadataListManager &video_list);

template <typename T>
inline void CheckedSet(T *ui_item, QString text)
{
    if (ui_item)
        ui_item->SetText(text);
}

template <>
void CheckedSet(class MythUIStateType *ui_item, QString state);

void CheckedSet(class MythUIType *container, QString itemName, QString text);

struct UIUtilException
{
    virtual QString What() = 0;
};

struct ETNop
{
    static void Child(QString container_name, QString child_name);
    static void Container(QString child_name);
};

struct ETPrintWarning
{
    static void Child(QString container_name, QString child_name);
    static void Container(QString child_name);
};

struct ETErrorException
{
    static void Child(QString container_name, QString child_name);
    static void Container(QString child_name);
};

template <typename ErrorDispatch = ETPrintWarning>
struct ui_util
{
    template <typename ContainerType, typename UIType>
    static void Assign(ContainerType *container, UIType *&item,
            const QString &name)
    {
        item = dynamic_cast<UIType *>(container->GetChild(name));

        if (item)
            return;

        if (!container)
        {
            ErrorDispatch::Container(name);
            return;
        }

        ErrorDispatch::Child(container->objectName(), name);
    }
};

typedef struct ui_util<ETPrintWarning> UIUtil;
typedef struct ui_util<ETNop> UIUtilN;
typedef struct ui_util<ETErrorException> UIUtilE;

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
