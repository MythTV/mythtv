// mythtv
#include <mythcontext.h>
#include <lcddevice.h>

// mythui
#include <mythuitext.h>
#include <mythuiimage.h>
#include <mythuistatetype.h>
#include <mythuiprogressbar.h>
//#include "mythdialogbox.h"

// mythmusic
#include "miniplayer.h"
#include "musicplayer.h"
#include "decoder.h"

MiniPlayer::MiniPlayer(MythScreenStack *parent, MusicPlayer *parentPlayer)
            : MythScreenType(parent, "music_miniplayer")
{
    m_parentPlayer = parentPlayer;

    m_timeText = m_infoText = NULL;
    m_volText = NULL;
    m_ratingsState = NULL;
    m_coverImage = NULL;
    m_progressBar = NULL;

    m_displayTimer = new QTimer(this);
    m_displayTimer->setSingleShot(true);
    connect(m_displayTimer, SIGNAL(timeout()), this, SLOT(timerTimeout()));

    m_infoTimer = new QTimer(this);
    m_infoTimer->setSingleShot(true);
    connect(m_infoTimer, SIGNAL(timeout()), this, SLOT(showInfoTimeout()));

    m_showingInfo = false;
}

MiniPlayer::~MiniPlayer(void)
{
    gPlayer->removeListener(this);

    // Timers are deleted by Qt
    m_displayTimer->disconnect();
    m_displayTimer = NULL;

    m_infoTimer->disconnect();
    m_infoTimer = NULL;

    if (class LCD *lcd = LCD::Get())
        lcd->switchToTime ();
}

void MiniPlayer::timerTimeout(void)
{
    Close();
}

bool MiniPlayer::Create(void)
{
    if (!LoadWindowFromXML("music-ui.xml", "miniplayer", this))
        return false;

    m_timeText = dynamic_cast<MythUIText *> (GetChild("time"));
    m_infoText = dynamic_cast<MythUIText *> (GetChild("info"));
    m_volText = dynamic_cast<MythUIText *> (GetChild("volume"));
    m_ratingsState = dynamic_cast<MythUIStateType *> (GetChild("userratingstate"));
    m_coverImage = dynamic_cast<MythUIImage *> (GetChild("coverart"));
    m_progressBar = dynamic_cast<MythUIProgressBar *> (GetChild("progress"));

    if (m_volText && gPlayer->getOutput())
    {
        m_volFormat = m_volText->GetText();
        m_volText->SetText(QString(m_volFormat)
                .arg((int) gPlayer->getOutput()->GetCurrentVolume()));
    }

    gPlayer->addListener(this);

    if (gPlayer->getCurrentMetadata())
    {
        m_maxTime = gPlayer->getCurrentMetadata()->Length() / 1000;
        updateTrackInfo(gPlayer->getCurrentMetadata());

        if (!gPlayer->isPlaying())
        {
            QString time_string = getTimeString(m_maxTime, 0);

            if (m_timeText)
                m_timeText->SetText(time_string);
            if (m_infoText)
                m_infoText->SetText(tr("Stopped"));
        }
    }

    m_displayTimer->start(10000);

    BuildFocusList();

    return true;
}

