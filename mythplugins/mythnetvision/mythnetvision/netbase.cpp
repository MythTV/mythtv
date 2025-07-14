#include <QDir>

#include <libmythbase/mythcorecontext.h>
#include <libmythbase/mythdate.h>
#include <libmythbase/mythdirs.h>
#include <libmythbase/mythlogging.h>
#include <libmythbase/remotefile.h>
#include <libmythbase/remoteutil.h>
#include <libmythbase/storagegroup.h>
#include <libmythmetadata/metadataimagedownload.h>
#include <libmythui/mythdialogbox.h>
#include <libmythui/mythmainwindow.h>
#include <libmythui/mythprogressdialog.h>
#include <libmythui/mythscreenstack.h>
#include <libmythui/mythuihelper.h>

#include "netbase.h"

NetBase::NetBase(MythScreenStack *parent, const char *name)
    : MythScreenType(parent, name),
      m_popupStack(GetMythMainWindow()->GetStack("popup stack")),
      m_imageDownload(new MetadataImageDownload(this))
{
    gCoreContext->addListener(this);
}

NetBase::~NetBase()
{
    CleanCacheDir();

    qDeleteAll(m_grabberList);
    m_grabberList.clear();

    cleanThumbnailCacheDir();

    delete m_imageDownload;
    m_imageDownload = nullptr;

    gCoreContext->removeListener(this);
}

void NetBase::Init()
{
    LoadData();
}

void NetBase::DownloadVideo(const QString &url, const QString &dest)
{
    InitProgressDialog();
    m_downloadFile = RemoteDownloadFile(url, "Default", dest);
}

void NetBase::InitProgressDialog()
{
    QString message = tr("Downloading Video...");
    m_progressDialog = new MythUIProgressDialog(message,
        m_popupStack, "videodownloadprogressdialog");

    if (m_progressDialog->Create())
        m_popupStack->AddScreen(m_progressDialog, false);
    else
    {
        delete m_progressDialog;
        m_progressDialog = nullptr;
    }
}

void NetBase::CleanCacheDir()
{
    QString cache = QString("%1/cache/netvision-thumbcache")
                       .arg(GetConfDir());
    QDir cacheDir(cache);
    QStringList thumbs = cacheDir.entryList(QDir::Files);

    for (auto i = thumbs.crbegin(); i != thumbs.crend(); ++i)
    {
        QString filename = QString("%1/%2").arg(cache, *i);
        LOG(VB_GENERAL, LOG_DEBUG, QString("Deleting file %1").arg(filename));
        QFileInfo fi(filename);
        QDateTime lastmod = fi.lastModified();
        if (lastmod.addDays(7) < MythDate::current())
            QFile::remove(filename);
    }
}

void NetBase::StreamWebVideo()
{
    ResultItem *item = GetStreamItem();

    if (!item)
        return;

    if (!item->GetDownloadable())
    {
        ShowWebVideo();
        return;
    }

    auto seconds = std::chrono::seconds(item->GetTime().toInt());
    GetMythMainWindow()->HandleMedia("Internal", item->GetMediaURL(),
        item->GetDescription(), item->GetTitle(), item->GetSubtitle(),
        QString(), item->GetSeason(), item->GetEpisode(), QString(),
        duration_cast<std::chrono::minutes>(seconds), item->GetDate().toString("yyyy"));
}

void NetBase::ShowWebVideo()
{
    ResultItem *item = GetStreamItem();

    if (!item)
        return;

    if (!item->GetPlayer().isEmpty())
    {
        const QString& cmd = item->GetPlayer();
        QStringList args = item->GetPlayerArguments();
        if (args.empty())
        {
            args += item->GetMediaURL();
            if (args.empty())
                args += item->GetURL();
        }
        else
        {
            args.replaceInStrings("%DIR%", GetConfDir() + "/MythNetvision");
            args.replaceInStrings("%MEDIAURL%", item->GetMediaURL());
            args.replaceInStrings("%URL%", item->GetURL());
            args.replaceInStrings("%TITLE%", item->GetTitle());
        }

        QString playerCommand = cmd + " " + args.join(" ");
        RunCmdWithoutScreensaver(playerCommand);
    }
    else
    {
        QString url = item->GetURL();

        LOG(VB_GENERAL, LOG_DEBUG, QString("Web URL = %1").arg(url));

        if (url.isEmpty())
            return;

        QString browser = gCoreContext->GetSetting("WebBrowserCommand", "");
        QString zoom = gCoreContext->GetSetting("WebBrowserZoomLevel", "1.0");

        if (browser.isEmpty())
        {
            ShowOkPopup(tr("No browser command set! MythNetTree needs "
                           "MythBrowser installed to display the video."));
            return;
        }

        if (browser.toLower() == "internal")
        {
            GetMythMainWindow()->HandleMedia("WebBrowser", url);
        }
        else
        {
            url.replace("mythflash://", "http://");
            QString cmd = browser;
            cmd.replace("%ZOOM%", zoom);
            cmd.replace("%URL%", url);
            cmd.replace('\'', "%27");
            cmd.replace("&","\\&");
            cmd.replace(";","\\;");

            RunCmdWithoutScreensaver(cmd);
        }
    }
}

