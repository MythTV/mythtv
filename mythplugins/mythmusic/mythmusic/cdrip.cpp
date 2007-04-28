// ANSI C includes
#include <cstdio>
#include <cstring>

// Unix C includes
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

// Linux C includes
#include <cdaudio.h>
extern "C" {
#include <cdda_interface.h>
#include <cdda_paranoia.h>
}

// C++ includes
#include <iostream>
using namespace std;

// Qt includes
#include <qapplication.h>
#include <qdir.h>
#include <qregexp.h>

// MythTV plugin includes
#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>
#include <mythtv/mythwidgets.h>
#include <mythtv/lcddevice.h>
#include <mythtv/dialogbox.h>
#include <mythtv/mythmediamonitor.h>

// MythMusic includes
#include "cdrip.h"
#include "cddecoder.h"
#include "encoder.h"
#include "vorbisencoder.h"
#include "lameencoder.h"
#include "flacencoder.h"
#include "genres.h"
#include "editmetadata.h"


CDScannerThread::CDScannerThread(Ripper *ripper)
{
    m_parent = ripper;
}

void CDScannerThread::run()
{
    m_parent->scanCD();
}

///////////////////////////////////////////////////////////////////////////////

CDEjectorThread::CDEjectorThread(Ripper *ripper)
{
    m_parent = ripper;
}

void CDEjectorThread::run()
{
    m_parent->ejectCD();
}

///////////////////////////////////////////////////////////////////////////////

static long int getSectorCount (QString &cddevice, int tracknum)
{
    cdrom_drive *device = cdda_identify(cddevice.ascii(), 0, NULL);

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
    return 0;
}

static void paranoia_cb(long inpos, int function)
{
    inpos = inpos; function = function;
}

CDRipperThread::CDRipperThread(RipStatus *parent,  QString device,
                               vector<Metadata*> *tracks, int quality)
{
    m_parent = parent;
    m_tracks = tracks;
    m_quality = quality;
    m_quit = false;
    m_totalTracks = m_tracks->size();
    m_CDdevice = device;
}

CDRipperThread::~CDRipperThread(void)
{
    cancel();
    wait();
}

void CDRipperThread::cancel(void)
{
    m_mutex.lock();
    m_quit = true;
    m_mutex.unlock();
}

bool CDRipperThread::isCancelled(void)
{
    bool res;
    m_mutex.lock();
    res = m_quit;
    m_mutex.unlock();
    return res;
}

void CDRipperThread::sendEvent(int eventType, const QString &value)
{
    StatusData *sd = new StatusData;
    sd->type  = eventType;
    sd->text = value;
    sd->value = 0;
    QApplication::postEvent(m_parent, new QCustomEvent(QEvent::User, sd));
}

void CDRipperThread::sendEvent(int eventType, int value)
{
    StatusData *sd = new StatusData;
    sd->type  = eventType;
    sd->text = "";
    sd->value = value;
    QApplication::postEvent(m_parent, new QCustomEvent(QEvent::User, sd));
}

