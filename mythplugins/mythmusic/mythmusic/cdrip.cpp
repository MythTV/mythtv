// Unix C includes
#include <sys/types.h>
#include <fcntl.h>

#include "config.h"

// C++ includes
#include <chrono>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <memory>

// Qt includes
#include <QApplication>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QKeyEvent>
#include <QRegularExpression>
#include <QTimer>
#include <QUrl>
#include <utility>

// MythTV includes
#include <libmythbase/lcddevice.h>
#include <libmythbase/mythcorecontext.h>
#include <libmythbase/mythdate.h>
#include <libmythbase/mythdb.h>
#include <libmythbase/mythdirs.h>
#include <libmythbase/mythlogging.h>
#include <libmythbase/mythsystemlegacy.h>
#include <libmythbase/remotefile.h>
#include <libmythbase/storagegroup.h>
#include <libmythmetadata/musicutils.h>
#include <libmythui/mediamonitor.h>
#include <libmythui/mythdialogbox.h>
#include <libmythui/mythprogressdialog.h>
#include <libmythui/mythscreenstack.h>
#include <libmythui/mythuibutton.h>
#include <libmythui/mythuibuttonlist.h>
#include <libmythui/mythuicheckbox.h>
#include <libmythui/mythuiprogressbar.h>
#include <libmythui/mythuitext.h>
#include <libmythui/mythuitextedit.h>

// MythMusic includes
#include "cdrip.h"
#ifdef HAVE_CDIO
#include "cddecoder.h"
#endif // HAVE_CDIO
#include "editmetadata.h"
#include "encoder.h"
#include "flacencoder.h"
#include "genres.h"
#include "lameencoder.h"
#include "vorbisencoder.h"

#ifdef HAVE_CDIO
// libparanoia compatibility
#ifndef cdrom_paranoia
#define cdrom_paranoia cdrom_paranoia_t
#endif // cdrom_paranoia

#ifndef CD_FRAMESIZE_RAW
# define CD_FRAMESIZE_RAW CDIO_CD_FRAMESIZE_RAW
#endif // CD_FRAMESIZE_RAW
#endif // HAVE_CDIO

const QEvent::Type RipStatusEvent::kTrackTextEvent =
    (QEvent::Type) QEvent::registerEventType();
const QEvent::Type RipStatusEvent::kOverallTextEvent =
    (QEvent::Type) QEvent::registerEventType();
const QEvent::Type RipStatusEvent::kStatusTextEvent =
    (QEvent::Type) QEvent::registerEventType();
const QEvent::Type RipStatusEvent::kTrackProgressEvent =
    (QEvent::Type) QEvent::registerEventType();
const QEvent::Type RipStatusEvent::kTrackPercentEvent =
    (QEvent::Type) QEvent::registerEventType();
const QEvent::Type RipStatusEvent::kTrackStartEvent =
    (QEvent::Type) QEvent::registerEventType();
const QEvent::Type RipStatusEvent::kOverallProgressEvent =
    (QEvent::Type) QEvent::registerEventType();
const QEvent::Type RipStatusEvent::kOverallPercentEvent =
    (QEvent::Type) QEvent::registerEventType();
const QEvent::Type RipStatusEvent::kOverallStartEvent =
    (QEvent::Type) QEvent::registerEventType();
const QEvent::Type RipStatusEvent::kCopyStartEvent =
    (QEvent::Type) QEvent::registerEventType();
const QEvent::Type RipStatusEvent::kCopyEndEvent =
    (QEvent::Type) QEvent::registerEventType();
const QEvent::Type RipStatusEvent::kFinishedEvent =
    (QEvent::Type) QEvent::registerEventType();
const QEvent::Type RipStatusEvent::kEncoderErrorEvent =
    (QEvent::Type) QEvent::registerEventType();

void CDScannerThread::run()
{
    RunProlog();
    m_parent->scanCD();
    RunEpilog();
}

///////////////////////////////////////////////////////////////////////////////

void CDEjectorThread::run()
{
    RunProlog();
    m_parent->ejectCD();
    RunEpilog();
}

///////////////////////////////////////////////////////////////////////////////

static long int getSectorCount ([[maybe_unused]] QString &cddevice,
                                [[maybe_unused]] int tracknum)
{
#ifdef HAVE_CDIO
    QByteArray devname = cddevice.toLatin1();
    cdrom_drive *device = cdda_identify(devname.constData(), 0, nullptr);

    if (!device)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Error: %1('%2',track=%3) failed at cdda_identify()").
            arg(__func__, cddevice, QString::number(tracknum)));
        return -1;
    }

    if (cdda_open(device))
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Error: %1('%2',track=%3) failed at cdda_open() - cdda not supported").
            arg(__func__, cddevice, QString::number(tracknum)));
        cdda_close(device);
        return -1;
    }

    // we only care about audio tracks
    if (cdda_track_audiop (device, tracknum))
    {
        cdda_verbose_set(device, CDDA_MESSAGE_FORGETIT, CDDA_MESSAGE_FORGETIT);
        long int start = cdda_track_firstsector(device, tracknum);
        long int end   = cdda_track_lastsector( device, tracknum);
        cdda_close(device);
        return end - start + 1;
    }
    LOG(VB_GENERAL, LOG_ERR,
        QString("Error: cdrip - cdda_track_audiop(%1) returned 0").arg(cddevice));

    cdda_close(device);
#endif // HAVE_CDIO
    return 0;
}

#ifdef HAVE_CDIO
static void paranoia_cb(long /*status*/, paranoia_cb_mode_t /*mode*/)
{
}
#endif // HAVE_CDIO

CDRipperThread::CDRipperThread(RipStatus *parent,  QString device,
                               QVector<RipTrack*> *tracks, int quality) :
    MThread("CDRipper"),
    m_parent(parent),
    m_cdDevice(std::move(device)), m_quality(quality),
    m_tracks(tracks)
{
#ifdef WIN32 // libcdio needs the drive letter with no path
    if (m_cdDevice.endsWith('\\'))
        m_cdDevice.chop(1);
#endif // WIN32

    QString lastHost = gCoreContext->GetSetting("MythMusicLastRipHost", gCoreContext->GetMasterHostName());
    QStringList dirs = StorageGroup::getGroupDirs("Music", lastHost);
    if (dirs.count() > 0)
        m_musicStorageDir = StorageGroup::getGroupDirs("Music", lastHost).at(0);
}