void NetBase::RunCmdWithoutScreensaver(const QString &cmd)
{
    GetMythMainWindow()->PauseIdleTimer(true);
    MythMainWindow::DisableScreensaver();
    GetMythMainWindow()->AllowInput(false);
    myth_system(cmd, kMSDontDisableDrawing);
    GetMythMainWindow()->AllowInput(true);
    GetMythMainWindow()->PauseIdleTimer(false);
    MythMainWindow::RestoreScreensaver();
}

void NetBase::SlotDeleteVideo()
{
    QString message = tr("Are you sure you want to delete this file?");

    auto *confirmdialog = new MythConfirmationDialog(m_popupStack, message);

    if (confirmdialog->Create())
    {
        m_popupStack->AddScreen(confirmdialog);
        connect(confirmdialog, &MythConfirmationDialog::haveResult,
                this, &NetBase::DoDeleteVideo);
    }
    else
    {
        delete confirmdialog;
    }
}

void NetBase::DoDeleteVideo(bool remove)
{
    if (!remove)
        return;

    ResultItem *item = GetStreamItem();

    if (!item)
        return;

    QString filename = GetDownloadFilename(item->GetTitle(),
                                           item->GetMediaURL());

    if (filename.startsWith("myth://"))
        RemoteFile::DeleteFile(filename);
    else
    {
        QFile file(filename);
        file.remove();
    }
}

void NetBase::customEvent(QEvent *event)
{
    if (event->type() == MythEvent::kMythEventMessage)
    {
        auto *me = dynamic_cast<MythEvent *>(event);
        if (me == nullptr)
            return;
        QStringList tokens = me->Message().split(" ", Qt::SkipEmptyParts);

        if (tokens.isEmpty())
            return;

        if (tokens[0] == "DOWNLOAD_FILE")
        {
            QStringList args = me->ExtraDataList();
            if ((tokens.size() != 2) ||
                (args[1] != m_downloadFile))
                return;

            if (tokens[1] == "UPDATE")
            {
                QString message = tr("Downloading Video...\n"
                                     "(%1 of %2 MB)")
                    .arg(QString::number(args[2].toInt() / 1024.0 / 1024.0,
                                         'f', 2),
                         QString::number(args[3].toInt() / 1024.0 / 1024.0,
                                         'f', 2));
                m_progressDialog->SetMessage(message);
                m_progressDialog->SetTotal(args[3].toInt());
                m_progressDialog->SetProgress(args[2].toInt());
            }
            else if (tokens[1] == "FINISHED")
            {
                int fileSize  = args[2].toInt();
                int errorCode = args[4].toInt();

                if (m_progressDialog)
                {
                    m_progressDialog->Close();
                    m_progressDialog = nullptr;
                }

                if ((m_downloadFile.startsWith("myth://")))
                {
                    if ((errorCode == 0) &&
                        (fileSize > 0))
                    {
                        DoPlayVideo(m_downloadFile);
                    }
                    else
                    {
                        ShowOkPopup(tr("Error downloading video to backend."));
                    }
                }
            }
        }
    }
}

void NetBase::DoDownloadAndPlay()
{
    ResultItem *item = GetStreamItem();
    if (!item)
        return;

    QString baseFilename = GetDownloadFilename(item->GetTitle(),
                                               item->GetMediaURL());

    QString finalFilename = StorageGroup::generate_file_url("Default",
                                              gCoreContext->GetMasterHostName(),
                                              baseFilename);

    LOG(VB_GENERAL, LOG_INFO, QString("Downloading %1 to %2")
        .arg(item->GetMediaURL(), finalFilename));

    // Does the file already exist?
    bool exists = RemoteFile::Exists(finalFilename);

    if (exists)
    {
        DoPlayVideo(finalFilename);
        return;
    }
    DownloadVideo(item->GetMediaURL(), baseFilename);
}

void NetBase::DoPlayVideo(const QString &filename)
{
    ResultItem *item = GetStreamItem();
    if (!item)
        return;

    GetMythMainWindow()->HandleMedia("Internal", filename);
}

void NetBase::DoPlayVideo(void)
{
    ResultItem *item = GetStreamItem();
    if (!item)
        return;

    QString filename = GetDownloadFilename(item->GetTitle(),
                                           item->GetMediaURL());

    DoPlayVideo(filename);
}
