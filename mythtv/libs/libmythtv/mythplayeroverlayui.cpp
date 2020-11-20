#include <chrono>

// MythTV
#include "tv_play.h"
#include "livetvchain.h"
#include "mythplayeroverlayui.h"

#define LOC QString("PlayerOverlay: ")

// N.B. Overlay is initialised without a player - it must be set before it can be used
MythPlayerOverlayUI::MythPlayerOverlayUI(MythMainWindow* MainWindow, TV* Tv, PlayerContext* Context, PlayerFlags Flags)
  : MythPlayerUIBase(MainWindow, Tv, Context, Flags),
    m_osd(MainWindow, Tv, nullptr, m_painter)
{
    // Register our state type for signalling
    qRegisterMetaType<MythOverlayState>();

    m_positionUpdateTimer.setInterval(999ms);
    connect(&m_positionUpdateTimer, &QTimer::timeout, this, &MythPlayerOverlayUI::UpdateOSDPosition);
    connect(this, &MythPlayerOverlayUI::OverlayStateChanged, m_tv, &TV::OverlayStateChanged);
    connect(m_tv, &TV::ChangeOSDMessage, this, QOverload<const QString&>::of(&MythPlayerOverlayUI::UpdateOSDMessage));

    // Signalled directly from TV to OSD
    connect(m_tv, &TV::DialogQuit, &m_osd, &OSD::DialogQuit);
    connect(m_tv, &TV::HideAll,    &m_osd, &OSD::HideAll);
}

void MythPlayerOverlayUI::BrowsingChanged(bool Browsing)
{
    m_browsing = Browsing;
    emit OverlayStateChanged({ m_browsing, m_editing });
}

void MythPlayerOverlayUI::EditingChanged(bool Editing)
{
    m_editing = Editing;
    emit OverlayStateChanged({ m_browsing, m_editing });
}

void MythPlayerOverlayUI::ChangeOSDPositionUpdates(bool Enable)
{
    if (Enable)
        m_positionUpdateTimer.start();
    else
        m_positionUpdateTimer.stop();
}

/*! \brief Update the OSD status/position window.
 *
 * This is triggered (roughly) once a second to update the osd_status window
 * for the latest position, duration, time etc (when visible).
 *
 * \note If data/state from a higher interface class is needed, then just re-implement
 * calcSliderPos
*/
void MythPlayerOverlayUI::UpdateOSDPosition()
{
    m_osdLock.lock();
    if (m_osd.IsWindowVisible(OSD_WIN_STATUS))
    {
        osdInfo info;
        UpdateSliderInfo(info);
        m_osd.SetText(OSD_WIN_STATUS, info.text, kOSDTimeout_Ignore);
        m_osd.SetValues(OSD_WIN_STATUS, info.values, kOSDTimeout_Ignore);
    }
    else
    {
        ChangeOSDPositionUpdates(false);
    }
    m_osdLock.unlock();
}

void MythPlayerOverlayUI::UpdateOSDMessage(const QString& Message)
{
    UpdateOSDMessage(Message, kOSDTimeout_Med);
}

void MythPlayerOverlayUI::UpdateOSDMessage(const QString& Message, OSDTimeout Timeout)
{
    m_osdLock.lock();
    InfoMap map;
    map.insert("message_text", Message);
    m_osd.SetText(OSD_WIN_MESSAGE, map, Timeout);
    m_osdLock.unlock();
}

void MythPlayerOverlayUI::SetOSDStatus(const QString& title, OSDTimeout timeout)
{
    m_osdLock.lock();
    osdInfo info;
    UpdateSliderInfo(info);
    info.text.insert("title", title);
    m_osd.SetText(OSD_WIN_STATUS, info.text, timeout);
    m_osd.SetValues(OSD_WIN_STATUS, info.values, timeout);
    m_osdLock.unlock();
}

void MythPlayerOverlayUI::UpdateOSDStatus(osdInfo &Info, int Type, OSDTimeout Timeout)
{
    m_osdLock.lock();
    m_osd.ResetWindow(OSD_WIN_STATUS);
    m_osd.SetValues(OSD_WIN_STATUS, Info.values, Timeout);
    m_osd.SetText(OSD_WIN_STATUS,   Info.text, Timeout);
    if (Type != kOSDFunctionalType_Default)
        m_osd.SetFunctionalWindow(OSD_WIN_STATUS, static_cast<OSDFunctionalType>(Type));
    m_osdLock.unlock();
}

void MythPlayerOverlayUI::UpdateOSDStatus(const QString& Title, const QString& Desc,
                                          const QString& Value, int Type, const QString& Units,
                                          int Position, OSDTimeout Timeout)
{
    osdInfo info;
    info.values.insert("position", Position);
    info.values.insert("relposition", Position);
    info.text.insert("title", Title);
    info.text.insert("description", Desc);
    info.text.insert("value", Value);
    info.text.insert("units", Units);
    UpdateOSDStatus(info, Type, Timeout);
}