CDRipperThread::~CDRipperThread(void)
{
    cancel();
    wait();
}

void CDRipperThread::cancel(void)
{
    m_quit = true;
}

bool CDRipperThread::isCancelled(void) const
{
    return m_quit;
}

void CDRipperThread::run(void)
{
    RunProlog();

    if (m_tracks->empty())
    {
        RunEpilog();
        return;
    }

    m_totalSectors = 0;
    m_totalSectorsDone = 0;
    for (int trackno = 0; trackno < m_tracks->size(); trackno++)
    {
        if (!m_tracks->at(trackno)->active)
            continue;
        m_totalSectors += getSectorCount(m_cdDevice, trackno + 1);
    }

    if (!m_totalSectors)
    {
        RunEpilog();
        return;
    }

    MusicMetadata *track = m_tracks->at(0)->metadata;
    QString tots;

    if (track->Compilation())
    {
        tots = track->CompilationArtist() + " ~ " + track->Album();
    }
    else
    {
        tots = track->Artist() + " ~ " + track->Album();
    }

    QApplication::postEvent(
        m_parent,
        new RipStatusEvent(RipStatusEvent::kOverallTextEvent, tots));
    QApplication::postEvent(
        m_parent,
        new RipStatusEvent(RipStatusEvent::kOverallProgressEvent, 0));
    QApplication::postEvent(
        m_parent,
        new RipStatusEvent(RipStatusEvent::kTrackProgressEvent, 0));

    QString textstatus;
    QString encodertype = gCoreContext->GetSetting("EncoderType");
    bool mp3usevbr = gCoreContext->GetBoolSetting("Mp3UseVBR", false);

    QApplication::postEvent(m_parent,
        new RipStatusEvent(RipStatusEvent::kOverallStartEvent, m_totalSectors));

    if (LCD *lcd = LCD::Get())
    {
        QString lcd_tots = tr("Importing %1").arg(tots);
        QList<LCDTextItem> textItems;
        textItems.append(LCDTextItem(1, ALIGN_CENTERED,
                                         lcd_tots, "Generic", false));
        lcd->switchToGeneric(textItems);
    }

    MusicMetadata *titleTrack = nullptr;
    QString saveDir = GetConfDir() + "/tmp/RipTemp/";
    QString outfile;

    std::unique_ptr<Encoder> encoder;

    for (int trackno = 0; trackno < m_tracks->size(); trackno++)
    {
        if (isCancelled())
            break;

        QApplication::postEvent(
            m_parent,
            new RipStatusEvent(RipStatusEvent::kStatusTextEvent,
                               QString("Track %1 of %2")
                               .arg(trackno + 1).arg(m_tracks->size())));

        QApplication::postEvent(
            m_parent,
            new RipStatusEvent(RipStatusEvent::kTrackProgressEvent, 0));

        track = m_tracks->at(trackno)->metadata;

        if (track)
        {
            textstatus = track->Title();
            QApplication::postEvent(
                m_parent,
                new RipStatusEvent(
                    RipStatusEvent::kTrackTextEvent, textstatus));
            QApplication::postEvent(
                m_parent,
                new RipStatusEvent(RipStatusEvent::kTrackProgressEvent, 0));
            QApplication::postEvent(
                m_parent,
                new RipStatusEvent(RipStatusEvent::kTrackPercentEvent, 0));

            // do we need to start a new file?
            if (!m_tracks->at(trackno)->active)
                continue;

            titleTrack = track;
            titleTrack->setLength(m_tracks->at(trackno)->length);

            if (m_quality < 3)
            {
                if (encodertype == "mp3")
                {
                    outfile = QString("track%1.mp3").arg(trackno);
                    encoder = std::make_unique<LameEncoder>(saveDir + outfile, m_quality,
                                                  titleTrack, mp3usevbr);
                }
                else // ogg
                {
                    outfile = QString("track%1.ogg").arg(trackno);
                    encoder = std::make_unique<VorbisEncoder>(saveDir + outfile, m_quality,
                                                    titleTrack);
                }
            }
            else
            {
                outfile = QString("track%1.flac").arg(trackno);
                encoder = std::make_unique<FlacEncoder>(saveDir + outfile, m_quality,
                                              titleTrack);
            }

            if (!encoder->isValid())
            {
                QApplication::postEvent(
                    m_parent,
                    new RipStatusEvent(
                        RipStatusEvent::kEncoderErrorEvent,
                        "Encoder failed to open file for writing"));
                LOG(VB_GENERAL, LOG_ERR, "MythMusic: Encoder failed"
                                         " to open file for writing");

                RunEpilog();
                return;
            }

            if (!encoder)
            {
                // This should never happen.
                QApplication::postEvent(
                    m_parent,
                    new RipStatusEvent(RipStatusEvent::kEncoderErrorEvent,
                                       "Failed to create encoder"));
                LOG(VB_GENERAL, LOG_ERR, "MythMusic: No encoder, failing");
                RunEpilog();
                return;
            }
            ripTrack(m_cdDevice, encoder.get(), trackno + 1);

            if (isCancelled())
            {
                RunEpilog();
                return;
            }

            QString ext = QFileInfo(outfile).suffix();
            QString destFile = filenameFromMetadata(titleTrack) + '.' + ext;
            QUrl url(m_musicStorageDir);

            // save the metadata to the DB
            titleTrack->setFilename(destFile);
            titleTrack->setHostname(url.host());
            titleTrack->setFileSize((quint64)QFileInfo(outfile).size());
            titleTrack->dumpToDatabase();

            // this will delete the encoder which will write the metadata in it's dtor
            encoder.reset();

            // copy track to the BE
            destFile = MythCoreContext::GenMythURL(url.host(), 0, destFile, "Music");

            QApplication::postEvent(m_parent, new RipStatusEvent(RipStatusEvent::kCopyStartEvent, 0));
            RemoteFile::CopyFile(saveDir + outfile, destFile, true);
            QApplication::postEvent(m_parent, new RipStatusEvent(RipStatusEvent::kCopyEndEvent, 0));
        }
    }

    QString PostRipCDScript = gCoreContext->GetSetting("PostCDRipScript");

    if (!PostRipCDScript.isEmpty())
        myth_system(PostRipCDScript);

    QApplication::postEvent(
        m_parent, new RipStatusEvent(RipStatusEvent::kFinishedEvent, ""));

    RunEpilog();
}