void CDRipperThread::run(void)
{
    Metadata *track = m_tracks->at(0);
    QString tots;

    if (track->Compilation())
    {
        tots = track->CompilationArtist() + " ~ " + track->Album();
    }
    else
    {
        tots = track->Artist() + " ~ " + track->Album();
    }

    sendEvent(ST_OVERALL_TEXT, tots);
    sendEvent(ST_OVERALL_PROGRESS, 0);
    sendEvent(ST_TRACK_PROGRESS, 0);

    QString textstatus;
    QString encodertype = gContext->GetSetting("EncoderType");
    bool mp3usevbr = gContext->GetNumSetting("Mp3UseVBR", 0);

    m_totalSectors = 0;
    m_totalSectorsDone = 0;
    for (int trackno = 0; trackno < m_totalTracks; trackno++)
    {
        m_totalSectors += getSectorCount(m_CDdevice, trackno + 1);
    }

    sendEvent(ST_OVERALL_START, m_totalSectors);

    if (class LCD * lcd = LCD::Get()) 
    {
        QString lcd_tots = QObject::tr("Importing ") + tots;
        QPtrList<LCDTextItem> textItems;
        textItems.setAutoDelete(true);
        textItems.append(new LCDTextItem(1, ALIGN_CENTERED,
                                         lcd_tots, "Generic", false));
        lcd->switchToGeneric(&textItems);
    }

    for (int trackno = 0; trackno < m_totalTracks; trackno++)
    {
        if (isCancelled())
            return;

        sendEvent(ST_STATUS_TEXT, QString("Track %1 of %2")
                                  .arg(trackno + 1).arg(m_totalTracks));

        Encoder *encoder;

        sendEvent(ST_TRACK_PROGRESS, 0);

        track = m_tracks->at(trackno);

        if (track)
        {
            QString outfile;

            textstatus = track->Title();
            sendEvent(ST_TRACK_TEXT, textstatus);

            sendEvent(ST_TRACK_PROGRESS, 0);
            sendEvent(ST_TRACK_PERCENT, 0);

            outfile = Ripper::filenameFromMetadata(track);

            if (m_quality < 3)
            {
                if (encodertype == "mp3")
                {
                    outfile += ".mp3";
                    encoder = new LameEncoder(outfile, m_quality, track,
                                              mp3usevbr);
                }
                else // ogg
                {
                    outfile += ".ogg";
                    encoder = new VorbisEncoder(outfile, m_quality, track);
                }
            }
            else
            {
                outfile += ".flac";
                encoder = new FlacEncoder(outfile, m_quality, track);
            }

            if (!encoder->isValid()) 
            {
                delete encoder;
                break;
            }

            ripTrack(m_CDdevice, encoder, trackno + 1);

            if (isCancelled())
                return;

            delete encoder;

            // save the metadata to the DB
            track->setFilename(outfile);
            track->dumpToDatabase();
        }
    }

    QString PostRipCDScript = gContext->GetSetting("PostCDRipScript");

    if (!PostRipCDScript.isEmpty()) 
    {
        VERBOSE(VB_IMPORTANT,
                QString("PostCDRipScript: %1").arg(PostRipCDScript));
        pid_t child = fork();
        if (child < 0)
        {
            perror("fork");
        }
        else if (child == 0)
        {
            execl("/bin/sh", "sh", "-c", PostRipCDScript.ascii(), NULL);
            perror("exec");
            _exit(1);
        }
    }

    sendEvent(ST_FINISHED, "");
}

int CDRipperThread::ripTrack(QString &cddevice, Encoder *encoder, int tracknum)
{
    cdrom_drive *device = cdda_identify(cddevice.ascii(), 0, NULL);

    if (!device)
        return -1;

    if (cdda_open(device))
    {
        cdda_close(device);
        return -1;
    }

    cdda_verbose_set(device, CDDA_MESSAGE_FORGETIT, CDDA_MESSAGE_FORGETIT);
    long int start = cdda_track_firstsector(device, tracknum);
    long int end = cdda_track_lastsector(device, tracknum);

    cdrom_paranoia *paranoia = paranoia_init(device);
    if (gContext->GetSetting("ParanoiaLevel") == "full")
        paranoia_modeset(paranoia, PARANOIA_MODE_FULL | 
                PARANOIA_MODE_NEVERSKIP);
    else
        paranoia_modeset(paranoia, PARANOIA_MODE_OVERLAP);

    paranoia_seek(paranoia, start, SEEK_SET);

    long int curpos = start;
    int16_t *buffer;

    sendEvent(ST_TRACK_START, end - start + 1);
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
                sendEvent(ST_OVERALL_PERCENT, newOverallPct);
                sendEvent(ST_OVERALL_PROGRESS, m_totalSectorsDone + curpos - start);
            }

            int newTrackPct = (int) (100.0 / (double) ((double) (end - start + 1) /
                    (double) (curpos - start)));
            if (newTrackPct != m_lastTrackPct)
            {
                m_lastTrackPct = newTrackPct;
                sendEvent(ST_TRACK_PERCENT, newTrackPct);
                sendEvent(ST_TRACK_PROGRESS, curpos - start);
            }

            if (class LCD * lcd = LCD::Get()) 
            {
                float fProgress = (float)(m_totalSectorsDone + (curpos - start)) / m_totalSectors;
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
}

///////////////////////////////////////////////////////////////////////////////

Ripper::Ripper(QString device, MythMainWindow *parent, const char *name)
      : MythThemedDialog(parent, "cdripper", "music-", name, true)
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
    wireupTheme();
    m_decoder = NULL;
    m_tracks = new vector<Metadata*>;

    QTimer::singleShot(500, this, SLOT(startScanCD()));
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
}

