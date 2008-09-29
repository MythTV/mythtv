#ifndef VIDEOUTILS_H_
#define VIDEOUTILS_H_

// QT headers
#include <Q3UrlOperator>
#include <QFileInfo>
#include <Q3Process>
#include <QTimer>

class Metadata;
class MetadataListManager;

typedef QMap<QString, QString> SearchListResults;

void PlayVideo(const QString &filename, const MetadataListManager &video_list);

template <typename T>
void checkedSetText(T *item, const QString &text)
{
    if (item) item->SetText(text);
}

void checkedSetText(class MythUIType *container, const QString &item_name,
                    const QString &text);

struct UIAssignException
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

template <typename ContainerType, typename UIType,
    typename ErrorTraits>
void UIAssign(ContainerType *container, UIType *&item, const QString &name)
{
    item = dynamic_cast<UIType *>(container->GetChild(name));

    if (item)
        return;

    if (!container)
    {
        ErrorTraits::Container(name);
        return;
    }

    ErrorTraits::Child(container->objectName(), name);
}

template <typename ContainerType, typename UIType>
void UIAssign(ContainerType *container, UIType *&item, const QString &name)
{
    UIAssign<ContainerType, UIType, ETPrintWarning>(container, item, name);
}

template <typename ContainerType, typename UIType>
void EUIAssign(ContainerType *container, UIType *&item, const QString &name)
{
    UIAssign<ContainerType, UIType, ETErrorException>(container, item, name);
}

template <typename ContainerType, typename UIType>
void NOPUIAssign(ContainerType *container, UIType *&item, const QString &name)
{
    UIAssign<ContainerType, UIType, ETNop>(container, item, name);
}

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

/** \class TimeoutSignalProxy
 *
 * \brief Holds the timer used for the poster download timeout notification.
 *
 */
class TimeoutSignalProxy : public QObject
{
    Q_OBJECT

  public:
    TimeoutSignalProxy();

    void start(int timeout, Metadata *item, const QString &url);
    void stop(void);

  signals:
    void SigTimeout(const QString &url, Metadata *item);

  private slots:
    void OnTimeout();

  private:
    Metadata *m_item;
    QString m_url;
    QTimer m_timer;
};

/** \class URLOperationProxy
 *
 * \brief Wrapper for Q3UrlOperator used to download the cover image.
 *
 */
class URLOperationProxy : public QObject
{
    Q_OBJECT

  public:
    URLOperationProxy();

    void copy(const QString &uri, const QString &dest, Metadata *item);
    void stop(void);

    signals:
    void SigFinished(Q3NetworkOperation *op, Metadata *item);

  private slots:
    void OnFinished(Q3NetworkOperation *op);

  private:
    Metadata *m_item;
    Q3UrlOperator m_url_op;
};

/** \class ExecuteExternalCommand
 *
 * \brief Base class for executing an external script or other process, must
 *        be subclassed.
 *
 */
class ExecuteExternalCommand : public QObject
{
    Q_OBJECT

  protected:
    ExecuteExternalCommand(QObject *oparent);
    ~ExecuteExternalCommand();

    void StartRun(const QString &command, const QStringList &args,
                  const QString &purpose);

    virtual void OnExecDone(bool normal_exit, const QStringList &out,
                            const QStringList &err);

  private slots:
    void OnReadReadyStdout(void);
    void OnReadReadyStderr(void);
    void OnProcessExit(void);

  private:
    void ShowError(const QString &error_msg);

    QString m_std_error;
    QString m_std_out;
    Q3Process m_process;
    QString m_purpose;
    QString m_raw_cmd;
};

/** \class VideoTitleSearch
 *
 * \brief Executes the external command to do video title searches.
 *
 */
class VideoTitleSearch : public ExecuteExternalCommand
{
    Q_OBJECT

  public:
    VideoTitleSearch(QObject *oparent);

    void Run(const QString &title, Metadata *item);

  signals:
    void SigSearchResults(bool normal_exit,
                          const SearchListResults &items,
                          Metadata *item);

  protected:
    virtual void OnExecDone(bool normal_exit, const QStringList &out,
                            const QStringList &err);

  private:
    ~VideoTitleSearch() {}

    Metadata *m_item;
};

/** \class VideoUIDSearch
 *
 * \brief Execute the command to do video searches based on their ID.
 *
 */
class VideoUIDSearch : public ExecuteExternalCommand
{
    Q_OBJECT

  public:
    VideoUIDSearch(QObject *oparent);

    void Run(const QString &video_uid, Metadata *item);

  signals:
    void SigSearchResults(bool normal_exit, const QStringList &results,
                            Metadata *item, const QString &video_uid);

  private:
    ~VideoUIDSearch() {}

    void OnExecDone(bool normal_exit, const QStringList &out,
                    const QStringList &err);

    QString m_video_uid;
    Metadata *m_item;
};

/** \class VideoPosterSearch
 *
 * \brief Execute external video poster command.
 *
 */
class VideoPosterSearch : public ExecuteExternalCommand
{
    Q_OBJECT

  public:
    VideoPosterSearch(QObject *oparent);

    void Run(const QString &video_uid, Metadata *item);

    signals:
    void SigPosterURL(const QString &url, Metadata *item);

  private:
    ~VideoPosterSearch() {}

    void OnExecDone(bool normal_exit, const QStringList &out,
                    const QStringList &err);

    Metadata *m_item;
};

#endif // VIDEOUTILS_H_