int CDRipperThread::ripTrack([[maybe_unused]] QString &cddevice,
                             [[maybe_unused]] Encoder *encoder,
                             [[maybe_unused]] int tracknum)
{
#ifdef HAVE_CDIO
    QByteArray devname = cddevice.toLatin1();
    cdrom_drive *device = cdda_identify(devname.constData(), 0, nullptr);

    if (!device)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("cdda_identify failed for device '%1', "
                    "CDRipperThread::ripTrack(tracknum = %2) exiting.")
                .arg(cddevice).arg(tracknum));
        return -1;
    }

    if (cdda_open(device))
    {
        LOG(VB_MEDIA, LOG_INFO,
            QString("Error: %1('%2',track=%3) failed at cdda_open() - cdda not supported")
            .arg(__func__, cddevice, QString::number(tracknum)));
        cdda_close(device);
        return -1;
    }

    cdda_verbose_set(device, CDDA_MESSAGE_FORGETIT, CDDA_MESSAGE_FORGETIT);
    long int start = cdda_track_firstsector(device, tracknum);
    long int end = cdda_track_lastsector(device, tracknum);
    LOG(VB_MEDIA, LOG_INFO, QString("%1(%2,track=%3) start=%4 end=%5")
        .arg(__func__,  cddevice).arg(tracknum).arg(start).arg(end));

    cdrom_paranoia *paranoia = paranoia_init(device);
    if (gCoreContext->GetSetting("ParanoiaLevel") == "full")
    {
        paranoia_modeset(paranoia, PARANOIA_MODE_FULL |
                PARANOIA_MODE_NEVERSKIP);
    }
    else
    {
        paranoia_modeset(paranoia, PARANOIA_MODE_OVERLAP);
    }

    paranoia_seek(paranoia, start, SEEK_SET);

    long int curpos = start;

    QApplication::postEvent(
        m_parent,
        new RipStatusEvent(RipStatusEvent::kTrackStartEvent, end - start + 1));
    m_lastTrackPct = -1;
    m_lastOverallPct = -1;

    int every15 = 15;
    while (curpos < end)
    {
        int16_t *buffer = paranoia_read(paranoia, paranoia_cb);

        if (encoder->addSamples(buffer, CD_FRAMESIZE_RAW))
            break;

        curpos++;

        every15--;

        if (every15 <= 0)
        {
            every15 = 15;

            // updating the UITypes can be slow - only update if we need to:
            int newOverallPct = (int) (100.0 /  ((double) m_totalSectors /
                                                 (m_totalSectorsDone + curpos - start)));
            if (newOverallPct != m_lastOverallPct)
            {
                m_lastOverallPct = newOverallPct;
                QApplication::postEvent(
                    m_parent,
                    new RipStatusEvent(RipStatusEvent::kOverallPercentEvent,
                                       newOverallPct));
                QApplication::postEvent(
                    m_parent,
                    new RipStatusEvent(RipStatusEvent::kOverallProgressEvent,
                                       m_totalSectorsDone + curpos - start));
            }

            int newTrackPct = (int) (100.0 / ((double) (end - start + 1) / (curpos - start)));
            if (newTrackPct != m_lastTrackPct)
            {
                m_lastTrackPct = newTrackPct;
                QApplication::postEvent(
                    m_parent,
                    new RipStatusEvent(RipStatusEvent::kTrackPercentEvent,
                                       newTrackPct));
                QApplication::postEvent(
                    m_parent,
                    new RipStatusEvent(RipStatusEvent::kTrackProgressEvent,
                                       curpos - start));
            }

            if (LCD *lcd = LCD::Get())
            {
                float fProgress = (float)(m_totalSectorsDone + (curpos - start))
                                             / m_totalSectors;
                lcd->setGenericProgress(fProgress);
            }
        }

        if (isCancelled())
        {
            break;
        }
    }

    m_totalSectorsDone += end - start + 1;

    paranoia_free(paranoia);
    cdda_close(device);

    return (curpos - start + 1) * CD_FRAMESIZE_RAW;
#else
    return 0;
#endif // HAVE_CDIO
}

///////////////////////////////////////////////////////////////////////////////

Ripper::Ripper(MythScreenStack *parent, QString device) :
    MythScreenType(parent, "ripcd"),
    m_tracks(new QVector<RipTrack*>),
    m_cdDevice(std::move(device))
{
#ifndef _WIN32
    // if the MediaMonitor is running stop it
    MediaMonitor *mon = MediaMonitor::GetMediaMonitor();
    if (mon && mon->IsActive())
    {
        m_mediaMonitorActive = true;
        mon->StopMonitoring();
    }
#endif // _WIN32

    // make sure the directory where we temporarily save the rips is present
    QDir dir;
    dir.mkpath(GetConfDir() + "/tmp/RipTemp/");

    // remove any ripped tracks from the temp rip directory
    QString command = "rm -f " + GetConfDir() + "/tmp/RipTemp/*";
    myth_system(command);

    // get last host and directory we ripped to
    QString lastHost = gCoreContext->GetSetting("MythMusicLastRipHost", gCoreContext->GetMasterHostName());
    QStringList dirs = StorageGroup::getGroupDirs("Music", lastHost);
    if (dirs.count() > 0)
        m_musicStorageDir = StorageGroup::getGroupDirs("Music", lastHost).at(0);
}