bool MiniPlayer::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget() && GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("Music", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "SELECT")
        {
            if (m_displayTimer)
                m_displayTimer->stop();
        }
        else if (action == "NEXTTRACK")
            gPlayer->next();
        else if (action == "PREVTRACK")
            gPlayer->previous();
        else if (action == "FFWD")
            seekforward();
        else if (action == "RWND")
            seekback();
        else if (action == "PLAY")
        {
            if (gPlayer->isPlaying())
                return false;

            if (gPlayer->getOutput() && gPlayer->getOutput()->IsPaused())
            {
                gPlayer->pause();
                return false;
            }

            gPlayer->play();
        }
        else if (action == "PAUSE")
        {
            if (gPlayer->isPlaying())
                gPlayer->pause();
            else
            {
                if (gPlayer->isPlaying())
                    gPlayer->stop();

                if (gPlayer->getOutput() &&
                    gPlayer->getOutput()->IsPaused())
                {
                    gPlayer->pause();
                    return false;
                }

                gPlayer->play();
            }
        }
        else if (action == "STOP")
        {
            gPlayer->stop();

            QString time_string = getTimeString(m_maxTime, 0);

            if (m_timeText)
                m_timeText->SetText(time_string);
            if (m_infoText)
                m_infoText->SetText("");
        }
        else if (action == "VOLUMEDOWN")
        {
            if (gPlayer->getOutput())
            {
                gPlayer->getOutput()->AdjustCurrentVolume(-2);
                showVolume();
            }
        }
        else if (action == "VOLUMEUP")
        {
            if (gPlayer->getOutput())
            {
                gPlayer->getOutput()->AdjustCurrentVolume(2);
                showVolume();
            }
        }
        else if (action == "MUTE")
        {
            if (gPlayer->getOutput())
            {
                gPlayer->getOutput()->ToggleMute();

                if (m_infoText)
                {
                    m_showingInfo = true;
                    if (gPlayer->IsMuted())
                        m_infoText->SetText(tr("Mute: On"));
                    else
                        m_infoText->SetText(tr("Mute: Off"));

                    if (m_infoTimer)
                    {
                        m_infoTimer->setSingleShot(true);
                        m_infoTimer->start(5000);
                    }
                }

                if (m_volText)
                {
                    if (gPlayer->IsMuted())
                        m_volText->SetText(QString(m_volFormat).arg(0));
                    else
                        m_volText->SetText(QString(m_volFormat)
                                .arg((int) gPlayer->getOutput()->GetCurrentVolume()));
                }
            }
        }
        else if (action == "THMBUP")
            increaseRating();
        else if (action == "THMBDOWN")
            decreaseRating();
        else if (action == "SPEEDDOWN")
        {
            gPlayer->decSpeed();
            showSpeed();
        }
        else if (action == "SPEEDUP")
        {
            gPlayer->incSpeed();
            showSpeed();
        }
        else if (action == "1")
        {
            gPlayer->toggleShuffleMode();
            showShuffleMode();
        }
        else if (action == "2")
        {
            gPlayer->toggleRepeatMode();
            showRepeatMode();
        }
        else if (action == "MENU")
        {
            gPlayer->autoShowPlayer(!gPlayer->getAutoShowPlayer());
            showAutoMode();
        }
        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    // restart the display timer on any keypress if it is active
    if (m_displayTimer && m_displayTimer->isActive())
        m_displayTimer->start();

    return handled;
}

