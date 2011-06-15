// ANSI C includes
#include <cstdio>
#include <cstring>

// Unix C includes
#include <sys/types.h>
#include <fcntl.h>

// Linux C includes
#include "config.h"
#ifdef HAVE_CDAUDIO
#include <cdaudio.h>
extern "C" {
#include <cdda_interface.h>
#include <cdda_paranoia.h>
}
#endif

// C++ includes
#include <iostream>
#include <memory>
using namespace std;

// Qt includes
#include <QApplication>
#include <QDir>
#include <QRegExp>
#include <QKeyEvent>
#include <QEvent>

// MythTV plugin includes
#include <mythcontext.h>
#include <mythdb.h>
#include <lcddevice.h>
#include <mythmediamonitor.h>
#include <dialogbox.h>

// MythUI
#include <mythdialogbox.h>
#include <mythuitext.h>
#include <mythuicheckbox.h>
#include <mythuitextedit.h>
#include <mythuibutton.h>
#include <mythuiprogressbar.h>
#include <mythuibuttonlist.h>
#include <mythsystem.h>

// MythUI headers
#include <mythtv/libmythui/mythscreenstack.h>
#include <mythtv/libmythui/mythprogressdialog.h>

// MythMusic includes
#include "cdrip.h"
#include "cddecoder.h"
#include "encoder.h"
#include "vorbisencoder.h"
#include "lameencoder.h"
#include "flacencoder.h"
#include "genres.h"
#include "editmetadata.h"
#include "mythlogging.h"

QEvent::Type RipStatusEvent::kTrackTextEvent =
    (QEvent::Type) QEvent::registerEventType();
QEvent::Type RipStatusEvent::kOverallTextEvent =
    (QEvent::Type) QEvent::registerEventType();
QEvent::Type RipStatusEvent::kStatusTextEvent =
    (QEvent::Type) QEvent::registerEventType();
QEvent::Type RipStatusEvent::kTrackProgressEvent =
    (QEvent::Type) QEvent::registerEventType();
QEvent::Type RipStatusEvent::kTrackPercentEvent =
    (QEvent::Type) QEvent::registerEventType();
QEvent::Type RipStatusEvent::kTrackStartEvent =
    (QEvent::Type) QEvent::registerEventType();
QEvent::Type RipStatusEvent::kOverallProgressEvent =
    (QEvent::Type) QEvent::registerEventType();
QEvent::Type RipStatusEvent::kOverallPercentEvent =
    (QEvent::Type) QEvent::registerEventType();
QEvent::Type RipStatusEvent::kOverallStartEvent =
    (QEvent::Type) QEvent::registerEventType();
QEvent::Type RipStatusEvent::kFinishedEvent =
    (QEvent::Type) QEvent::registerEventType();
QEvent::Type RipStatusEvent::kEncoderErrorEvent =
    (QEvent::Type) QEvent::registerEventType();

CDScannerThread::CDScannerThread(Ripper *ripper)
{
    m_parent = ripper;
}

void CDScannerThread::run()
{
    threadRegister("CDScanner");
    m_parent->scanCD();
    threadDeregister();
}

///////////////////////////////////////////////////////////////////////////////

CDEjectorThread::CDEjectorThread(Ripper *ripper)
{
    m_parent = ripper;
}

void CDEjectorThread::run()
{
    threadRegister("CDEjector");
    m_parent->ejectCD();
    threadDeregister();
}

///////////////////////////////////////////////////////////////////////////////