Ripper::~Ripper(void)
{
    // remove any ripped tracks from the temp rip directory
    QString command = "rm -f " + GetConfDir() + "/tmp/RipTemp/*";
    myth_system(command);

    delete m_decoder;

#ifndef _WIN32
    // if the MediaMonitor was active when we started then restart it
    if (m_mediaMonitorActive)
    {
        MediaMonitor *mon = MediaMonitor::GetMediaMonitor();
        if (mon)
            mon->StartMonitoring();
    }
#endif // _WIN32

    if (m_somethingwasripped)
        emit ripFinished();
}

bool Ripper::Create(void)
{
    if (!LoadWindowFromXML("music-ui.xml", "cdripper", this))
        return false;

    m_qualityList = dynamic_cast<MythUIButtonList *>(GetChild("quality"));
    m_artistEdit = dynamic_cast<MythUITextEdit *>(GetChild("artist"));
    m_searchArtistButton = dynamic_cast<MythUIButton *>(GetChild("searchartist"));
    m_albumEdit = dynamic_cast<MythUITextEdit *>(GetChild("album"));
    m_searchAlbumButton = dynamic_cast<MythUIButton *>(GetChild("searchalbum"));
    m_genreEdit = dynamic_cast<MythUITextEdit *>(GetChild("genre"));
    m_yearEdit = dynamic_cast<MythUITextEdit *>(GetChild("year"));
    m_searchGenreButton = dynamic_cast<MythUIButton *>(GetChild("searchgenre"));
    m_compilationCheck = dynamic_cast<MythUICheckBox *>(GetChild("compilation"));
    m_switchTitleArtist = dynamic_cast<MythUIButton *>(GetChild("switch"));
    m_scanButton = dynamic_cast<MythUIButton *>(GetChild("scan"));
    m_ripButton = dynamic_cast<MythUIButton *>(GetChild("rip"));
    m_trackList = dynamic_cast<MythUIButtonList *>(GetChild("tracks"));

    BuildFocusList();

    if (!m_artistEdit || !m_scanButton || !m_ripButton || !m_switchTitleArtist
        || !m_trackList || !m_compilationCheck || !m_searchGenreButton
        || !m_yearEdit || !m_genreEdit || !m_searchArtistButton
        || !m_albumEdit || !m_searchAlbumButton || !m_qualityList)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "Missing theme elements for screen 'cdripper'");
        return false;
    }

    connect(m_trackList, &MythUIButtonList::itemClicked,
            this, qOverload<MythUIButtonListItem *>(&Ripper::toggleTrackActive));
    connect(m_ripButton, &MythUIButton::Clicked, this, &Ripper::startRipper);
    connect(m_scanButton, &MythUIButton::Clicked, this, &Ripper::startScanCD);
    connect(m_switchTitleArtist, &MythUIButton::Clicked,
            this, &Ripper::switchTitlesAndArtists);
    connect(m_compilationCheck, &MythUICheckBox::toggled,
            this, &Ripper::compilationChanged);
    connect(m_searchGenreButton, &MythUIButton::Clicked, this, &Ripper::searchGenre);
    connect(m_genreEdit, &MythUITextEdit::valueChanged, this, &Ripper::genreChanged);
    m_yearEdit->SetFilter((InputFilter)(FilterAlpha | FilterSymbols | FilterPunct));
    m_yearEdit->SetMaxLength(4);
    connect(m_yearEdit, &MythUITextEdit::valueChanged, this, &Ripper::yearChanged);
    connect(m_artistEdit, &MythUITextEdit::valueChanged, this, &Ripper::artistChanged);
    connect(m_searchArtistButton, &MythUIButton::Clicked, this, &Ripper::searchArtist);
    connect(m_albumEdit, &MythUITextEdit::valueChanged, this, &Ripper::albumChanged);
    connect(m_searchAlbumButton, &MythUIButton::Clicked, this, &Ripper::searchAlbum);

    // Populate Quality List
    new MythUIButtonListItem(m_qualityList, tr("Low"), QVariant::fromValue(0));
    new MythUIButtonListItem(m_qualityList, tr("Medium"), QVariant::fromValue(1));
    new MythUIButtonListItem(m_qualityList, tr("High"), QVariant::fromValue(2));
    new MythUIButtonListItem(m_qualityList, tr("Perfect"), QVariant::fromValue(3));
    m_qualityList->SetValueByData(QVariant::fromValue(
                        gCoreContext->GetNumSetting("DefaultRipQuality", 1)));

    QTimer::singleShot(500ms, this, &Ripper::startScanCD);

    return true;
}

bool Ripper::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget() && GetFocusWidget()->keyPressEvent(event))
        return true;

    QStringList actions;
    bool handled = GetMythMainWindow()->TranslateKeyPress("Global", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        const QString& action = actions[i];
        handled = true;

        if (action == "EDIT" || action == "INFO") // INFO purely for historical reasons
            showEditMetadataDialog(m_trackList->GetItemCurrent());
        else if (action == "MENU")
            ShowMenu();
        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void Ripper::ShowMenu()
{
    if (m_tracks->empty())
        return;

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    auto *menu = new MythDialogBox("", popupStack, "ripmusicmenu");

    if (menu->Create())
        popupStack->AddScreen(menu);
    else
    {
        delete menu;
        return;
    }

    menu->SetReturnEvent(this, "menu");
    menu->AddButton(tr("Select Where To Save Tracks"), &Ripper::chooseBackend);
    menu->AddButton(tr("Edit Track Metadata"),
                    qOverload<>(&Ripper::showEditMetadataDialog));
}

void Ripper::showEditMetadataDialog(void)
{
    showEditMetadataDialog(m_trackList->GetItemCurrent());
}

void Ripper::chooseBackend(void) const
{
    QStringList hostList;

    // get a list of hosts with a directory defined for the 'Music' storage group
    MSqlQuery query(MSqlQuery::InitCon());
    QString sql = "SELECT DISTINCT hostname "
                  "FROM storagegroup "
                  "WHERE groupname = 'Music'";
    if (!query.exec(sql) || !query.isActive())
        MythDB::DBError("Ripper::chooseBackend get host list", query);
    else
    {
        while(query.next())
        {
            hostList.append(query.value(0).toString());
        }
    }

    if (hostList.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, "Ripper::chooseBackend: No backends found");
        return;
    }

    QString msg = tr("Select where to save tracks");

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    auto *searchDlg = new MythUISearchDialog(popupStack, msg, hostList, false, "");

    if (!searchDlg->Create())
    {
        delete searchDlg;
        return;
    }

    connect(searchDlg, &MythUISearchDialog::haveResult, this, &Ripper::setSaveHost);

    popupStack->AddScreen(searchDlg);
}

