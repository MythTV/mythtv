#include <iostream>
//Added by qt3to4:
#include <QKeyEvent>
#include <Q3PtrList>
#include <Q3Frame>
using namespace std;

// qt
#include <qpixmap.h>
#include <qimage.h>
#include <qapplication.h>

// mythtv
#include <mythtv/mythcontext.h>
#include <mythtv/mythdialogs.h>
#include <mythtv/uitypes.h>
#include <mythtv/lcddevice.h>

// mythmusic
#include "miniplayer.h"
#include "musicplayer.h"
#include "decoder.h"

MiniPlayer::MiniPlayer(MythMainWindow *parent,
                    MusicPlayer *parentPlayer,
                    const char *name,
                    bool setsize)
            : MythThemedDialog(parent, name, setsize)
{
    setFrameStyle(Q3Frame::NoFrame | Q3Frame::Plain);
    setLineWidth(1);
    m_parentPlayer = parentPlayer;

    m_displayTimer = new QTimer(this);
    connect(m_displayTimer, SIGNAL(timeout()), this, SLOT(timerTimeout()));

    m_infoTimer = new QTimer(this);
    connect(m_infoTimer, SIGNAL(timeout()), this, SLOT(showInfoTimeout()));

    wireupTheme();

    gPlayer->setListener(this);

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

    m_showingInfo = false;
}

MiniPlayer::~MiniPlayer(void)
{
    gPlayer->setListener(NULL);

    m_displayTimer->deleteLater();
    m_displayTimer = NULL;

    m_infoTimer->deleteLater();
    m_infoTimer = NULL;

    if (class LCD *lcd = LCD::Get()) 
        lcd->switchToTime ();
}

void MiniPlayer::showPlayer(int showTime)
{
    m_displayTimer->start(showTime * 1000, true);
    exec();
}

void MiniPlayer::timerTimeout(void)
{
    done(Accepted);
}

void MiniPlayer::wireupTheme(void)
{
    QString theme_file = QString("music-");

    if (!loadThemedWindow("miniplayer", theme_file))
    {
        VERBOSE(VB_GENERAL, "MiniPlayer: cannot load theme!");
        done(0);
        return;
    }

    // get dialog size from player_container area
    LayerSet *container = getContainer("player_container");

    if (!container)
    {
        cerr << "MiniPlayer: cannot find the 'player_container'"
                " in your theme" << endl;
        done(0);
        return;
    }

    QRect area = container->GetAreaRect();

    // fixup the container position
    container->SetAreaRect(QRect(0, 0, area.width(), area.height()));

    //fix up the widget positions
    vector<UIType *> *types = container->getAllTypes();
    vector<UIType *>::iterator i = types->begin();
    for (; i != types->end(); i++)
    {
        UIType *type = (*i);
        type->calculateScreenArea();
    }

    setFixedSize(QSize(area.width(), area.height()));

    QPoint pos(area.x(), area.y());
    this->move(pos);

    m_titleText = getUITextType("title_text");
    m_artistText = getUITextType("artist_text");
    m_timeText = getUITextType("time_text");
    m_infoText = getUITextType("info_text");
    m_albumText = getUITextType("album_text");
    m_ratingsImage = getUIRepeatedImageType("ratings_image");
    m_coverImage = getUIImageType("cover_image");
    m_progressBar = getUIStatusBarType("progress_bar");
    m_volText = getUITextType("volume_text");

    if (m_volText && gPlayer->getOutput())
    {
        m_volFormat = m_volText->GetText();
        m_volText->SetText(QString(m_volFormat)
                .arg((int) gPlayer->getOutput()->GetCurrentVolume()));
    }
}

void MiniPlayer::show()
{
    grabKeyboard();

    MythDialog::show();
}

void MiniPlayer::hide()
{
    releaseKeyboard();

    MythDialog::hide();
}

void MiniPlayer::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    if (gContext->GetMainWindow()->TranslateKeyPress("Music", e, actions, false))
    {
        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            handled = true;
            if (action == "ESCAPE")
                done(0);
            else if (action == "SELECT")
                    m_displayTimer->stop();
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
                    return;

                if (gPlayer->getOutput() && gPlayer->getOutput()->IsPaused())
                {
                    gPlayer->pause();
                    return;
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
                        return;
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
                        if (gPlayer->getOutput()->GetMute())
                            m_infoText->SetText(tr("Mute: On"));
                        else
                            m_infoText->SetText(tr("Mute: Off"));

                        m_infoTimer->start(5000, true);
                    }

                    if (m_volText)
                    {
                        if (gPlayer->getOutput()->GetMute())
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
    }
}