void MiniPlayer::customEvent(QEvent *event)
{
    if (!IsVisible())
        return;

    if (event->type() == OutputEvent::Playing)
    {
        if (gPlayer->getCurrentMetadata())
        {
            m_maxTime = gPlayer->getCurrentMetadata()->Length() / 1000;
            updateTrackInfo(gPlayer->getCurrentMetadata());
        }
    }
    else if (event->type() == OutputEvent::Buffering)
    {
    }
    else if (event->type() == OutputEvent::Paused)
    {
    }
    else if (event->type() == OutputEvent::Info)
    {
        OutputEvent *oe = (OutputEvent *) event;

        int rs;
        m_currTime = rs = oe->elapsedSeconds();

        QString time_string = getTimeString(rs, m_maxTime);

        QString info_string;

        //  Hack around for cd bitrates
        if (oe->bitrate() < 2000)
        {
            info_string.sprintf(
                "%d "+tr("kbps")+ "   %.1f "+ tr("kHz")+ "   %s "+ tr("ch"),
                oe->bitrate(), float(oe->frequency()) / 1000.0,
                oe->channels() > 1 ? "2" : "1");
        }
        else
        {
            info_string.sprintf("%.1f "+ tr("kHz")+ "   %s "+ tr("ch"),
                                float(oe->frequency()) / 1000.0,
                                oe->channels() > 1 ? "2" : "1");
        }

        if (m_timeText)
            m_timeText->SetText(time_string);
        if (m_infoText && !m_showingInfo)
            m_infoText->SetText(info_string);

        if (m_progressBar)
        {
            m_progressBar->SetTotal(m_maxTime);
            m_progressBar->SetUsed(m_currTime);
        }

        if (gPlayer->getCurrentMetadata())
        {
            if (class LCD *lcd = LCD::Get())
            {
                float percent_heard = (m_maxTime <= 0) ? 0.0 :
                    ((float)rs /
                     (float)gPlayer->getCurrentMetadata()->Length()) * 1000.0;

                QString lcd_time_string = time_string;

                // if the string is longer than the LCD width, remove all spaces
                if (time_string.length() > (int)lcd->getLCDWidth())
                    lcd_time_string.remove(' ');

                lcd->setMusicProgress(lcd_time_string, percent_heard);
            }
        }
    }
    else if (event->type() == OutputEvent::Error)
    {
    }
    else if (event->type() == DecoderEvent::Stopped)
    {
    }
    else if (event->type() == DecoderEvent::Finished)
    {
        if (gPlayer->getRepeatMode() == MusicPlayer::REPEAT_TRACK)
            gPlayer->play();
        else
            gPlayer->next();
    }
    else if (event->type() == DecoderEvent::Error)
    {
    }

    QObject::customEvent(event);
}

QString MiniPlayer::getTimeString(int exTime, int maxTime)
{
    QString time_string;

    int eh = exTime / 3600;
    int em = (exTime / 60) % 60;
    int es = exTime % 60;

    int maxh = maxTime / 3600;
    int maxm = (maxTime / 60) % 60;
    int maxs = maxTime % 60;

    if (maxTime <= 0)
    {
        if (eh > 0)
            time_string.sprintf("%d:%02d:%02d", eh, em, es);
        else
            time_string.sprintf("%02d:%02d", em, es);
    }
    else
    {
        if (maxh > 0)
            time_string.sprintf("%d:%02d:%02d / %02d:%02d:%02d", eh, em,
                    es, maxh, maxm, maxs);
        else
            time_string.sprintf("%02d:%02d / %02d:%02d", em, es, maxm,
                    maxs);
    }

    return time_string;
}

void MiniPlayer::updateTrackInfo(Metadata *mdata)
{
    MythUIText *titleText = dynamic_cast<MythUIText *> (GetChild("title"));
    MythUIText *artisttitleText = dynamic_cast<MythUIText *>
                                                    (GetChild("artisttitle"));
    MythUIText *artistText = dynamic_cast<MythUIText *> (GetChild("artist"));
    MythUIText *albumText = dynamic_cast<MythUIText *> (GetChild("album"));

    if (titleText)
        titleText->SetText(mdata->FormatTitle());
    if (artistText)
        artistText->SetText(mdata->FormatArtist());
    if (artisttitleText)
        artisttitleText->SetText(tr("%1  by  %2",
                                    "Music track 'title by artist'")
                                        .arg(mdata->FormatTitle())
                                        .arg(mdata->FormatArtist()));
    if (albumText)
        albumText->SetText(mdata->Album());
    if (m_ratingsState)
        m_ratingsState->DisplayState(QString("%1").arg(mdata->Rating()));

    if (m_coverImage)
    {
        QImage image = gPlayer->getCurrentMetadata()->getAlbumArt();
        if (!image.isNull())
        {
            MythImage *mimage = GetMythPainter()->GetFormatImage();
            mimage->Assign(image);
            m_coverImage->SetImage(mimage);
        }
        else
            m_coverImage->Reset();
    }

    // Set the Artist and Track on the LCD
    LCD *lcd = LCD::Get();
    if (lcd)
        lcd->switchToMusic(mdata->Artist(), mdata->Album(), mdata->Title());
}