void Ripper::wireupTheme(void)
{
    m_qualitySelector = getUISelectorType("quality_selector");
    if (m_qualitySelector)
    {
        m_qualitySelector->addItem(0, tr("Low"));
        m_qualitySelector->addItem(1, tr("Medium"));
        m_qualitySelector->addItem(2, tr("High"));
        m_qualitySelector->addItem(3, tr("Perfect"));
        m_qualitySelector->setToItem(gContext->GetNumSetting("DefaultRipQuality", 1));
    }

    m_artistEdit = getUIRemoteEditType("artist_edit");
    if (m_artistEdit)
    {
        m_artistEdit->createEdit(this);
        connect(m_artistEdit, SIGNAL(textChanged(QString)), this, SLOT(artistChanged(QString)));
    }

    m_searchArtistButton = getUIPushButtonType("searchartist_button");
    if (m_searchArtistButton)
    {
        connect(m_searchArtistButton, SIGNAL(pushed()), this, SLOT(searchArtist()));
    }

    m_albumEdit = getUIRemoteEditType("album_edit");
    if (m_albumEdit)
    {
        m_albumEdit->createEdit(this);
        connect(m_albumEdit, SIGNAL(textChanged(QString)), this, SLOT(albumChanged(QString)));
    }

    m_searchAlbumButton = getUIPushButtonType("searchalbum_button");
    if (m_searchAlbumButton)
    {
        connect(m_searchAlbumButton, SIGNAL(pushed()), this, SLOT(searchAlbum()));
    }

    m_genreEdit = getUIRemoteEditType("genre_edit");
    if (m_genreEdit)
    {
        m_genreEdit->createEdit(this);
        connect(m_genreEdit, SIGNAL(textChanged(QString)), this, SLOT(genreChanged(QString)));
    }

    m_yearEdit = getUIRemoteEditType("year_edit");
    if (m_yearEdit)
    {
        m_yearEdit->createEdit(this);
        connect(m_yearEdit, SIGNAL(textChanged(QString)), this, SLOT(yearChanged(QString)));
    }

    m_searchGenreButton = getUIPushButtonType("searchgenre_button");
    if (m_searchGenreButton)
    {
        connect(m_searchGenreButton, SIGNAL(pushed()), this, SLOT(searchGenre()));
    }

    m_compilation = getUICheckBoxType("compilation_check");
    if (m_compilation)
    {
        connect(m_compilation, SIGNAL(pushed(bool)),
                this, SLOT(compilationChanged(bool)));
    }

    m_switchTitleArtist = getUITextButtonType("switch_text");
    if (m_switchTitleArtist)
    {
        m_switchTitleArtist->setText(tr("Switch Titles"));
        connect(m_switchTitleArtist, SIGNAL(pushed()), this, 
                SLOT(switchTitlesAndArtists()));
    }

    m_scanButton = getUITextButtonType("scan_button");
    if (m_scanButton)
    {
        m_scanButton->setText(tr("Scan CD"));
        connect(m_scanButton, SIGNAL(pushed()), this, 
                SLOT(startScanCD()));
    }

    m_ripButton = getUITextButtonType("rip_button");
    if (m_ripButton)
    {
        m_ripButton->setText(tr("Rip CD"));
        connect(m_ripButton, SIGNAL(pushed()), this, 
                SLOT(startRipper()));
    }

    m_switchTitleArtist = getUITextButtonType("switch_button");
    {
        m_switchTitleArtist->setText("Switch Titles");
        connect(m_switchTitleArtist, SIGNAL(pushed()), this, 
                SLOT(switchTitlesAndArtists()));
        m_switchTitleArtist->hide();
    }

    m_trackList = (UIListType*) getUIObject("track_list");

    buildFocusList();
    assignFirstFocus();
}

void Ripper::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;

    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Global", e, actions, true);

    for (unsigned int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "SELECT")
        {
            if (getCurrentFocusWidget() == m_trackList)
                showEditMetadataDialog();
            else
                activateCurrent();
        }
        else if (action == "ESCAPE")
        {
            reject();
        }
        else if (action == "UP")
        {
            if (getCurrentFocusWidget() == m_trackList)
            {
                trackListUp(false);
            }
            else
                nextPrevWidgetFocus(false);
        }
        else if (action == "DOWN")
        {
            if (getCurrentFocusWidget() == m_trackList)
            {
                trackListDown(false);
            }
            else
                nextPrevWidgetFocus(true);
        }
        else if (action == "LEFT")
        {
            if (getCurrentFocusWidget() == m_qualitySelector)
            {
                m_qualitySelector->push(false);
            }
            else
                nextPrevWidgetFocus(false);
        }
        else if (action == "RIGHT")
        {
            if (getCurrentFocusWidget() == m_qualitySelector)
            {
                m_qualitySelector->push(true);
            }
            else
                nextPrevWidgetFocus(true);
        }
        else if (action == "PAGEUP")
        {
            if (getCurrentFocusWidget() == m_trackList)
            {
                trackListUp(true);
            }
        }
        else if (action == "PAGEDOWN")
        {
            if (getCurrentFocusWidget() == m_trackList)
            {
                trackListDown(true);
            }
        }
        else if (action == "INFO")
        {
            showEditMetadataDialog();
        }
        else if (action == "1")
            m_scanButton->push();
        else if (action == "2")
            m_ripButton->push();
        else
            handled = false;
    }
}