void MythPlayerOverlayUI::UpdateSliderInfo(osdInfo &Info, bool PaddedFields)
{
    if (!m_decoder)
        return;

    bool islive = false;
    Info.text.insert("chapteridx",    QString());
    Info.text.insert("totalchapters", QString());
    Info.text.insert("titleidx",      QString());
    Info.text.insert("totaltitles",   QString());
    Info.text.insert("angleidx",      QString());
    Info.text.insert("totalangles",   QString());
    Info.values.insert("position",    0);
    Info.values.insert("progbefore",  0);
    Info.values.insert("progafter",   0);

    int playbackLen = 0;
    bool fixed_playbacklen = false;

    if (m_liveTV && m_playerCtx->m_tvchain)
    {
        Info.values["progbefore"] = static_cast<int>(m_playerCtx->m_tvchain->HasPrev());
        Info.values["progafter"]  = static_cast<int>(m_playerCtx->m_tvchain->HasNext());
        playbackLen = m_playerCtx->m_tvchain->GetLengthAtCurPos();
        islive = true;
        fixed_playbacklen = true;
    }
    else if (IsWatchingInprogress())
    {
        islive = true;
    }
    else
    {
        int chapter  = GetCurrentChapter();
        int chapters = GetNumChapters();
        if (chapter && chapters > 1)
        {
            Info.text["chapteridx"]    = QString::number(chapter + 1);
            Info.text["totalchapters"] = QString::number(chapters);
        }

        int title  = GetCurrentTitle();
        int titles = GetNumTitles();
        if (title && titles > 1)
        {
            Info.text["titleidx"]    = QString::number(title + 1);
            Info.text["totaltitles"] = QString::number(titles);
        }

        int angle  = GetCurrentAngle();
        int angles = GetNumAngles();
        if (angle && angles > 1)
        {
            Info.text["angleidx"]    = QString::number(angle + 1);
            Info.text["totalangles"] = QString::number(angles);
        }
    }

    // Set the raw values, followed by the translated values.
    for (int i = 0; i < 2 ; ++i)
    {
        bool honorCutList = (i > 0);
        bool stillFrame = false;
        int  pos = 0;

        QString relPrefix = (honorCutList ? "rel" : "");
        if (!fixed_playbacklen)
            playbackLen = static_cast<int>(GetTotalSeconds(honorCutList));
        int secsplayed = static_cast<int>(GetSecondsPlayed(honorCutList));

        stillFrame = (secsplayed < 0);
        playbackLen = std::max(playbackLen, 0);
        secsplayed = std::min(playbackLen, std::max(secsplayed, 0));
        int secsbehind = std::max((playbackLen - secsplayed), 0);

        if (playbackLen > 0)
            pos = static_cast<int>(1000.0F * (secsplayed / static_cast<float>(playbackLen)));

        Info.values.insert(relPrefix + "secondsplayed", secsplayed);
        Info.values.insert(relPrefix + "totalseconds", playbackLen);
        Info.values[relPrefix + "position"] = pos;

        QString text1;
        QString text2;
        QString text3;
        if (PaddedFields)
        {
            text1 = MythFormatTime(secsplayed, "HH:mm:ss");
            text2 = MythFormatTime(playbackLen, "HH:mm:ss");
            text3 = MythFormatTime(secsbehind, "HH:mm:ss");
        }
        else
        {
            QString fmt = (playbackLen >= ONEHOURINSEC) ? "H:mm:ss" : "m:ss";
            text1 = MythFormatTime(secsplayed, fmt);
            text2 = MythFormatTime(playbackLen, fmt);

            if (secsbehind >= ONEHOURINSEC)
                text3 = MythFormatTime(secsbehind, "H:mm:ss");
            else if (secsbehind >= ONEMININSEC)
                text3 = MythFormatTime(secsbehind, "m:ss");
            else
                text3 = tr("%n second(s)", "", secsbehind);
        }

        QString desc = stillFrame ? tr("Still Frame") : tr("%1 of %2").arg(text1).arg(text2);
        Info.text[relPrefix + "description"] = desc;
        Info.text[relPrefix + "playedtime"] = text1;
        Info.text[relPrefix + "totaltime"] = text2;
        Info.text[relPrefix + "remainingtime"] = islive ? QString() : text3;
        Info.text[relPrefix + "behindtime"] = islive ? text3 : QString();
    }
}

// GetSecondsPlayed() and GetTotalSeconds() internally calculate
// in terms of milliseconds and divide the result by 1000.  This
// divisor can be passed in as an argument, e.g. pass divisor=1 to
// return the time in milliseconds.
int64_t MythPlayerOverlayUI::GetSecondsPlayed(bool HonorCutList, int Divisor)
{
    uint64_t pos = TranslatePositionFrameToMs(m_framesPlayed, HonorCutList);
    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("GetSecondsPlayed: framesPlayed %1, honorCutList %2, divisor %3, pos %4")
        .arg(m_framesPlayed).arg(HonorCutList).arg(Divisor).arg(pos));
    return static_cast<int64_t>(TranslatePositionFrameToMs(m_framesPlayed, HonorCutList) / static_cast<uint64_t>(Divisor));
}

int64_t MythPlayerOverlayUI::GetTotalSeconds(bool HonorCutList, int Divisor) const
{
    uint64_t pos = IsWatchingInprogress() ? UINT64_MAX : m_totalFrames;
    return static_cast<int64_t>(TranslatePositionFrameToMs(pos, HonorCutList) / static_cast<uint64_t>(Divisor));
}