static long int getSectorCount (QString &cddevice, int tracknum)
{
#ifdef HAVE_CDAUDIO
    QByteArray devname = cddevice.toAscii();
    cdrom_drive *device = cdda_identify(devname.constData(), 0, NULL);

    if (!device)
        return -1;

    if (cdda_open(device))
    {
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

    cdda_close(device);
#else
    (void)cddevice; (void)tracknum;
#endif
    return 0;
}

static void paranoia_cb(long inpos, int function)
{
    inpos = inpos; function = function;
}

CDRipperThread::CDRipperThread(RipStatus *parent,  QString device,
                               QVector<RipTrack*> *tracks, int quality) :
    m_parent(parent),   m_quit(false),
    m_CDdevice(device), m_quality(quality),
    m_tracks(tracks), m_totalSectors(0),
    m_totalSectorsDone(0), m_lastTrackPct(0),
    m_lastOverallPct(0)
{
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

bool CDRipperThread::isCancelled(void)
{
    return m_quit;
}

void CDRipperThread::run(void)
{
    if (!m_tracks->size() > 0)
        return;

    threadRegister("CDRipper");
    Metadata *track = m_tracks->at(0)->metadata;
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
    bool mp3usevbr = gCoreContext->GetNumSetting("Mp3UseVBR", 0);

    m_totalSectors = 0;
    m_totalSectorsDone = 0;
    for (int trackno = 0; trackno < m_tracks->size(); trackno++)
    {
        m_totalSectors += getSectorCount(m_CDdevice, trackno + 1);
    }

    QApplication::postEvent(m_parent,
        new RipStatusEvent(RipStatusEvent::kOverallStartEvent, m_totalSectors));

    if (LCD *lcd = LCD::Get())
    {
        QString lcd_tots = QObject::tr("Importing ") + tots;
        QList<LCDTextItem> textItems;
        textItems.append(LCDTextItem(1, ALIGN_CENTERED,
                                         lcd_tots, "Generic", false));
        lcd->switchToGeneric(textItems);
    }

    Metadata *titleTrack = NULL;
    QString outfile;

    std::auto_ptr<Encoder> encoder;

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
            if (m_tracks->at(trackno)->active)
            {
                titleTrack = track;
                titleTrack->setLength(m_tracks->at(trackno)->length);

                outfile = Ripper::filenameFromMetadata(track);

                if (m_quality < 3)
                {
                    if (encodertype == "mp3")
                    {
                        outfile += ".mp3";
                        encoder.reset(new LameEncoder(outfile, m_quality,
                                                      titleTrack, mp3usevbr));
                    }
                    else // ogg
                    {
                        outfile += ".ogg";
                        encoder.reset(new VorbisEncoder(outfile, m_quality,
                                                        titleTrack));
                    }
                }
                else
                {
                    outfile += ".flac";
                    encoder.reset(new FlacEncoder(outfile, m_quality,
                                                  titleTrack));
                }

                if (!encoder->isValid())
                {
                    QApplication::postEvent(
                        m_parent,
                        new RipStatusEvent(
                            RipStatusEvent::kEncoderErrorEvent,
                            "Encoder failed to open file for writing"));
                    VERBOSE(VB_IMPORTANT, "MythMusic: Encoder failed"
                                          " to open file for writing");

                    return;
                }
            }

            if (!encoder.get())
            {
                // This should never happen.
                QApplication::postEvent(
                    m_parent,
                    new RipStatusEvent(RipStatusEvent::kEncoderErrorEvent,
                                       "Failed to create encoder"));
                VERBOSE(VB_IMPORTANT,
                        QString("MythMusic: Error: No encoder, failing"));
                return;
            }
            ripTrack(m_CDdevice, encoder.get(), trackno + 1);

            if (isCancelled())
                return;

            // save the metadata to the DB
            if (m_tracks->at(trackno)->active)
            {
                titleTrack->setFilename(outfile);
                titleTrack->dumpToDatabase();
            }
        }
    }

    QString PostRipCDScript = gCoreContext->GetSetting("PostCDRipScript");

    if (!PostRipCDScript.isEmpty())
	myth_system(PostRipCDScript);

    QApplication::postEvent(
        m_parent, new RipStatusEvent(RipStatusEvent::kFinishedEvent, ""));

    threadDeregister();
}