void Ripper::startScanCD(void)
{
    MythBusyDialog *busy = new MythBusyDialog("Scanning CD. Please Wait ...");
    CDScannerThread *scanner = new CDScannerThread(this);
    busy->start();
    scanner->start();

    while (!scanner->finished())
    {
        usleep(500);
        qApp->processEvents();
    }

    delete scanner;

    m_tracks->clear();

    bool isCompilation = false;
    bool newTune = true;
    if (m_decoder)
    {
        QString label;
        Metadata *track;

        m_artistName = "";
        m_albumName = "";
        m_genreName = "";
        m_year = "";
        bool yesToAll = false;
        bool noToAll = false;

        for (int trackno = 0; trackno < m_decoder->getNumTracks(); trackno++)
        {
            track = m_decoder->getMetadata(trackno + 1);
            if (track)
            {
                if (track->Compilation())
                {
                    isCompilation = true;
                    m_artistName = track->CompilationArtist();
                }
                else if ("" == m_artistName)
                {
                    m_artistName = track->Artist();
                }

                if ("" == m_albumName)
                    m_albumName = track->Album();

                if ("" == m_genreName 
                    && "" != track->Genre())
                {
                    m_genreName = track->Genre();
                }

                if ("" == m_year 
                    && 0 != track->Year())
                {
                    m_year = QString::number(track->Year());
                }

                QString title = track->Title();
                newTune = Ripper::isNewTune(m_artistName, m_albumName, title);

                if (newTune)
                {
                    m_tracks->push_back(track);
                }
                else
                {
                    if (yesToAll)
                    {
                        deleteTrack(m_artistName, m_albumName, title);
                        m_tracks->push_back(track);
                    }
                    else if (noToAll)
                    {
                        continue;
                    }
                    else
                    {
                        DialogBox dialog(gContext->GetMainWindow(),
                                         tr("Artist: %1\n"
                                         "Album: %2\n"
                                         "Track: %3\n\n"
                                         "This track is already in the database. \n"
                                         "Do you want to remove the existing track?")
                                         .arg(m_artistName).arg(m_albumName).arg(title));
                        dialog.AddButton("No");
                        dialog.AddButton("No To All");
                        dialog.AddButton("Yes");
                        dialog.AddButton("Yes To All");
                        int res = dialog.exec();

                        if (res == 1)
                        {
                        }
                        else if (res == 2)
                        {
                            noToAll = true;
                        }
                        else if (res == 3)
                        {
                            deleteTrack(m_artistName, m_albumName, title);
                            m_tracks->push_back(track);
                        }
                        else if (res == 4)
                        {
                            yesToAll = true;
                            deleteTrack(m_artistName, m_albumName, title);
                            m_tracks->push_back(track);
                        }
                    }
                }
            }
        }

        m_artistEdit->setText(m_artistName);
        m_albumEdit->setText(m_albumName);
        m_genreEdit->setText(m_genreName);
        m_yearEdit->setText(m_year);
        m_compilation->setState(isCompilation);

        if (!isCompilation)
            m_switchTitleArtist->hide();
        else
            m_switchTitleArtist->show();

        m_totalTracks = m_tracks->size();
    }

    m_currentTrack = 0;

    buildFocusList();
    updateTrackList();

    delete busy;
}

void Ripper::scanCD(void)
{
    int cdrom_fd = cd_init_device((char*)m_CDdevice.ascii());
    VERBOSE(VB_MEDIA, "Ripper::scanCD() - dev:" + m_CDdevice);
    if (cdrom_fd == -1)
    {
        perror("Could not open cdrom_fd");
        return;
    }
    cd_close(cdrom_fd);  //Close the CD tray
    cd_finish(cdrom_fd);

    if (m_decoder)
        delete m_decoder;

    m_decoder = new CdDecoder("cda", NULL, NULL, NULL);
    if (m_decoder)
        m_decoder->setDevice(m_CDdevice);
}