void MiniPlayer::customEvent(QEvent *event)
{
    if (isHidden())
        return;

    switch ((int)event->type()) 
    {
        case OutputEvent::Playing:
        {
            if (gPlayer->getCurrentMetadata())
            {
                m_maxTime = gPlayer->getCurrentMetadata()->Length() / 1000;
                updateTrackInfo(gPlayer->getCurrentMetadata());
            }
            break;
        }

        case OutputEvent::Buffering:
        {
            break;
        }

        case OutputEvent::Paused:
        {
            break;
        }

        case OutputEvent::Info:
       {
            OutputEvent *oe = (OutputEvent *) event;

            int rs;
            m_currTime = rs = oe->elapsedSeconds();

            QString time_string = getTimeString(rs, m_maxTime);

            QString info_string;

            //  Hack around for cd bitrates
            if (oe->bitrate() < 2000)
            {
                info_string.sprintf("%d "+tr("kbps")+ "   %.1f "+ tr("kHz")+ "   %s "+ tr("ch"),
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
                    float percent_heard = m_maxTime <=0 ? 0.0 :
                            ((float)rs / (float)gPlayer->getCurrentMetadata()->Length()) * 1000.0;

                    QString lcd_time_string = time_string; 

                    // if the string is longer than the LCD width, remove all spaces
                    if (time_string.length() > lcd->getLCDWidth())
                        lcd_time_string.remove(' ');

                    lcd->setMusicProgress(lcd_time_string, percent_heard);
                }
            }
            break;
        }
        case OutputEvent::Error:
        {
            break;
        }
        case DecoderEvent::Stopped:
        {
            break;
        }
        case DecoderEvent::Finished:
        {
            if (gPlayer->getRepeatMode() == MusicPlayer::REPEAT_TRACK)
               gPlayer->play();
            else 
                gPlayer->next();
            break;
        }
        case DecoderEvent::Error:
        {
            break;
        }
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
    if (m_titleText)
        m_titleText->SetText(mdata->FormatTitle());
    if (m_artistText)
        m_artistText->SetText(mdata->FormatArtist());
    if (m_albumText)
        m_albumText->SetText(mdata->Album());
    if (m_ratingsImage)
        m_ratingsImage->setRepeat(mdata->Rating());

    if (m_coverImage)
    {
        QImage image = gPlayer->getCurrentMetadata()->getAlbumArt();
        if (!image.isNull())
        {
            m_coverImage->SetImage(
                    QPixmap(image.smoothScale(m_coverImage->GetSize(true))));
        }
        else
        {
            m_coverImage->SetImage("mm_nothumb.png");
            m_coverImage->LoadImage();
        }

        m_coverImage->refresh();
    }

    LCD *lcd = LCD::Get();
    if (lcd)
    {
        // Set the Artist and Track on the LCD
        lcd->switchToMusic(mdata->Artist(), 
                       mdata->Album(), 
                       mdata->Title());
    }
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

        if (gPlayer->getDecoder() && gPlayer->getDecoder()->running()) 
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

            //showProgressBar();

            if (class LCD *lcd = LCD::Get())
            {
                float percent_heard = m_maxTime <= 0 ? 0.0 : ((float)pos /
                                      (float)m_maxTime);

                QString lcd_time_string = getTimeString(pos, m_maxTime);

                // if the string is longer than the LCD width, remove all spaces
                if (lcd_time_string.length() > lcd->getLCDWidth())
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

    if (m_ratingsImage)
    {
        curMeta->incRating();
        curMeta->persist();
        m_ratingsImage->setRepeat(curMeta->Rating());

        // if all_music is still in scope we need to keep that in sync
        if (gMusicData->all_music)
        {
            if (gPlayer->getCurrentNode())
            {
                Metadata *mdata = gMusicData->all_music->getMetadata(gPlayer->getCurrentNode()->getInt());
                if (mdata)
                {
                    mdata->incRating();
                }
            }
        }
    }
}

void MiniPlayer::decreaseRating(void)
{
    Metadata *curMeta = gPlayer->getCurrentMetadata();

    if (!curMeta)
        return;

    if (m_ratingsImage)
    {
        curMeta->decRating();
        curMeta->persist();
        m_ratingsImage->setRepeat(curMeta->Rating());

        // if all_music is still in scope we need to keep that in sync
        if (gMusicData->all_music)
        {
            if (gPlayer->getCurrentNode())
            {
                Metadata *mdata = gMusicData->all_music->getMetadata(gPlayer->getCurrentNode()->getInt());
                if (mdata)
                {
                    mdata->decRating();
                }
            }
        }
    }
}

void MiniPlayer::showInfoTimeout(void) 
{
    m_showingInfo = false;
    LCD *lcd = LCD::Get();
    Metadata * mdata = gPlayer->getCurrentMetadata();

    if (lcd && mdata)
    {
        // Set the Artist and Track on the LCD
        lcd->switchToMusic(mdata->Artist(), 
                       mdata->Album(), 
                       mdata->Title());
    }
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
        m_infoTimer->start(5000, true);
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
        m_infoTimer->start(5000, true);
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
        m_infoTimer->start(5000, true);
    }
}

void MiniPlayer::showVolume(void)
{
    float level = (float)gPlayer->getOutput()->GetCurrentVolume();
    bool muted = gPlayer->getOutput()->GetMute();

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
        m_infoTimer->start(5000, true);
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
        m_infoTimer->start(5000, true);
    }

    if (class LCD *lcd = LCD::Get())
    {
        Q3PtrList<LCDTextItem> textItems;
        textItems.setAutoDelete(true);
        textItems.append(new LCDTextItem(lcd->getLCDHeight() / 2, ALIGN_CENTERED,
                                         msg, "Generic", false));
        lcd->switchToGeneric(&textItems);
    }
}
