#ifndef NETBASE_H_
#define NETBASE_H_

#include <QDomDocument>
#include <QString>
#include <QUrl>

#include <libmythbase/netutils.h>
#include <libmythui/mythscreentype.h>

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
    explicit NetBase(MythScreenStack *parent, const char *name = nullptr);
    ~NetBase() override;

  protected:
    void Init() override; // MythScreenType
    virtual ResultItem *GetStreamItem() = 0;
    virtual void LoadData(void) = 0;
    void InitProgressDialog();
    static void CleanCacheDir();
    void DownloadVideo(const QString &url, const QString &dest);
    static void RunCmdWithoutScreensaver(const QString &cmd);

  protected slots:
    void StreamWebVideo(void);
    void ShowWebVideo(void);
    void DoDownloadAndPlay(void);
    void DoPlayVideo(const QString &filename);
    void DoPlayVideo();
    void SlotDeleteVideo(void);
    void DoDeleteVideo(bool remove);
    void customEvent(QEvent *event) override; // MythUIType

  protected:
    MythUIImage           *m_thumbImage     {nullptr};
    MythUIStateType       *m_downloadable   {nullptr};
    MythScreenStack       *m_popupStack     {nullptr};
    MythUIProgressDialog  *m_progressDialog {nullptr};
    MetadataImageDownload *m_imageDownload  {nullptr};

    QString m_downloadFile;
    GrabberScript::scriptList m_grabberList;
};

#endif // NETBASE_H_
