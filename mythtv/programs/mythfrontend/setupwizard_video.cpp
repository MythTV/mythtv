// Qt
#include <QFileInfo>
#include <QString>
#include <QVariant>

// MythTV
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdbcon.h"
#include "libmythbase/mythdirs.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/remotefile.h"
#include "libmythbase/remoteutil.h"
#include "libmythbase/storagegroup.h"
#include "libmythui/mythprogressdialog.h"

// MythFrontend
#include "setupwizard_audio.h"
#include "setupwizard_general.h"
#include "setupwizard_video.h"

const QString VIDEO_SAMPLE_HD_LOCATION =
                QString("http://services.mythtv.org/samples/video/?sample=HD");
const QString VIDEO_SAMPLE_SD_LOCATION =
                QString("http://services.mythtv.org/samples/video/?sample=SD");
const QString VIDEO_SAMPLE_HD_FILENAME =
                QString("mythtv_video_test_HD_19000Kbps_H264.mkv");
const QString VIDEO_SAMPLE_SD_FILENAME =
                QString("mythtv_video_test_SD_6000Kbps_H264.mkv");

VideoSetupWizard::VideoSetupWizard(MythScreenStack *parent,
                                   MythScreenType *general,
                                   MythScreenType *audio, const char *name)
    : MythScreenType(parent, name),
      m_generalScreen(general),             m_audioScreen(audio)
{
    m_popupStack = GetMythMainWindow()->GetStack("popup stack");

    gCoreContext->addListener(this);
}

bool VideoSetupWizard::Create()
{
    // Load the theme for this screen
    bool foundtheme = LoadWindowFromXML("config-ui.xml", "videowizard", this);
    if (!foundtheme)
        return false;

    m_playbackProfileButtonList =
        dynamic_cast<MythUIButtonList *> (GetChild("playbackprofiles"));

    m_testSDButton = dynamic_cast<MythUIButton *> (GetChild("testsd"));
    m_testHDButton = dynamic_cast<MythUIButton *> (GetChild("testhd"));

    m_nextButton = dynamic_cast<MythUIButton *> (GetChild("next"));
    m_prevButton = dynamic_cast<MythUIButton *> (GetChild("previous"));

    if (!m_playbackProfileButtonList || !m_testSDButton ||
        !m_testHDButton ||!m_nextButton || !m_prevButton)
    {
        LOG(VB_GENERAL, LOG_ERR, "Theme is missing critical theme elements.");
        return false;
    }

    m_playbackProfileButtonList->SetHelpText( tr("Select from one of the "
                                "preconfigured playback profiles.  When "
                                "satisfied, you can test Standard Definition "
                                "and High Definition playback with the selected "
                                "profile before moving on.") );
    m_testSDButton->SetHelpText( tr("Test your playback settings with Standard "
                                    "Definition content. (480p)") );
    m_testHDButton->SetHelpText( tr("Test your playback settings with High "
                                    "Definition content (1080p).") );
    m_nextButton->SetHelpText( tr("Save these changes and move on to the "
                                  "next configuration step.") );
    m_prevButton->SetHelpText(tr("Return to the previous configuration step."));

    connect(m_testSDButton, &MythUIButton::Clicked, this, &VideoSetupWizard::testSDVideo);
    connect(m_testHDButton, &MythUIButton::Clicked, this, &VideoSetupWizard::testHDVideo);
    connect(m_nextButton, &MythUIButton::Clicked, this, &VideoSetupWizard::slotNext);
    connect(m_prevButton, &MythUIButton::Clicked, this, &VideoSetupWizard::slotPrevious);

    BuildFocusList();
    loadData();

    return true;
}

VideoSetupWizard::~VideoSetupWizard()
{
    gCoreContext->removeListener(this);
}

void VideoSetupWizard::loadData(void)
{
    QStringList profiles = MythVideoProfile::GetProfiles(gCoreContext->GetHostName());

    for (const auto & prof : std::as_const(profiles))
    {
        auto *item = new MythUIButtonListItem(m_playbackProfileButtonList, prof);
        item->SetData(prof);
    }

    QString currentpbp = MythVideoProfile::GetDefaultProfileName(gCoreContext->GetHostName());
    if (!currentpbp.isEmpty())
    {
        MythUIButtonListItem *set =
                m_playbackProfileButtonList->GetItemByData(currentpbp);
        m_playbackProfileButtonList->SetItemCurrent(set);
    }
}

void VideoSetupWizard::slotNext(void)
{
    save();

    if (m_audioScreen)
    {
        m_audioScreen->Close();
        m_audioScreen = nullptr;
    }

    if (m_generalScreen)
    {
        m_generalScreen->Close();
        m_generalScreen = nullptr;
    }

    Close();
}