void MiniPlayer::seekforward(void)
{
    int nextTime = m_currTime + 5;
    if (nextTime > m_maxTime)
        nextTime = m_maxTime;
    seek(nextTime);
}

void MiniPlayer::seekback(void)
{
    int nextTime = m_currTime - 5;
    if (nextTime < 0)
        nextTime = 0;
    seek(nextTime);
}

void MiniPlayer::seek(int pos)
{
    if (gPlayer->getOutput())
    {
        gPlayer->getOutput()->Reset();
        gPlayer->getOutput()->SetTimecode(pos*1000);

        if (gPlayer->getDecoder() && gPlayer->getDecoder()->isRunning())
        {
            gPlayer->getDecoder()->lock();
            gPlayer->getDecoder()->seek(pos);
            gPlayer->getDecoder()->unlock();
        }

        if (!gPlayer->isPlaying())
        {
            m_currTime = pos;
            if (m_timeText)
                m_timeText->SetText(getTimeString(pos, m_maxTime));

            if (class LCD *lcd = LCD::Get())
            {
                float percent_heard = m_maxTime <= 0 ? 0.0 : ((float)pos /
                                      (float)m_maxTime);

                QString lcd_time_string = getTimeString(pos, m_maxTime);

                // if the string is longer than the LCD width, remove all spaces
                if (lcd_time_string.length() > (int)lcd->getLCDWidth())
                    lcd_time_string.remove(' ');

                lcd->setMusicProgress(lcd_time_string, percent_heard);
            }
        }
    }
}

void MiniPlayer::increaseRating(void)
{
    Metadata *curMeta = gPlayer->getCurrentMetadata();

    if (!curMeta)
        return;

    if (m_ratingsState)
    {
        curMeta->incRating();
        curMeta->persist();
        m_ratingsState->DisplayState(QString("%1").arg(curMeta->Rating()));

        // if all_music is still in scope we need to keep that in sync
        if (gMusicData->all_music)
        {
            if (gPlayer->getCurrentNode())
            {
                Metadata *mdata = gMusicData->all_music->getMetadata(gPlayer->getCurrentNode()->getInt());
                if (mdata)
                    mdata->incRating();
            }
        }
    }
}

void MiniPlayer::decreaseRating(void)
{
    Metadata *curMeta = gPlayer->getCurrentMetadata();

    if (!curMeta)
        return;

    if (m_ratingsState)
    {
        curMeta->decRating();
        curMeta->persist();
        m_ratingsState->DisplayState(QString("%1").arg(curMeta->Rating()));

        // if all_music is still in scope we need to keep that in sync
        if (gMusicData->all_music)
        {
            if (gPlayer->getCurrentNode())
            {
                Metadata *mdata = gMusicData->all_music->getMetadata(gPlayer->getCurrentNode()->getInt());
                if (mdata)
                    mdata->decRating();
            }
        }
    }
}

void MiniPlayer::showInfoTimeout(void)
{
    m_showingInfo = false;
    LCD *lcd = LCD::Get();
    Metadata * mdata = gPlayer->getCurrentMetadata();

    // Set the Artist and Track on the LCD
    if (lcd && mdata)
        lcd->switchToMusic(mdata->Artist(), mdata->Album(), mdata->Title());
}