int CDRipperThread::ripTrack(QString &cddevice, Encoder *encoder, int tracknum)
{
#ifdef HAVE_CDAUDIO  // && HAVE_CDPARANOIA
    QByteArray devname = cddevice.toAscii();
    cdrom_drive *device = cdda_identify(devname.constData(), 0, NULL);

    if (!device)
    {
        VERBOSE(VB_IMPORTANT,
                QString("Error: cdda_identify failed for device '%1', "
                        "CDRipperThread::ripTrack(tracknum = %2) exiting.")
                .arg(cddevice).arg(tracknum));
        return -1;
    }

    if (cdda_open(device))
    {
        cdda_close(device);
        return -1;
    }

    cdda_verbose_set(device, CDDA_MESSAGE_FORGETIT, CDDA_MESSAGE_FORGETIT);
    long int start = cdda_track_firstsector(device, tracknum);
    long int end = cdda_track_lastsector(device, tracknum);

    cdrom_paranoia *paranoia = paranoia_init(device);
    if (gCoreContext->GetSetting("ParanoiaLevel") == "full")
        paranoia_modeset(paranoia, PARANOIA_MODE_FULL |
                PARANOIA_MODE_NEVERSKIP);
    else
        paranoia_modeset(paranoia, PARANOIA_MODE_OVERLAP);

    paranoia_seek(paranoia, start, SEEK_SET);

    long int curpos = start;
    int16_t *buffer;

    QApplication::postEvent(
        m_parent,
        new RipStatusEvent(RipStatusEvent::kTrackStartEvent, end - start + 1));
    m_lastTrackPct = -1;
    m_lastOverallPct = -1;

    int every15 = 15;
    while (curpos < end)
    {
        buffer = paranoia_read(paranoia, paranoia_cb);

        if (encoder->addSamples(buffer, CD_FRAMESIZE_RAW))
            break;

        curpos++;

        every15--;

        if (every15 <= 0)
        {
            every15 = 15;

            // updating the UITypes can be slow - only update if we need to:
            int newOverallPct = (int) (100.0 /  (double) ((double) m_totalSectors /
                    (double) (m_totalSectorsDone + curpos - start)));
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

            int newTrackPct = (int) (100.0 / (double) ((double) (end - start + 1) /
                    (double) (curpos - start)));
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
    (void)cddevice; (void)encoder; (void)tracknum;
    return 0;
#endif
}

///////////////////////////////////////////////////////////////////////////////

Ripper::Ripper(MythScreenStack *parent, QString device)
       : MythScreenType(parent, "ripcd"),
         m_ejectThread(NULL), m_scanThread(NULL)
{
    m_CDdevice = device;

#ifndef _WIN32
    // if the MediaMonitor is running stop it
    m_mediaMonitorActive = false;
    MediaMonitor *mon = MediaMonitor::GetMediaMonitor();
    if (mon && mon->IsActive())
    {
        m_mediaMonitorActive = true;
        mon->StopMonitoring();
    }
#endif

    // Set this to false so we can tell if the ripper has done anything
    // (i.e. we can tell if the user quit prior to ripping)
    m_somethingwasripped = false;
    m_decoder = NULL;
    m_tracks = new QVector<RipTrack*>;
}

Ripper::~Ripper(void)
{
    if (m_decoder)
        delete m_decoder;

#ifndef _WIN32
    // if the MediaMonitor was active when we started then restart it
    if (m_mediaMonitorActive)
    {
        MediaMonitor *mon = MediaMonitor::GetMediaMonitor();
        if (mon)
            mon->StartMonitoring();
    }
#endif

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
        VERBOSE(VB_IMPORTANT, "Missing theme elements for screen 'cdripper'");
        return false;
    }

    connect(m_trackList, SIGNAL(itemClicked(MythUIButtonListItem *)),
            SLOT(toggleTrackActive(MythUIButtonListItem *)));
    connect(m_ripButton, SIGNAL(Clicked()), SLOT(startRipper()));
    connect(m_scanButton, SIGNAL(Clicked()), SLOT(startScanCD()));
    connect(m_switchTitleArtist, SIGNAL(Clicked()),
            SLOT(switchTitlesAndArtists()));
    connect(m_compilationCheck, SIGNAL(toggled(bool)),
            SLOT(compilationChanged(bool)));
    connect(m_searchGenreButton, SIGNAL(Clicked()), SLOT(searchGenre()));
    connect(m_genreEdit, SIGNAL(valueChanged()), SLOT(genreChanged()));
    m_yearEdit->SetFilter((InputFilter)(FilterAlpha | FilterSymbols | FilterPunct));
    m_yearEdit->SetMaxLength(4);
    connect(m_yearEdit, SIGNAL(valueChanged()), SLOT(yearChanged()));
    connect(m_artistEdit, SIGNAL(valueChanged()), SLOT(artistChanged()));
    connect(m_searchArtistButton, SIGNAL(Clicked()), SLOT(searchArtist()));
    connect(m_albumEdit, SIGNAL(valueChanged()), SLOT(albumChanged()));
    connect(m_searchAlbumButton, SIGNAL(Clicked()), SLOT(searchAlbum()));

    // Populate Quality List
    new MythUIButtonListItem(m_qualityList, tr("Low"), qVariantFromValue(0));
    new MythUIButtonListItem(m_qualityList, tr("Medium"), qVariantFromValue(1));
    new MythUIButtonListItem(m_qualityList, tr("High"), qVariantFromValue(2));
    new MythUIButtonListItem(m_qualityList, tr("Perfect"), qVariantFromValue(3));
    m_qualityList->SetValueByData(qVariantFromValue(
                        gCoreContext->GetNumSetting("DefaultRipQuality", 1)));

    QTimer::singleShot(500, this, SLOT(startScanCD()));

    return true;
}

bool Ripper::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget() && GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("Global", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "INFO")
        {
            showEditMetadataDialog(m_trackList->GetItemCurrent());
        }
        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void Ripper::startScanCD(void)
{
    if (m_scanThread)
        return;

    QString message = QObject::tr("Scanning CD. Please Wait ...");
    OpenBusyPopup(message);

    m_scanThread = new CDScannerThread(this);
    connect(m_scanThread, SIGNAL(finished()), SLOT(ScanFinished()));
    m_scanThread->start();
}

void Ripper::ScanFinished()
{
    delete m_scanThread;
    m_scanThread = NULL;

    m_tracks->clear();

    bool isCompilation = false;
    bool newTune = true;
    if (m_decoder)
    {
        QString label;
        Metadata *metadata;

        m_artistName.clear();
        m_albumName.clear();
        m_genreName.clear();
        m_year.clear();
        bool yesToAll = false;
        bool noToAll = false;

        for (int trackno = 0; trackno < m_decoder->getNumTracks(); trackno++)
        {
            RipTrack *ripTrack = new RipTrack;

            metadata = m_decoder->getMetadata(trackno + 1);
            if (metadata)
            {
                ripTrack->metadata = metadata;
                ripTrack->length = metadata->Length();
                ripTrack->active = true;

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
                newTune = Ripper::isNewTune(m_artistName, m_albumName, title);

                if (newTune)
                {
                    m_tracks->push_back(ripTrack);
                }
                else
                {
                    if (yesToAll)
                    {
                        deleteTrack(m_artistName, m_albumName, title);
                        m_tracks->push_back(ripTrack);
                    }
                    else if (noToAll)
                    {
                        delete ripTrack;
                        delete metadata;
                        continue;
                    }
                    else
                    {
                        DialogBox *dlg = new DialogBox(
                            GetMythMainWindow(),
                            tr("Artist: %1\n"
                               "Album: %2\n"
                               "Track: %3\n\n"
                               "This track is already in the database. \n"
                               "Do you want to remove the existing track?")
                            .arg(m_artistName).arg(m_albumName).arg(title));

                        dlg->AddButton("No");
                        dlg->AddButton("No To All");
                        dlg->AddButton("Yes");
                        dlg->AddButton("Yes To All");
                        DialogCode res = dlg->exec();
                        dlg->deleteLater();
                        dlg = NULL;

                        if (kDialogCodeButton0 == res)
                        {
                            delete ripTrack;
                            delete metadata;
                        }
                        else if (kDialogCodeButton1 == res)
                        {
                            noToAll = true;
                            delete ripTrack;
                            delete metadata;
                        }
                        else if (kDialogCodeButton2 == res)
                        {
                            deleteTrack(m_artistName, m_albumName, title);
                            m_tracks->push_back(ripTrack);
                        }
                        else if (kDialogCodeButton3 == res)
                        {
                            yesToAll = true;
                            deleteTrack(m_artistName, m_albumName, title);
                            m_tracks->push_back(ripTrack);
                        }
                        else // treat cancel as no
                        {
                            delete ripTrack;
                            delete metadata;
                        }
                    }
                }
            }
            else
                delete ripTrack;
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
#ifdef HAVE_CDAUDIO
    QByteArray devname = m_CDdevice.toAscii();
    int cdrom_fd = cd_init_device(const_cast<char*>(devname.constData()));
    VERBOSE(VB_MEDIA, "Ripper::scanCD() - dev:" + m_CDdevice);
    if (cdrom_fd == -1)
    {
        perror("Could not open cdrom_fd");
        return;
    }
    cd_close(cdrom_fd);  //Close the CD tray
    cd_finish(cdrom_fd);
#endif

    if (m_decoder)
        delete m_decoder;

    m_decoder = new CdDecoder("cda", NULL, NULL, NULL);
    if (m_decoder)
        m_decoder->setDevice(m_CDdevice);
}

/**
 * determine if there is already a similar track in the database
 */
bool Ripper::isNewTune(const QString& artist,
                       const QString& album, const QString& title)
{

    QString matchartist = artist;
    QString matchalbum = album;
    QString matchtitle = title;

    if (! matchartist.isEmpty())
    {
        matchartist.replace(QRegExp("(/|\\\\|:|\'|\\,|\\!|\\(|\\)|\"|\\?|\\|)"), QString("_"));
    }

    if (! matchalbum.isEmpty())
    {
        matchalbum.replace(QRegExp("(/|\\\\|:|\'|\\,|\\!|\\(|\\)|\"|\\?|\\|)"), QString("_"));
    }

    if (! matchtitle.isEmpty())
    {
        matchtitle.replace(QRegExp("(/|\\\\|:|\'|\\,|\\!|\\(|\\)|\"|\\?|\\|)"), QString("_"));
    }

    MSqlQuery query(MSqlQuery::InitCon());
    QString queryString("SELECT filename, artist_name,"
                        " album_name, name, song_id "
                        "FROM music_songs "
                        "LEFT JOIN music_artists"
                        " ON music_songs.artist_id=music_artists.artist_id "
                        "LEFT JOIN music_albums"
                        " ON music_songs.album_id=music_albums.album_id "
                        "WHERE artist_name LIKE :ARTIST "
                        "AND album_name LIKE :ALBUM "
                        "AND name LIKE :TITLE "
                        "ORDER BY artist_name, album_name,"
                        " name, song_id, filename");

    query.prepare(queryString);

    query.bindValue(":ARTIST", matchartist);
    query.bindValue(":ALBUM", matchalbum);
    query.bindValue(":TITLE", matchtitle);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("Search music database", query);
        return true;
    }

    if (query.size() > 0)
        return false;

    return true;
}

void Ripper::deleteTrack(QString& artist, QString& album, QString& title)
{
    MSqlQuery query(MSqlQuery::InitCon());
    QString queryString("SELECT song_id, filename "
            "FROM music_songs "
            "LEFT JOIN music_artists"
            " ON music_songs.artist_id=music_artists.artist_id "
            "LEFT JOIN music_albums"
            " ON music_songs.album_id=music_albums.album_id "
            "WHERE artist_name REGEXP \'");
    QString token = artist;
    token.replace(QRegExp("(/|\\\\|:|\'|\\,|\\!|\\(|\\)|\"|\\?|\\|)"),
                  QString("."));

    queryString += token + "\' AND " + "album_name REGEXP \'";
    token = album;
    token.replace(QRegExp("(/|\\\\|:|\'|\\,|\\!|\\(|\\)|\"|\\?|\\|)"),
                  QString("."));
    queryString += token + "\' AND " + "name    REGEXP \'";
    token = title;
    token.replace(QRegExp("(/|\\\\|:|\'|\\,|\\!|\\(|\\)|\"|\\?|\\|)"),
                  QString("."));
    queryString += token + "\' ORDER BY artist_name, album_name,"
                           " name, song_id, filename";
    query.prepare(queryString);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("Search music database", query);
        return;
    }

    while (query.next())
    {
        int trackID = query.value(0).toInt();
        QString filename = query.value(1).toString();

        // delete file
        QString musicdir = gCoreContext->GetSetting("MusicLocation");
        musicdir = QDir::cleanPath(musicdir);
        if (!musicdir.endsWith("/"))
            musicdir += "/";
        QFile::remove(musicdir + filename);

        // remove database entry
        MSqlQuery deleteQuery(MSqlQuery::InitCon());
        deleteQuery.prepare("DELETE FROM music_songs"
                            " WHERE song_id = :SONG_ID");
        deleteQuery.bindValue(":SONG_ID", trackID);
        if (!deleteQuery.exec())
            MythDB::DBError("Delete Track", deleteQuery);
    }
}

// static function to create a filename based on the metadata information
// if createDir is true then the directory structure will be created
QString Ripper::filenameFromMetadata(Metadata *track, bool createDir)
{
    QString musicdir = gCoreContext->GetSetting("MusicLocation");
    musicdir = QDir::cleanPath(musicdir);
    if (!musicdir.endsWith("/"))
        musicdir += "/";

    QDir directoryQD(musicdir);
    QString filename;
    QString fntempl = gCoreContext->GetSetting("FilenameTemplate");
    bool no_ws = gCoreContext->GetNumSetting("NoWhitespace", 0);

    QRegExp rx_ws("\\s{1,}");
    QRegExp rx("(GENRE|ARTIST|ALBUM|TRACK|TITLE|YEAR)");
    int i = 0;
    int old_i = 0;
    while (i >= 0)
    {
        i = rx.indexIn(fntempl, i);
        if (i >= 0)
        {
            if (i > 0)
                filename += fixFileToken_sl(fntempl.mid(old_i,i-old_i));
            i += rx.matchedLength();
            old_i = i;

            if ((rx.capturedTexts()[1] == "GENRE") && (!track->Genre().isEmpty()))
                filename += fixFileToken(track->Genre());

            if ((rx.capturedTexts()[1] == "ARTIST")
                    && (!track->FormatArtist().isEmpty()))
                filename += fixFileToken(track->FormatArtist());

            if ((rx.capturedTexts()[1] == "ALBUM") && (!track->Album().isEmpty()))
                filename += fixFileToken(track->Album());

            if ((rx.capturedTexts()[1] == "TRACK") && (track->Track() >= 0))
            {
                QString tempstr = QString::number(track->Track(), 10);
                if (track->Track() < 10)
                    tempstr.prepend('0');
                filename += fixFileToken(tempstr);
            }

            if ((rx.capturedTexts()[1] == "TITLE")
                    && (!track->FormatTitle().isEmpty()))
                filename += fixFileToken(track->FormatTitle());

            if ((rx.capturedTexts()[1] == "YEAR") && (track->Year() >= 0))
                filename += fixFileToken(QString::number(track->Year(), 10));
        }
    }

    if (no_ws)
        filename.replace(rx_ws, "_");

    if (filename == musicdir || filename.length() > FILENAME_MAX)
    {
        QString tempstr = QString::number(track->Track(), 10);
        tempstr += " - " + track->FormatTitle();
        filename = musicdir + fixFileToken(tempstr);
        VERBOSE(VB_GENERAL, QString("Invalid file storage definition."));
    }

    QStringList directoryList = filename.split("/");
    for (int i = 0; i < (directoryList.size() - 1); i++)
    {
        musicdir += "/" + directoryList[i];
        if (createDir)
        {
            umask(022);
            directoryQD.mkdir(musicdir);
            directoryQD.cd(musicdir);
        }
    }

    filename = QDir::cleanPath(musicdir) + "/" + directoryList.last();

    return filename;
}

inline QString Ripper::fixFileToken(QString token)
{
    token.replace(QRegExp("(/|\\\\|:|\'|\"|\\?|\\|)"), QString("_"));
    return token;
}

inline QString Ripper::fixFileToken_sl(QString token)
{
    // this version doesn't remove fwd-slashes so we can
    // pick them up later and create directories as required
    token.replace(QRegExp("(\\\\|:|\'|\"|\\?|\\|)"), QString("_"));
    return token;
}


bool Ripper::somethingWasRipped()
{
    return m_somethingwasripped;
}

void Ripper::artistChanged()
{
    QString newartist = m_artistEdit->GetText();
    Metadata *data;

    if (m_tracks->size() > 0)
    {
        for (int trackno = 0; trackno < m_tracks->size(); ++trackno)
        {
            data = m_tracks->at(trackno)->metadata;

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
    Metadata *data;

    if (m_tracks->size() > 0)
    {
        for (int trackno = 0; trackno < m_tracks->size(); ++trackno)
        {
            data = m_tracks->at(trackno)->metadata;

            if (data)
                data->setAlbum(newalbum);
        }
    }

    m_albumName = newalbum;
}

void Ripper::genreChanged()
{
    QString newgenre = m_genreEdit->GetText();
    Metadata *data;

    if (m_tracks->size() > 0)
    {
        for (int trackno = 0; trackno < m_tracks->size(); ++trackno)
        {
            data = m_tracks->at(trackno)->metadata;

            if (data)
                data->setGenre(newgenre);
        }
    }

    m_genreName = newgenre;
}

void Ripper::yearChanged()
{
    QString newyear = m_yearEdit->GetText();

    Metadata *data;

    if (m_tracks->size() > 0)
    {
        for (int trackno = 0; trackno < m_tracks->size(); ++trackno)
        {
            data = m_tracks->at(trackno)->metadata;

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
        Metadata *data;
        if (m_tracks->size() > 0)
        {
            // Update artist MetaData of each track on the ablum...
            for (int trackno = 0; trackno < m_tracks->size(); ++trackno)
            {
                data = m_tracks->at(trackno)->metadata;

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
        if (m_tracks->size() > 0)
        {
            // Update artist MetaData of each track on the album...
            for (int trackno = 0; trackno < m_tracks->size(); ++trackno)
            {
                Metadata *data;
                data = m_tracks->at(trackno)->metadata;

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

    Metadata *data;

    // Switch title and artist for each track
    QString tmp;
    if (m_tracks->size() > 0)
    {
        for (int track = 0; track < m_tracks->size(); ++track)
        {
            data = m_tracks->at(track)->metadata;

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
    if (m_tracks->size() == 0)
    {
        ShowOkPopup(tr("There are no tracks to rip?"));
        return;
    }

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    int quality = m_qualityList->GetItemCurrent()->GetData().toInt();

    RipStatus *statusDialog = new RipStatus(mainStack, m_CDdevice, m_tracks,
                                            quality);

    if (statusDialog->Create())
    {
        connect(statusDialog, SIGNAL(Result(bool)), SLOT(RipComplete(bool)));
        mainStack->AddScreen(statusDialog);
    }
    else
        delete statusDialog;
}

void Ripper::RipComplete(bool result)
{
    if (result == true)
    {
        bool EjectCD = gCoreContext->GetNumSetting("EjectCDAfterRipping", 1);
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
    connect(m_ejectThread, SIGNAL(finished()), SLOT(EjectFinished()));
    m_ejectThread->start();
}

void Ripper::EjectFinished()
{
    delete m_ejectThread;
    m_ejectThread = NULL;

    CloseBusyPopup();
}

void Ripper::ejectCD()
{
    bool bEjectCD = gCoreContext->GetNumSetting("EjectCDAfterRipping",1);
    if (bEjectCD)
    {
#ifdef HAVE_CDAUDIO
        QByteArray devname = m_CDdevice.toAscii();
        int cdrom_fd = cd_init_device(const_cast<char*>(devname.constData()));
        VERBOSE(VB_MEDIA, "Ripper::ejectCD() - dev " + m_CDdevice);
        if (cdrom_fd != -1)
        {
            if (cd_eject(cdrom_fd) == -1)
                perror("Failed on cd_eject");

            cd_finish(cdrom_fd);
        }
        else
            perror("Failed on cd_init_device");
#else
        MediaMonitor *mon = MediaMonitor::GetMediaMonitor();
        if (mon)
        {
            QByteArray devname = m_CDdevice.toAscii();
            MythMediaDevice *pMedia = mon->GetMedia(devname.constData());
            if (pMedia && mon->ValidateAndLock(pMedia))
            {
                pMedia->eject();
                mon->Unlock(pMedia);
            }
        }
#endif
    }
}

void Ripper::updateTrackList(void)
{
    if (m_tracks->size() == 0)
        return;

    QString tmptitle;
    if (m_trackList)
    {
        m_trackList->Reset();

        int i;
        for (i = 0; i < (int)m_tracks->size(); i++)
        {
            if (i >= m_tracks->size())
                break;

            RipTrack *track = m_tracks->at(i);
            Metadata *metadata = track->metadata;

            MythUIButtonListItem *item = new MythUIButtonListItem(m_trackList,"");

            item->setCheckable(true);

            item->SetData(qVariantFromValue(metadata));

            if (track->active)
                item->setChecked(MythUIButtonListItem::FullChecked);
            else
                item->setChecked(MythUIButtonListItem::NotChecked);

            item->SetText(QString::number(metadata->Track()), "track");
            item->SetText(metadata->Title(), "title");
            item->SetText(metadata->Artist(), "artist");

            int length = track->length / 1000;
            if (length > 0)
            {
                int min, sec;
                min = length / 60;
                sec = length % 60;
                QString s;
                s.sprintf("%02d:%02d", min, sec);
                item->SetText(s, "length");
            }
            else
                item->SetText("", "length");

//             if (i == m_currentTrack)
//                 m_trackList->SetItemCurrent(i);
        }
    }
}

void Ripper::searchArtist()
{
    QString s;

    m_searchList = Metadata::fillFieldList("artist");

    s = m_artistEdit->GetText();
    if (showList(tr("Select an Artist"), s))
    {
        m_artistEdit->SetText(s);
    }
}

void Ripper::searchAlbum()
{
    QString s;

    m_searchList = Metadata::fillFieldList("album");

    s = m_albumEdit->GetText();
    if (showList(tr("Select an Album"), s))
    {
        m_albumEdit->SetText(s);
    }
}

void Ripper::searchGenre()
{
    QString s;

    // load genre list
    m_searchList.clear();
    for (int x = 0; x < genre_table_size; x++)
        m_searchList.push_back(QString(genre_table[x]));
    m_searchList.sort();

    s = m_genreEdit->GetText();
    if (showList(tr("Select a Genre"), s))
    {
        m_genreEdit->SetText(s);
    }
}

bool Ripper::showList(QString caption, QString &value)
{
    bool res = false;

    MythSearchDialog *searchDialog
        = new MythSearchDialog(GetMythMainWindow(), "");
    searchDialog->setCaption(caption);
    searchDialog->setSearchText(value);
    searchDialog->setItems(m_searchList);
    DialogCode rescode = searchDialog->ExecPopupAtXY(-1, 8);
    if (kDialogCodeRejected != rescode)
    {
        value = searchDialog->getResult();
        res = true;
    }

    searchDialog->deleteLater();

    return res;
}

void Ripper::showEditMetadataDialog(MythUIButtonListItem *item)
{
    if (!item || m_tracks->size() == 0)
        return;

    Metadata *editMeta = qVariantValue<Metadata *>(item->GetData());

    EditMetadataDialog editDialog(editMeta, GetMythMainWindow(),
                                  "edit_metadata", "music-", "edit metadata");
    editDialog.setSaveMetadataOnly();

    if (kDialogCodeRejected != editDialog.exec())
    {
        updateTrackList();
    }
}

void Ripper::toggleTrackActive(MythUIButtonListItem *item)
{
    if (m_tracks->size() == 0 || !item)
        return;

    RipTrack *track = m_tracks->at(m_trackList->GetItemPos(item));

    track->active = !track->active;

    if (track->active)
        item->setChecked(MythUIButtonListItem::FullChecked);
    else
        item->setChecked(MythUIButtonListItem::NotChecked);

    updateTrackLengths();
}

void Ripper::updateTrackLengths()
{
    QVector<RipTrack*>::iterator it;
    RipTrack *track;
    int length = 0;

    for (it = m_tracks->end() - 1; it == m_tracks->begin(); --it)
    {
        track = *it;
        if (track->active)
        {
            track->length = length + track->metadata->Length();
            length = 0;
        }
        else
        {
            track->length = 0;
            length += track->metadata->Length();
        }
    }
}

///////////////////////////////////////////////////////////////////////////////

RipStatus::RipStatus(MythScreenStack *parent, const QString &device,
                     QVector<RipTrack*> *tracks, int quality)
    : MythScreenType(parent, "ripstatus")
{
    m_CDdevice = device;
    m_tracks = tracks;
    m_quality = quality;
    m_ripperThread = NULL;

    m_overallText = m_trackText = m_statusText = m_trackPctText =
    m_overallPctText = NULL;

    m_overallProgress = m_trackProgress = NULL;
}

RipStatus::~RipStatus(void)
{
    if (m_ripperThread)
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

    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("Global", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;


        if (action == "ESCAPE")
        {
            if (m_ripperThread && m_ripperThread->isRunning())
            {
                if (MythPopupBox::showOkCancelPopup(GetMythMainWindow(),
                    "Stop Rip?",
                    tr("Are you sure you want to cancel ripping the CD?"),
                    false))
                {
                    m_ripperThread->cancel();
                    m_ripperThread->wait();
                    Close();
                }
            }
        }
        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void RipStatus::customEvent(QEvent *event)
{
    RipStatusEvent *rse = (RipStatusEvent *)event;

    if (event->type() == RipStatusEvent::kTrackTextEvent)
    {
        if (m_trackText)
            m_trackText->SetText(rse->text);
    }
    else if (event->type() == RipStatusEvent::kOverallTextEvent)
    {
        if (m_overallText)
            m_overallText->SetText(rse->text);
    }
    else if (event->type() == RipStatusEvent::kStatusTextEvent)
    {
        if (m_statusText)
            m_statusText->SetText(rse->text);
    }
    else if (event->type() == RipStatusEvent::kTrackProgressEvent)
    {
        if (m_trackProgress)
            m_trackProgress->SetUsed(rse->value);
    }
    else if (event->type() == RipStatusEvent::kTrackPercentEvent)
    {
        if (m_trackPctText)
            m_trackPctText->SetText(QString("%1%").arg(rse->value));
    }
    else if (event->type() == RipStatusEvent::kTrackStartEvent)
    {
        if (m_trackProgress)
            m_trackProgress->SetTotal(rse->value);
    }
    else if (event->type() == RipStatusEvent::kOverallProgressEvent)
    {
        if (m_overallProgress)
            m_overallProgress->SetUsed(rse->value);
    }
    else if (event->type() == RipStatusEvent::kOverallStartEvent)
    {
        if (m_overallProgress)
            m_overallProgress->SetTotal(rse->value);
    }
    else if (event->type() == RipStatusEvent::kOverallPercentEvent)
    {
        if (m_overallPctText)
            m_overallPctText->SetText(QString("%1%").arg(rse->value));
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
        VERBOSE(VB_IMPORTANT, "Received an unknown event type!");
    }
}

void RipStatus::startRip(void)
{
    if (m_ripperThread)
        delete m_ripperThread;

    m_ripperThread = new CDRipperThread(this, m_CDdevice, m_tracks, m_quality);
    m_ripperThread->start();
}