void Ripper::setSaveHost(const QString& host)
{
    gCoreContext->SaveSetting("MythMusicLastRipHost", host);

    QStringList dirs = StorageGroup::getGroupDirs("Music", host);
    if (dirs.count() > 0)
        m_musicStorageDir = StorageGroup::getGroupDirs("Music", host).at(0);
}

void Ripper::startScanCD(void)
{
    if (m_scanThread)
        return;

    QString message = tr("Scanning CD. Please Wait ...");
    OpenBusyPopup(message);

    m_scanThread = new CDScannerThread(this);
    connect(m_scanThread->qthread(), &QThread::finished, this, &Ripper::ScanFinished);
    m_scanThread->start();
}

void Ripper::ScanFinished()
{
    delete m_scanThread;
    m_scanThread = nullptr;

    m_tracks->clear();

    if (m_decoder)
    {
        bool isCompilation = false;

        m_artistName.clear();
        m_albumName.clear();
        m_genreName.clear();
        m_year.clear();

        int max_tracks = m_decoder->getNumTracks();
        for (int trackno = 0; trackno < max_tracks; trackno++)
        {
            auto *ripTrack = new RipTrack;

            MusicMetadata *metadata = m_decoder->getMetadata(trackno + 1);
            if (metadata)
            {
                ripTrack->metadata = metadata;
                ripTrack->length = metadata->Length();

                if (metadata->Compilation())
                {
                    isCompilation = true;
                    m_artistName = metadata->CompilationArtist();
                }
                else if (m_artistName.isEmpty())
                {
                    m_artistName = metadata->Artist();
                }

                if (m_albumName.isEmpty())
                    m_albumName = metadata->Album();

                if (m_genreName.isEmpty() && !metadata->Genre().isEmpty())
                    m_genreName = metadata->Genre();

                if (m_year.isEmpty() && metadata->Year() > 0)
                    m_year = QString::number(metadata->Year());

                QString title = metadata->Title();
                ripTrack->isNew = isNewTune(m_artistName, m_albumName, title);

                ripTrack->active = ripTrack->isNew;

                m_tracks->push_back(ripTrack);

            }
            else
            {
                delete ripTrack;
            }
        }

        m_artistEdit->SetText(m_artistName);
        m_albumEdit->SetText(m_albumName);
        m_genreEdit->SetText(m_genreName);
        m_yearEdit->SetText(m_year);
        m_compilationCheck->SetCheckState(isCompilation);

        if (!isCompilation)
            m_switchTitleArtist->SetVisible(false);
        else
            m_switchTitleArtist->SetVisible(true);
    }

    BuildFocusList();
    updateTrackList();

    CloseBusyPopup();
}

void Ripper::scanCD(void)
{
#ifdef HAVE_CDIO
    {
    LOG(VB_MEDIA, LOG_INFO, QString("Ripper::%1 CD='%2'").
        arg(__func__, m_cdDevice));
    (void)cdio_close_tray(m_cdDevice.toLatin1().constData(), nullptr);
    }
#endif // HAVE_CDIO

    delete m_decoder;
    m_decoder = new CdDecoder("cda", nullptr, nullptr);
    if (m_decoder)
        m_decoder->setDevice(m_cdDevice);
}

void Ripper::deleteAllExistingTracks(void)
{
    // NOLINTNEXTLINE(readability-qualified-auto) // qt6
    for (auto it = m_tracks->begin(); it < m_tracks->end(); ++it)
    {
        RipTrack *track = (*it);
        if (track && !track->isNew)
        {
            if (deleteExistingTrack(track))
            {
                track->isNew = true;
                toggleTrackActive(track);
            }
        }
    }
}

bool Ripper::deleteExistingTrack(RipTrack *track)
{
    if (!track)
        return false;

    MusicMetadata *metadata = track->metadata;

    if (!metadata)
        return false;

    QString artist = metadata->Artist();
    QString album = metadata->Album();
    QString title = metadata->Title();

    MSqlQuery query(MSqlQuery::InitCon());
    QString queryString("SELECT song_id, "
            "CONCAT_WS('/', music_directories.path, music_songs.filename) AS filename "
            "FROM music_songs "
            "LEFT JOIN music_artists"
            " ON music_songs.artist_id=music_artists.artist_id "
            "LEFT JOIN music_albums"
            " ON music_songs.album_id=music_albums.album_id "
            "LEFT JOIN music_directories "
            " ON music_songs.directory_id=music_directories.directory_id "
            "WHERE artist_name REGEXP \'");
    QString token = artist;
    static const QRegularExpression punctuation
        { R"((/|\\|:|'|\,|\!|\(|\)|"|\?|\|))" };
    token.replace(punctuation, QString("."));
    queryString += token + "\' AND " + "album_name REGEXP \'";
    token = album;
    token.replace(punctuation, QString("."));
    queryString += token + "\' AND " + "name    REGEXP \'";
    token = title;
    token.replace(punctuation, QString("."));
    queryString += token + "\' ORDER BY artist_name, album_name,"
                           " name, song_id, filename LIMIT 1";
    query.prepare(queryString);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("Search music database", query);
        return false;
    }

    if (query.next())
    {
        int trackID = query.value(0).toInt();
        QString filename = query.value(1).toString();
        QUrl url(m_musicStorageDir);
        filename = MythCoreContext::GenMythURL(url.host(), 0, filename, "Music");

        // delete file
        // FIXME: RemoteFile::DeleteFile will only work with files on the master BE
        if (RemoteFile::Exists(filename) && !RemoteFile::DeleteFile(filename))
        {
            LOG(VB_GENERAL, LOG_NOTICE, QString("Ripper::deleteExistingTrack() "
                                                "Could not delete %1")
                                                .arg(filename));
            return false;
        }

        // remove database entry
        MSqlQuery deleteQuery(MSqlQuery::InitCon());
        deleteQuery.prepare("DELETE FROM music_songs"
                            " WHERE song_id = :SONG_ID");
        deleteQuery.bindValue(":SONG_ID", trackID);
        if (!deleteQuery.exec())
        {
            MythDB::DBError("Delete Track", deleteQuery);
            return false;
        }
        return true;
    }

    return false;
}