// static function to determin if there is already a similar track in the database
bool Ripper::isNewTune(const QString& artist, const QString& album, const QString& title)
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
    QString queryString("SELECT filename, artist_name, album_name, name, song_id "
                        "FROM music_songs "
                        "LEFT JOIN music_artists ON music_songs.artist_id=music_artists.artist_id "
                        "LEFT JOIN music_albums ON music_songs.album_id=music_albums.album_id "
                        "WHERE artist_name LIKE :ARTIST "
                        "AND album_name LIKE :ALBUM "
                        "AND name LIKE :TITLE "
                        "ORDER BY artist_name, album_name, name, song_id, filename");

    query.prepare(queryString);

    query.bindValue(":ARTIST", matchartist);
    query.bindValue(":ALBUM", matchalbum);
    query.bindValue(":TITLE", matchtitle);

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("Search music database", query);
        return true;
    }

    if (query.numRowsAffected() > 0)
    {
        return false;
    }

    return true;
}

void Ripper::deleteTrack(QString& artist, QString& album, QString& title)
{
    MSqlQuery query(MSqlQuery::InitCon());
    QString queryString("SELECT song_id, filename "
            "FROM music_songs "
            "LEFT JOIN music_artists ON music_songs.artist_id=music_artists.artist_id "
            "LEFT JOIN music_albums ON music_songs.album_id=music_albums.album_id "
            "WHERE artist_name REGEXP \'");      
    QString token = artist;
    token.replace(QRegExp("(/|\\\\|:|\'|\\,|\\!|\\(|\\)|\"|\\?|\\|)"), QString("."));

    queryString += token + "\' AND " + "album_name REGEXP \'";
    token = album;
    token.replace(QRegExp("(/|\\\\|:|\'|\\,|\\!|\\(|\\)|\"|\\?|\\|)"), QString("."));
    queryString += token + "\' AND " + "name    REGEXP \'";
    token = title;
    token.replace(QRegExp("(/|\\\\|:|\'|\\,|\\!|\\(|\\)|\"|\\?|\\|)"), QString("."));
    queryString += token + "\' ORDER BY artist_name, album_name, name, song_id, filename";      
    query.prepare(queryString);

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("Search music database", query);
        return;
    }

    if (query.numRowsAffected() > 0)
    {
        while (query.next())
        {
            int trackID = query.value(0).toInt();
            QString filename = query.value(1).toString();

            // delete file
            QString musicdir = gContext->GetSetting("MusicLocation");
            musicdir = QDir::cleanDirPath(musicdir);
            if (!musicdir.endsWith("/"))
                musicdir += "/";
            QFile::remove(musicdir + filename);

            // remove database entry
            MSqlQuery deleteQuery(MSqlQuery::InitCon());
            deleteQuery.prepare("DELETE FROM music_songs WHERE song_id = :SONG_ID");
            deleteQuery.bindValue(":SONG_ID", trackID);
            if (!deleteQuery.exec())
                MythContext::DBError("Delete Track", deleteQuery);
        }
    }
}