void VideoSetupWizard::save(void)
{
    QString desiredpbp =
        m_playbackProfileButtonList->GetItemCurrent()->GetText();
    MythVideoProfile::SetDefaultProfileName(desiredpbp, gCoreContext->GetHostName());
}

void VideoSetupWizard::slotPrevious(void)
{
    Close();
}

bool VideoSetupWizard::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;

    if (MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void VideoSetupWizard::testSDVideo(void)
{
    QString sdtestfile = StorageGroup::generate_file_url("Temp",
                              gCoreContext->GetMasterHostName(),
                              VIDEO_SAMPLE_SD_FILENAME);
    QString desiredpbp =
        m_playbackProfileButtonList->GetItemCurrent()->GetText();
    QString desc = tr("A short test of your system's playback of "
                    "Standard Definition content with the %1 profile.")
                    .arg(desiredpbp);
    QString title = tr("Standard Definition Playback Test");

    if (!RemoteFile::Exists(sdtestfile))
    {
        m_testType = ttStandardDefinition;
        DownloadSample(VIDEO_SAMPLE_SD_LOCATION, VIDEO_SAMPLE_SD_FILENAME);
    }
    else
    {
        playVideoTest(desc, title, sdtestfile);
    }
}

void VideoSetupWizard::testHDVideo(void)
{
    QString hdtestfile = StorageGroup::generate_file_url("Temp",
                              gCoreContext->GetMasterHostName(),
                              VIDEO_SAMPLE_HD_FILENAME);
    QString desiredpbp =
        m_playbackProfileButtonList->GetItemCurrent()->GetText();
    QString desc = tr("A short test of your system's playback of "
                    "High Definition content with the %1 profile.")
                    .arg(desiredpbp);
    QString title = tr("High Definition Playback Test");

    if (!RemoteFile::Exists(hdtestfile))
    {
        m_testType = ttHighDefinition;
        DownloadSample(VIDEO_SAMPLE_HD_LOCATION, VIDEO_SAMPLE_HD_FILENAME);
    }
    else
    {
        playVideoTest(desc, title, hdtestfile);
    }
}

void VideoSetupWizard::playVideoTest(const QString& desc, const QString& title, const QString& file)
{
    QString desiredpbp =
        m_playbackProfileButtonList->GetItemCurrent()->GetText();
    QString currentpbp = MythVideoProfile::GetDefaultProfileName(gCoreContext->GetHostName());

    MythVideoProfile::SetDefaultProfileName(desiredpbp, gCoreContext->GetHostName());
    GetMythMainWindow()->HandleMedia("Internal", file, desc, title);
    MythVideoProfile::SetDefaultProfileName(currentpbp, gCoreContext->GetHostName());
}

void VideoSetupWizard::DownloadSample(const QString& url, const QString& dest)
{
    initProgressDialog();
    m_downloadFile = RemoteDownloadFile(url, "Temp", dest);
}

void VideoSetupWizard::initProgressDialog()
{
    QString message = tr("Downloading Video Sample...");
    m_progressDialog = new MythUIProgressDialog(message,
               m_popupStack, "sampledownloadprogressdialog");

    if (m_progressDialog->Create())
    {
        m_popupStack->AddScreen(m_progressDialog, false);
    }
    else
    {
        delete m_progressDialog;
        m_progressDialog = nullptr;
    }
}

void VideoSetupWizard::customEvent(QEvent *e)
{
    if (e->type() == MythEvent::kMythEventMessage)
    {
        auto *me = dynamic_cast<MythEvent *>(e);
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
                QString message = tr("Downloading Video Sample...\n"
                                     "(%1 of %2 MB)")
                                     .arg(QString::number(args[2].toInt() / 1024.0 / 1024.0, 'f', 2),
                                          QString::number(args[3].toInt() / 1024.0 / 1024.0, 'f', 2));
                m_progressDialog->SetMessage(message);
                m_progressDialog->SetTotal(args[3].toInt());
                m_progressDialog->SetProgress(args[2].toInt());
            }
            else if (tokens[1] == "FINISHED")
            {
                int fileSize  = args[2].toInt();
                int errorCode = args[4].toInt();

                if (m_progressDialog)
                    m_progressDialog->Close();

                if ((m_downloadFile.startsWith("myth://")))
                {
                    if ((errorCode == 0) &&
                        (fileSize > 0))
                    {
                        if (m_testType == ttHighDefinition)
                            testHDVideo();
                        else if (m_testType == ttStandardDefinition)
                            testSDVideo();
                    }
                    else
                    {
                        ShowOkPopup(tr("Error downloading sample to backend."));
                    }
                }
            }
        }
    }
}