bool Ripper::somethingWasRipped() const
{
    return m_somethingwasripped;
}

void Ripper::artistChanged()
{
    QString newartist = m_artistEdit->GetText();

    if (!m_tracks->empty())
    {
        for (const auto *track : std::as_const(*m_tracks))
        {
            MusicMetadata *data = track->metadata;
            if (data)
            {
                if (m_compilationCheck->GetBooleanCheckState())
                {
                    data->setCompilationArtist(newartist);
                }
                else
                {
                    data->setArtist(newartist);
                    data->setCompilationArtist("");
                }
            }
        }

        updateTrackList();
    }

    m_artistName = newartist;
}

void Ripper::albumChanged()
{
    QString newalbum = m_albumEdit->GetText();

    if (!m_tracks->empty())
    {
        for (const auto *track : std::as_const(*m_tracks))
        {
            MusicMetadata *data = track->metadata;
            if (data)
                data->setAlbum(newalbum);
        }
    }

    m_albumName = newalbum;
}

void Ripper::genreChanged()
{
    QString newgenre = m_genreEdit->GetText();

    if (!m_tracks->empty())
    {
        for (const auto *track : std::as_const(*m_tracks))
        {
            MusicMetadata *data = track->metadata;
            if (data)
                data->setGenre(newgenre);
        }
    }

    m_genreName = newgenre;
}

void Ripper::yearChanged()
{
    QString newyear = m_yearEdit->GetText();

    if (!m_tracks->empty())
    {
        for (const auto *track : std::as_const(*m_tracks))
        {
            MusicMetadata *data = track->metadata;
            if (data)
                data->setYear(newyear.toInt());
        }
    }

    m_year = newyear;
}

void Ripper::compilationChanged(bool state)
{
    if (!state)
    {
        if (!m_tracks->empty())
        {
            // Update artist MetaData of each track on the ablum...
            for (const auto *track : std::as_const(*m_tracks))
            {
                MusicMetadata *data = track->metadata;
                if (data)
                {
                    data->setCompilationArtist("");
                    data->setArtist(m_artistName);
                    data->setCompilation(false);
                }
            }
        }

        m_switchTitleArtist->SetVisible(false);
    }
    else
    {
        if (!m_tracks->empty())
        {
            // Update artist MetaData of each track on the album...
            for (const auto *track : std::as_const(*m_tracks))
            {
                MusicMetadata *data = track->metadata;

                if (data)
                {
                    data->setCompilationArtist(m_artistName);
                    data->setCompilation(true);
                }
            }
        }

        m_switchTitleArtist->SetVisible(true);
    }

    BuildFocusList();
    updateTrackList();
}

void Ripper::switchTitlesAndArtists()
{
    if (!m_compilationCheck->GetBooleanCheckState())
        return;

    // Switch title and artist for each track
    QString tmp;
    if (!m_tracks->empty())
    {
        for (const auto *track : std::as_const(*m_tracks))
        {
            MusicMetadata *data = track->metadata;

            if (data)
            {
                tmp = data->Artist();
                data->setArtist(data->Title());
                data->setTitle(tmp);
            }
        }

        updateTrackList();
    }
}

void Ripper::startRipper(void)
{
    if (m_tracks->isEmpty())
    {
        ShowOkPopup(tr("There are no tracks to rip?"));
        return;
    }

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    int quality = m_qualityList->GetItemCurrent()->GetData().toInt();

    auto *statusDialog = new RipStatus(mainStack, m_cdDevice, m_tracks, quality);

    if (statusDialog->Create())
    {
        connect(statusDialog, &RipStatus::Result, this, &Ripper::RipComplete);
        mainStack->AddScreen(statusDialog);
    }
    else
    {
        delete statusDialog;
    }
}

void Ripper::RipComplete(bool result)
{
    if (result)
    {
        bool EjectCD = gCoreContext->GetBoolSetting("EjectCDAfterRipping", true);
        if (EjectCD)
            startEjectCD();

        ShowOkPopup(tr("Rip completed successfully."));

        m_somethingwasripped = true;
    }

    if (LCD *lcd = LCD::Get())
        lcd->switchToTime();
}


void Ripper::startEjectCD()
{
    if (m_ejectThread)
        return;

    QString message = tr("Ejecting CD. Please Wait ...");

    OpenBusyPopup(message);

    m_ejectThread = new CDEjectorThread(this);
    connect(m_ejectThread->qthread(),
            &QThread::finished, this, &Ripper::EjectFinished);
    m_ejectThread->start();
}

void Ripper::EjectFinished()
{
    delete m_ejectThread;
    m_ejectThread = nullptr;

    CloseBusyPopup();
}