// static function to create a filename based on the metadata information
// if createDir is true then the directory structure will be created
QString Ripper::filenameFromMetadata(Metadata *track, bool createDir)
{
    QString musicdir = gContext->GetSetting("MusicLocation");
    musicdir = QDir::cleanDirPath(musicdir);
    if (!musicdir.endsWith("/"))
        musicdir += "/";

    QDir directoryQD(musicdir);
    QString filename = "";
    QString fntempl = gContext->GetSetting("FilenameTemplate");
    bool no_ws = gContext->GetNumSetting("NoWhitespace", 0);

    QRegExp rx_ws("\\s{1,}");
    QRegExp rx("(GENRE|ARTIST|ALBUM|TRACK|TITLE|YEAR)");
    int i = 0;
    int old_i = 0;
    while (i >= 0)
    {
        i = rx.search(fntempl, i);
        if (i >= 0)
        {
            if (i > 0) 
                filename += fixFileToken_sl(fntempl.mid(old_i,i-old_i));
            i += rx.matchedLength();
            old_i = i;

            if ((rx.capturedTexts()[1] == "GENRE") && (track->Genre() != ""))
                filename += fixFileToken(track->Genre());

            if ((rx.capturedTexts()[1] == "ARTIST") && (track->FormatArtist() != ""))
                filename += fixFileToken(track->FormatArtist());

            if ((rx.capturedTexts()[1] == "ALBUM") && (track->Album() != ""))
                filename += fixFileToken(track->Album());

            if ((rx.capturedTexts()[1] == "TRACK") && (track->Track() >= 0))
            {
                QString tempstr = QString::number(track->Track(), 10); 
                if (track->Track() < 10) 
                    tempstr.prepend('0'); 
                filename += fixFileToken(tempstr);
            }

            if ((rx.capturedTexts()[1] == "TITLE") && (track->FormatTitle() != ""))
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

    filename = filename.local8Bit();

    QStringList directoryList = QStringList::split("/", filename);
    for (unsigned i = 0; i < (directoryList.size() - 1); i++)
    {
        musicdir += "/" + directoryList[i];
        if (createDir)
        {
            umask(022);
            directoryQD.mkdir(musicdir, true);
            directoryQD.cd(musicdir, true);
        }
    }

    filename = QDir::cleanDirPath(musicdir) + "/" + directoryList.last();

    return filename;
}

inline QString Ripper::fixFileToken(QString token)
{
    token.replace(QRegExp("(/|\\\\|:|\'|\"|\\?|\\|)"), QString("_"));
    return token;
}

inline QString Ripper::fixFileToken_sl(QString token)
{
    // this version doesn't remove fwd-slashes so we can pick them up later and create directories as required
    token.replace(QRegExp("(\\\\|:|\'|\"|\\?|\\|)"), QString("_"));
    return token;
}


bool Ripper::somethingWasRipped()
{
    return m_somethingwasripped;
}

void Ripper::artistChanged(QString newartist)
{
    Metadata *data;

    for (int trackno = 0; trackno < m_totalTracks; ++trackno)
    {
        data = m_tracks->at(trackno);

        if (data)
        {
            if (m_compilation->getState())
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
    m_artistName = newartist;
}

void Ripper::albumChanged(QString newalbum)
{
    Metadata *data;

    for (int trackno = 0; trackno < m_totalTracks; ++trackno)
    {
        data = m_tracks->at(trackno);

        if (data)
            data->setAlbum(newalbum);
    }

    m_albumName = newalbum;
}

void Ripper::genreChanged(QString newgenre)
{
    Metadata *data;

    for (int trackno = 0; trackno < m_totalTracks; ++trackno)
    {
        data = m_tracks->at(trackno);

        if (data)
            data->setGenre(newgenre);
    }

    m_genreName = newgenre;
} 

void Ripper::yearChanged(QString newyear)
{
    Metadata *data;

    for (int trackno = 0; trackno < m_totalTracks; ++trackno)
    {
        data = m_tracks->at(trackno);

        if (data)
            data->setYear(newyear.toInt());
    }

    m_year = newyear;
} 

void Ripper::compilationChanged(bool state)
{
    if (!state)
    {
        Metadata *data;

        // Update artist MetaData of each track on the ablum...
        for (int trackno = 0; trackno < m_totalTracks; ++trackno)
        {
            data = m_tracks->at(trackno);

            if (data)
            {
                data->setCompilationArtist("");
                data->setArtist(m_artistName);
                data->setCompilation(false);
            }
        }

        m_switchTitleArtist->hide();
    }
    else
    {
        // Update artist MetaData of each track on the album...
        for (int trackno = 0; trackno < m_totalTracks; ++trackno)
        {
            Metadata *data;
            data = m_tracks->at(trackno);

            if (data)
            {
                data->setCompilationArtist(m_artistName);
                data->setCompilation(true);
            }
        }

        m_switchTitleArtist->show();
    }

    buildFocusList();
    updateTrackList();
}

void Ripper::switchTitlesAndArtists()
{
    if (!m_compilation->getState())
        return;

    Metadata *data;

    // Switch title and artist for each track
    QString tmp;

    for (int track = 0; track < m_totalTracks; ++track)
    {
        data = m_tracks->at(track);

        if (data)
        {
            tmp = data->Artist();
            data->setArtist(data->Title());
            data->setTitle(tmp);
        }
    }

    updateTrackList();
}

void Ripper::reject() 
{
    if (!gContext->GetMainWindow()->IsExitingToMain())
        startEjectCD();

    done(Rejected);
}

void Ripper::startRipper(void)
{
    if (m_tracks->size() == 0)
    {
        MythPopupBox::showOkPopup(gContext->GetMainWindow(), tr("No tracks"),
                                  tr("There are no tracks to rip?"));
        return;
    }

    RipStatus statusDialog(m_tracks, m_qualitySelector->getCurrentInt(),
                           gContext->GetMainWindow(), "edit metadata");
    int res = statusDialog.exec();
    if (res == Accepted)
    {
        bool EjectCD = gContext->GetNumSetting("EjectCDAfterRipping", 1);
        if (EjectCD) 
            startEjectCD();

        MythPopupBox::showOkPopup(gContext->GetMainWindow(), tr("Success"),
                                  tr("Rip completed successfully."));
    }
}


void Ripper::startEjectCD()
{
    MythBusyDialog *busy = new MythBusyDialog(tr("Ejecting CD. Please Wait ..."));
    CDEjectorThread *ejector = new CDEjectorThread(this);
    busy->start();
    ejector->start();

    while (!ejector->finished())
    {
        usleep(500);
        qApp->processEvents();
    }

    delete ejector;
    delete busy;
}

void Ripper::ejectCD()
{
    bool bEjectCD = gContext->GetNumSetting("EjectCDAfterRipping",1);
    if (bEjectCD)
    {
        int cdrom_fd;
        cdrom_fd = cd_init_device((char*)m_CDdevice.ascii());
        VERBOSE(VB_MEDIA, "Ripper::ejectCD() - dev " + m_CDdevice);
        if (cdrom_fd != -1)
        {
            if (cd_eject(cdrom_fd) == -1) 
                perror("Failed on cd_eject");

            cd_finish(cdrom_fd);
        }
        else
            perror("Failed on cd_init_device");
    }
}

void Ripper::updateTrackList(void)
{
    QString tmptitle;
    if (m_trackList)
    {
        int trackListSize = m_trackList->GetItems();
        m_trackList->ResetList();
        if (m_trackList->isFocused())
            m_trackList->SetActive(true);

        int skip;
        if (m_totalTracks <= trackListSize || m_currentTrack <= trackListSize / 2)
            skip = 0;
        else if (m_currentTrack >= m_totalTracks - trackListSize + trackListSize / 2)
            skip = m_totalTracks - trackListSize;
        else
            skip = m_currentTrack - trackListSize / 2;
        m_trackList->SetUpArrow(skip > 0);
        m_trackList->SetDownArrow(skip + trackListSize < m_totalTracks);

        int i;
        for (i = 0; i < trackListSize; i++)
        {
            if (i + skip >= m_totalTracks)
                break;

            Metadata *track = m_tracks->at(i + skip);

            m_trackList->SetItemText(i, 1, QString::number(track->Track()));
            m_trackList->SetItemText(i, 2, track->Title());
            m_trackList->SetItemText(i, 3, track->Artist());
            int length = track->Length() / 1000;
            int min, sec;
            min = length / 60;
            sec = length % 60;
            QString s;
            s.sprintf("%02d:%02d", min, sec);
            m_trackList->SetItemText(i, 4, s);

            if (i + skip == m_currentTrack)
            {
                m_trackList->SetItemCurrent(i);
            }
        }

        m_trackList->refresh();
    }
}

void Ripper::trackListDown(bool page)
{
    if (m_currentTrack < m_totalTracks - 1)
    {
        int trackListSize = m_trackList->GetItems();

        m_currentTrack += (page ? trackListSize : 1);
        if (m_currentTrack > m_totalTracks - 1)
            m_currentTrack = m_totalTracks - 1;

        updateTrackList();
    }
}

void Ripper::trackListUp(bool page)
{
    if (m_currentTrack > 0)
    {
        int trackListSize = m_trackList->GetItems();

        m_currentTrack -= (page ? trackListSize : 1);
        if (m_currentTrack < 0)
            m_currentTrack = 0;

        updateTrackList();
    }
}

void Ripper::searchArtist()
{
    QString s;

    m_searchList = Metadata::fillFieldList("artist");

    s = m_artistEdit->getText();
    if (showList(tr("Select an Artist"), s))
    {
        m_artistEdit->setText(s);
        artistChanged(s);
        updateTrackList();
    }
}

void Ripper::searchAlbum()
{
    QString s;

    m_searchList = Metadata::fillFieldList("album");

    s = m_albumEdit->getText();
    if (showList(tr("Select an Album"), s))
    {
        m_albumEdit->setText(s);
        albumChanged(s);
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

    s = m_genreEdit->getText();
    if (showList(tr("Select a Genre"), s))
    {
        m_genreEdit->setText(s);
        genreChanged(s);
    }
}

bool Ripper::showList(QString caption, QString &value)
{
    bool res = false;

    MythSearchDialog *searchDialog = new MythSearchDialog(gContext->GetMainWindow(), "");
    searchDialog->setCaption(caption);
    searchDialog->setSearchText(value);
    searchDialog->setItems(m_searchList);
    if (searchDialog->ExecPopupAtXY(-1, 8) == 0)
    {
        value = searchDialog->getResult();
        res = true;
    }

    delete searchDialog;
    setActiveWindow();

    return res;
}

void Ripper::showEditMetadataDialog()
{
    Metadata *editMeta = m_tracks->at(m_currentTrack);

    EditMetadataDialog editDialog(editMeta, gContext->GetMainWindow(),
                                  "edit_metadata", "music-", "edit metadata");
    editDialog.setSaveMetadataOnly();

    if (editDialog.exec())
    {
        updateTrackList();
    }
}

///////////////////////////////////////////////////////////////////////////////

RipStatus::RipStatus(vector<Metadata*> *tracks, int quality, MythMainWindow *parent, const char *name)
    : MythThemedDialog(parent, "ripstatus", "music-", name, true)
{
    m_tracks = tracks;
    m_quality = quality;
    m_ripperThread = NULL;

    wireupTheme();
    QTimer::singleShot(500, this, SLOT(startRip(QString)));
}

RipStatus::~RipStatus(void)
{
    if (m_ripperThread)
        delete m_ripperThread;
}

void RipStatus::wireupTheme(void)
{
    m_overallText = getUITextType("overall_text");
    m_trackText = getUITextType("track_text");
    m_statusText = getUITextType("status_text");
    m_trackPctText = getUITextType("trackpct_text");
    m_overallPctText = getUITextType("overallpct_text");

    m_overallProgress = getUIStatusBarType("overall_progress");
    if (m_overallProgress)
    {
        m_overallProgress->SetUsed(0);
        m_overallProgress->SetTotal(1);
    }

    m_trackProgress = getUIStatusBarType("track_progress");
    if (m_trackProgress)
    {
        m_trackProgress->SetUsed(0);
        m_trackProgress->SetTotal(1);
    }

    buildFocusList();
    assignFirstFocus();
}

void RipStatus::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;

    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Global", e, actions, false);

    for (unsigned int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "ESCAPE")
        {
            if (m_ripperThread && m_ripperThread->running())
            {
                if (MythPopupBox::showOkCancelPopup(gContext->GetMainWindow(), tr("Stop Rip?"),
                    tr("Are you sure you want to cancel ripping the CD?"), false))
                {
                    m_ripperThread->cancel();
                    m_ripperThread->wait();
                    done(Rejected);
                }
            }
        }
        else
            handled = false;
    }
}

void RipStatus::customEvent(QCustomEvent *e)
{
    if (!e || (e->type() != QEvent::User))
        return;

    StatusData *sd = (StatusData*) (e->data());

    if (!sd) return;

    switch(sd->type)
    {
        case ST_TRACK_TEXT:
        {
            m_trackText->SetText(sd->text);
            break;
        }

        case ST_OVERALL_TEXT:
        {
            m_overallText->SetText(sd->text);
            break;
        }

        case ST_STATUS_TEXT:
        {
            m_statusText->SetText(sd->text);
            break;
        }

        case ST_TRACK_PROGRESS:
        {
            m_trackProgress->SetUsed(sd->value);
            break;
        }

        case ST_TRACK_PERCENT:
        {
            m_trackPctText->SetText(QString("%1%").arg(sd->value));
            break;
        }

        case ST_TRACK_START:
        {
            m_trackProgress->SetTotal(sd->value);
            break;
        }

        case ST_OVERALL_PROGRESS:
        {
            m_overallProgress->SetUsed(sd->value);
            break;
        }

        case ST_OVERALL_START:
        {
            m_overallProgress->SetTotal(sd->value);
            break;
        }

        case ST_OVERALL_PERCENT:
        {
            m_overallPctText->SetText(QString("%1%").arg(sd->value));
            break;
        }

        case ST_FINISHED:
        {
            done(Accepted);
            break;
        }

        default:
            cout << "Received an unknown event type!" << endl;
            break;
    }

    delete sd;
}

void RipStatus::startRip(QString device)
{
    if (m_ripperThread)
        delete m_ripperThread;

    m_ripperThread = new CDRipperThread(this, device, m_tracks, m_quality);
    m_ripperThread->start();
}
