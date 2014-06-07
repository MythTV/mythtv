#ifndef NETBASE_H_
#define NETBASE_H_

#include <QString>
#include <QDomDocument>
#include <QUrl>

#include <netutils.h>
#include <mythscreentype.h>

class MythUIImage;
class MythUIStateType;
class MythUIBusyDialog;
class MythUIProgressDialog;
class MetadataImageDownload;
class ResultItem;

class NetBase : public MythScreenType
{
  Q_OBJECT

  public:
    NetBase(MythScreenStack *parent, const char *name = 0);
    virtual ~NetBase();

  protected:
    virtual void Init();
    virtual ResultItem *GetStreamItem() = 0;
    virtual void loadData(void) = 0;
    void InitProgressDialog();
    void CreateBusyDialog(const QString &title);
    void CleanCacheDir();
    void DownloadVideo(const QString &url, const QString &dest);
    void RunCmdWithoutScreensaver(const QString &cmd);

  protected slots:
    void streamWebVideo(void);
    void showWebVideo(void);
    void doDownloadAndPlay(void);
    void doPlayVideo(const QString &filename);
    void slotDeleteVideo(void);
    void doDeleteVideo(bool remove);
    virtual void customEvent(QEvent *event);

  protected:
    MythUIImage           *m_thumbImage;
    MythUIStateType       *m_downloadable;
    MythUIBusyDialog      *m_busyPopup;
    MythScreenStack       *m_popupStack;
    MythUIProgressDialog  *m_progressDialog;
    MetadataImageDownload *m_imageDownload;

    QString m_downloadFile;
    GrabberScript::scriptList m_grabberList;
};

#endif // NETBASE_H_