void MiniPlayer::showShuffleMode(void)
{
    if (m_infoText)
    {
        m_infoTimer->stop();
        QString msg = tr("Shuffle Mode: ");
        switch (gPlayer->getShuffleMode())
        {
            case MusicPlayer::SHUFFLE_INTELLIGENT:
                msg += tr("Smart");
                if (class LCD *lcd = LCD::Get())
                    lcd->setMusicShuffle(LCD::MUSIC_SHUFFLE_SMART);
                break;
            case MusicPlayer::SHUFFLE_RANDOM:
                msg += tr("Rand");
                if (class LCD *lcd = LCD::Get())
                    lcd->setMusicShuffle(LCD::MUSIC_SHUFFLE_RAND);
                break;
            case MusicPlayer::SHUFFLE_ALBUM:
                msg += tr("Album");
                if (class LCD *lcd = LCD::Get())
                    lcd->setMusicShuffle(LCD::MUSIC_SHUFFLE_ALBUM);
                break;
            case MusicPlayer::SHUFFLE_ARTIST:
                msg += tr("Artist");
                if (class LCD *lcd = LCD::Get())
                    lcd->setMusicShuffle(LCD::MUSIC_SHUFFLE_ARTIST);
                break;
            default:
                msg += tr("None");
                if (class LCD *lcd = LCD::Get())
                    lcd->setMusicShuffle(LCD::MUSIC_SHUFFLE_NONE);
                break;
        }

        m_showingInfo = true;
        m_infoText->SetText(msg);
        m_infoTimer->start(5000);
    }
}

void MiniPlayer::showRepeatMode(void)
{
    if (m_infoText)
    {
        m_infoTimer->stop();
        QString msg = tr("Repeat Mode: ");
        switch (gPlayer->getRepeatMode())
        {
            case MusicPlayer::REPEAT_ALL:
                msg += tr("All");
                if (class LCD *lcd = LCD::Get())
                    lcd->setMusicRepeat (LCD::MUSIC_REPEAT_ALL);
                break;
            case MusicPlayer::REPEAT_TRACK:
                msg += tr("Track");
                if (class LCD *lcd = LCD::Get())
                    lcd->setMusicRepeat (LCD::MUSIC_REPEAT_TRACK);
                break;
            default:
                msg += tr("None");
                if (class LCD *lcd = LCD::Get())
                    lcd->setMusicRepeat (LCD::MUSIC_REPEAT_NONE);
                break;
        }

        m_showingInfo = true;
        m_infoText->SetText(msg);
        m_infoTimer->start(5000);
    }
}

void MiniPlayer::showAutoMode(void)
{
    if (m_infoText)
    {
        m_infoTimer->stop();
        QString msg = tr("Auto Show Player: ");
        if (gPlayer->getAutoShowPlayer())
            msg += tr("On");
        else
            msg += tr("Off");

        m_showingInfo = true;
        m_infoText->SetText(msg);
        m_infoTimer->start(5000);
    }
}

void MiniPlayer::showVolume(void)
{
    float level = (float)gPlayer->getOutput()->GetCurrentVolume();
    bool muted = gPlayer->IsMuted();

    if (m_infoText)
    {
        m_infoTimer->stop();
        QString msg = tr("Volume: ");

        if (muted)
            msg += QString::number((int) level) + "% " + tr("(muted)");
        else
            msg += QString::number((int) level) + "%";

        m_showingInfo = true;
        m_infoText->SetText(msg);
        m_infoTimer->start(5000);
    }

    if (class LCD *lcd = LCD::Get())
    {
        if (muted)
        {
            lcd->switchToVolume("Music (muted)");
            lcd->setVolumeLevel(level / (float)100);
        }
        else
        {
            lcd->switchToVolume("Music");
            lcd->setVolumeLevel(level / (float)100);
        }
    }

    if (m_volText)
    {
        if (muted)
            level = 0.0;

        m_volText->SetText(QString(m_volFormat).arg((int) level));
    }
}

void MiniPlayer::showSpeed(void)
{
    float level = (float)gPlayer->getSpeed();
    QString msg = tr("Speed: ");
    QString param;

    param.sprintf("x%4.2f",level);
    msg += param;

    if (m_infoText)
    {
        m_infoTimer->stop();
        m_showingInfo = true;
        m_infoText->SetText(msg);
        m_infoTimer->start(5000);
    }

    if (class LCD *lcd = LCD::Get())
    {
        QList<LCDTextItem> textItems;
        textItems.append(LCDTextItem(lcd->getLCDHeight() / 2, ALIGN_CENTERED,
                                     msg, "Generic", false));
        lcd->switchToGeneric(textItems);
    }
}