void Ripper::ejectCD()
{
    LOG(VB_MEDIA, LOG_INFO, __PRETTY_FUNCTION__);
    bool bEjectCD = gCoreContext->GetBoolSetting("EjectCDAfterRipping",true);
    if (bEjectCD)
    {
#ifdef HAVE_CDIO
        LOG(VB_MEDIA, LOG_INFO, QString("Ripper::%1 '%2'").
            arg(__func__, m_cdDevice));
        (void)cdio_eject_media_drive(m_cdDevice.toLatin1().constData());
#else
        MediaMonitor *mon = MediaMonitor::GetMediaMonitor();
        if (mon)
        {
            QByteArray devname = m_cdDevice.toLatin1();
            MythMediaDevice *pMedia = mon->GetMedia(devname.constData());
            if (pMedia && mon->ValidateAndLock(pMedia))
            {
                pMedia->eject();
                mon->Unlock(pMedia);
            }
        }
#endif // HAVE_CDIO
    }
}

void Ripper::updateTrackList(void)
{
    if (m_tracks->isEmpty())
        return;

    if (m_trackList)
    {
        m_trackList->Reset();

        for (int i = 0; i < m_tracks->size(); i++)
        {
            if (i >= m_tracks->size())
                break;

            RipTrack *track = m_tracks->at(i);
            MusicMetadata *metadata = track->metadata;

            auto *item = new MythUIButtonListItem(m_trackList,"");

            item->setCheckable(true);

            item->SetData(QVariant::fromValue(track));

            if (track->isNew)
                item->DisplayState("new", "yes");
            else
                item->DisplayState("new", "no");

            if (track->active)
                item->setChecked(MythUIButtonListItem::FullChecked);
            else
                item->setChecked(MythUIButtonListItem::NotChecked);

            item->SetText(QString::number(metadata->Track()), "track");
            item->SetText(metadata->Title(), "title");
            item->SetText(metadata->Artist(), "artist");

            if (track->length >= 1s)
            {
                item->SetText(MythDate::formatTime(track->length, "mm:ss"), "length");
            }
            else
            {
                item->SetText("", "length");
            }

//             if (i == m_currentTrack)
//                 m_trackList->SetItemCurrent(i);
        }
    }
}

void Ripper::searchArtist() const
{
    QString msg = tr("Select an Artist");
    QStringList searchList = MusicMetadata::fillFieldList("artist");

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    auto *searchDlg = new MythUISearchDialog(popupStack, msg, searchList, false, "");

    if (!searchDlg->Create())
    {
        delete searchDlg;
        return;
    }

    connect(searchDlg, &MythUISearchDialog::haveResult, this, &Ripper::setArtist);

    popupStack->AddScreen(searchDlg);
}

void Ripper::setArtist(const QString& artist)
{
    m_artistEdit->SetText(artist);
}

void Ripper::searchAlbum() const
{
    QString msg = tr("Select an Album");
    QStringList searchList = MusicMetadata::fillFieldList("album");

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    auto *searchDlg = new MythUISearchDialog(popupStack, msg, searchList, false, "");

    if (!searchDlg->Create())
    {
        delete searchDlg;
        return;
    }

    connect(searchDlg, &MythUISearchDialog::haveResult, this, &Ripper::setAlbum);

    popupStack->AddScreen(searchDlg);
}

void Ripper::setAlbum(const QString& album)
{
    m_albumEdit->SetText(album);
}

void Ripper::searchGenre()
{
    QString msg = tr("Select a Genre");
    QStringList searchList = MusicMetadata::fillFieldList("genre");
    // load genre list
    m_searchList.clear();
    for (const auto & genre : genre_table)
        m_searchList.push_back(QString::fromStdString(genre));
    m_searchList.sort();

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    auto *searchDlg = new MythUISearchDialog(popupStack, msg, searchList, false, "");

    if (!searchDlg->Create())
    {
        delete searchDlg;
        return;
    }

    connect(searchDlg, &MythUISearchDialog::haveResult, this, &Ripper::setGenre);

    popupStack->AddScreen(searchDlg);
}

void Ripper::setGenre(const QString& genre)
{
    m_genreEdit->SetText(genre);
}

void Ripper::showEditMetadataDialog(MythUIButtonListItem *item)
{
    if (!item || m_tracks->isEmpty())
        return;

    auto *track = item->GetData().value<RipTrack *>();

    if (!track)
        return;

    MusicMetadata *editMeta = track->metadata;

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    auto *editDialog = new EditMetadataDialog(mainStack, editMeta);
    editDialog->setSaveMetadataOnly();

    if (!editDialog->Create())
    {
        delete editDialog;
        return;
    }

    connect(editDialog, &EditMetadataCommon::metadataChanged, this, &Ripper::metadataChanged);

    mainStack->AddScreen(editDialog);
}

void Ripper::metadataChanged(void)
{
    updateTrackList();
}

void Ripper::toggleTrackActive(RipTrack* track)
{
    QVariant data = QVariant::fromValue(track);
    MythUIButtonListItem *item = m_trackList->GetItemByData(data);
    if (item)
    {
        toggleTrackActive(item);
    }
}

void Ripper::toggleTrackActive(MythUIButtonListItem *item)
{
    if (m_tracks->isEmpty() || !item)
        return;

    int pos = m_trackList->GetItemPos(item);

    // sanity check item position
    if (pos < 0 || pos > m_tracks->count() - 1)
        return;

    RipTrack *track = m_tracks->at(pos);

    if (!track->active && !track->isNew)
    {
        ShowConflictMenu(track);
        return;
    }

    track->active = !track->active;

    if (track->active)
        item->setChecked(MythUIButtonListItem::FullChecked);
    else
        item->setChecked(MythUIButtonListItem::NotChecked);

    updateTrackLengths();
}

void Ripper::ShowConflictMenu(RipTrack* track)
{
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    QString msg = tr("This track has been disabled because it is already "
                     "present in the database.\n"
                     "Do you want to permanently delete the existing "
                     "file(s)?");
    auto *menu = new MythDialogBox(msg, popupStack, "conflictmenu", true);

    if (menu->Create())
        popupStack->AddScreen(menu);
    else
    {
        delete menu;
        return;
    }

    menu->SetReturnEvent(this, "conflictmenu");
    menu->AddButton(tr("No, Cancel"));
    menu->AddButtonV(tr("Yes, Delete"), QVariant::fromValue(track));
    menu->AddButton(tr("Yes, Delete All"));
}

void Ripper::updateTrackLengths()
{
    std::chrono::milliseconds length = 0ms;

    // NOLINTNEXTLINE(readability-qualified-auto) // qt6
    for (auto it = m_tracks->end() - 1; it == m_tracks->begin(); --it)
    {
        RipTrack *track = *it;
        if (track->active)
        {
            track->length = length + track->metadata->Length();
            length = 0ms;
        }
        else
        {
            track->length = 0ms;
            length += track->metadata->Length();
        }
    }
}

void Ripper::customEvent(QEvent* event)
{
    if (event->type() == DialogCompletionEvent::kEventType)
    {
        auto *dce = dynamic_cast<DialogCompletionEvent *>(event);
        if (dce == nullptr)
            return;
        if (dce->GetId() == "conflictmenu")
        {
            int buttonNum = dce->GetResult();
            auto *track = dce->GetData().value<RipTrack *>();

            switch (buttonNum)
            {
                case 0:
                    // Do nothing
                    break;
                case 1:
                    if (deleteExistingTrack(track))
                    {
                        track->isNew = true;
                        toggleTrackActive(track);
                    }
                    break;
                case 2:
                    deleteAllExistingTracks();
                    break;
                default:
                    break;
            }
        }

        return;
    }

    MythUIType::customEvent(event);
}


///////////////////////////////////////////////////////////////////////////////

RipStatus::~RipStatus(void)
{
    delete m_ripperThread;
    if (LCD *lcd = LCD::Get())
        lcd->switchToTime();
}

bool RipStatus::Create(void)
{
    if (!LoadWindowFromXML("music-ui.xml", "ripstatus", this))
        return false;

    m_overallText = dynamic_cast<MythUIText *>(GetChild("overall"));
    m_trackText = dynamic_cast<MythUIText *>(GetChild("track"));
    m_statusText = dynamic_cast<MythUIText *>(GetChild("status"));
    m_trackPctText = dynamic_cast<MythUIText *>(GetChild("trackpct"));
    m_overallPctText = dynamic_cast<MythUIText *>(GetChild("overallpct"));

    m_overallProgress = dynamic_cast<MythUIProgressBar *>(GetChild("overall_progress"));
    m_trackProgress = dynamic_cast<MythUIProgressBar *>(GetChild("track_progress"));

    BuildFocusList();

    startRip();

    return true;
}

bool RipStatus::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget() && GetFocusWidget()->keyPressEvent(event))
        return true;

    QStringList actions;
    bool handled = GetMythMainWindow()->TranslateKeyPress("Global", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        const QString& action = actions[i];
        handled = true;


        if (action == "ESCAPE" &&
            m_ripperThread && m_ripperThread->isRunning())
        {
            MythConfirmationDialog *dialog =
                ShowOkPopup(tr("Cancel ripping the CD?"), true);
            if (dialog)
                dialog->SetReturnEvent(this, "stop_ripping");
        }
        else
        {
            handled = false;
        }
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void RipStatus::customEvent(QEvent *event)
{
    if (event->type() == DialogCompletionEvent::kEventType)
    {
        auto *dce = dynamic_cast<DialogCompletionEvent *>(event);
        if (dce == nullptr)
            return;
        if ((dce->GetId() == "stop_ripping") && (dce->GetResult() != 0))
        {
            m_ripperThread->cancel();
            m_ripperThread->wait();
            Close();
        }

        return;
    }

    auto *rse = dynamic_cast<RipStatusEvent *> (event);
    if (!rse)
        return;

    if (event->type() == RipStatusEvent::kTrackTextEvent)
    {
        if (m_trackText)
            m_trackText->SetText(rse->m_text);
    }
    else if (event->type() == RipStatusEvent::kOverallTextEvent)
    {
        if (m_overallText)
            m_overallText->SetText(rse->m_text);
    }
    else if (event->type() == RipStatusEvent::kStatusTextEvent)
    {
        if (m_statusText)
            m_statusText->SetText(rse->m_text);
    }
    else if (event->type() == RipStatusEvent::kTrackProgressEvent)
    {
        if (m_trackProgress)
            m_trackProgress->SetUsed(rse->m_value);
    }
    else if (event->type() == RipStatusEvent::kTrackPercentEvent)
    {
        if (m_trackPctText)
            m_trackPctText->SetText(QString("%1%").arg(rse->m_value));
    }
    else if (event->type() == RipStatusEvent::kTrackStartEvent)
    {
        if (m_trackProgress)
            m_trackProgress->SetTotal(rse->m_value);
    }
    else if (event->type() == RipStatusEvent::kCopyStartEvent)
    {
        if (m_trackPctText)
            m_trackPctText->SetText(tr("Copying Track ..."));
    }
    else if (event->type() == RipStatusEvent::kCopyEndEvent)
    {
        if (m_trackPctText)
            m_trackPctText->SetText("");
    }
    else if (event->type() == RipStatusEvent::kOverallProgressEvent)
    {
        if (m_overallProgress)
            m_overallProgress->SetUsed(rse->m_value);
    }
    else if (event->type() == RipStatusEvent::kOverallStartEvent)
    {
        if (m_overallProgress)
            m_overallProgress->SetTotal(rse->m_value);
    }
    else if (event->type() == RipStatusEvent::kOverallPercentEvent)
    {
        if (m_overallPctText)
            m_overallPctText->SetText(QString("%1%").arg(rse->m_value));
    }
    else if (event->type() == RipStatusEvent::kFinishedEvent)
    {
        emit Result(true);
        Close();
    }
    else if (event->type() == RipStatusEvent::kEncoderErrorEvent)
    {
        ShowOkPopup(tr("The encoder failed to create the file.\n"
                       "Do you have write permissions"
                       " for the music directory?"));
        Close();
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, "Received an unknown event type!");
    }
}

void RipStatus::startRip(void)
{
    delete m_ripperThread;
    m_ripperThread = new CDRipperThread(this, m_cdDevice, m_tracks, m_quality);
    m_ripperThread->start();
}
