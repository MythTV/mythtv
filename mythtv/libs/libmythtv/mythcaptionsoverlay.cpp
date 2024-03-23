// MythTV other libs
#include "libmythbase/mythlogging.h"

// MythTV
#include "Bluray/mythbdbuffer.h"
#include "Bluray/mythbdoverlayscreen.h"
#include "captions/subtitlescreen.h"
#include "captions/teletextscreen.h"
#include "mheg/interactivescreen.h"
#include "mythcaptionsoverlay.h"
#include "mythplayerui.h"

#define LOC QString("Captions: ")

MythCaptionsOverlay::MythCaptionsOverlay(MythMainWindow* MainWindow, TV* Tv,
                                         MythPlayerUI* Player, MythPainter* Painter)
  : MythMediaOverlay(MainWindow, Tv, Player, Painter)
{
}

MythCaptionsOverlay::~MythCaptionsOverlay()
{
    MythCaptionsOverlay::TearDown();
}

void MythCaptionsOverlay::TearDown()
{
    MythMediaOverlay::TearDown();
}

void MythCaptionsOverlay::Draw(QRect Rect)
{
    bool visible = false;
    for (auto * screen : std::as_const(m_children))
    {
        if (screen->IsVisible())
        {
            visible = true;
            screen->Pulse();
        }
    }

    if (visible)
    {
        m_painter->Begin(nullptr);
        for (auto * screen : std::as_const(m_children))
        {
            if (screen->IsVisible())
            {
                screen->Draw(m_painter, 0, 0, 255, Rect);
                screen->SetAlpha(255);
                screen->ResetNeedsRedraw();
            }
        }
        m_painter->End();
    }
}

MythScreenType* MythCaptionsOverlay::GetWindow(const QString& Window)
{
    if (m_children.contains(Window))
        return m_children.value(Window);

    MythScreenType* newwindow = nullptr;
    if (Window == OSD_WIN_INTERACT)
        newwindow = new InteractiveScreen(m_player, m_painter, Window);
    else if (Window == OSD_WIN_BDOVERLAY)
        newwindow = new MythBDOverlayScreen(m_player, m_painter, Window);
    else
        return MythMediaOverlay::GetWindow(Window);

    return InitWindow(Window, newwindow);
}

TeletextScreen* MythCaptionsOverlay::InitTeletext()
{
    TeletextScreen* teletext = nullptr;
    if (m_children.contains(OSD_WIN_TELETEXT))
    {
        teletext = qobject_cast<TeletextScreen*>(m_children.value(OSD_WIN_TELETEXT));
    }
    else
    {
        OverrideUIScale();
        teletext = new TeletextScreen(m_player, m_painter, OSD_WIN_TELETEXT, m_fontStretch);
        if (teletext->Create())
        {
            m_children.insert(OSD_WIN_TELETEXT, teletext);
            LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Created window %1").arg(OSD_WIN_TELETEXT));
        }
        else
        {
            delete teletext;
            teletext = nullptr;
        }
        RevertUIScale();
    }

    if (!teletext)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create Teletext window");
        return nullptr;
    }

    HideWindow(OSD_WIN_TELETEXT);
    teletext->SetDisplaying(false);
    return teletext;
}

void MythCaptionsOverlay::EnableTeletext(bool Enable, int Page)
{
    TeletextScreen* tt = InitTeletext();
    if (tt)
    {
        tt->SetVisible(Enable);
        tt->SetDisplaying(Enable);
        if (Enable)
        {
            tt->SetPage(Page, -1);
            LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Enabled teletext page %1").arg(Page));
        }
        else
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC + "Disabled teletext");
        }
    }
}

bool MythCaptionsOverlay::TeletextAction(const QString &Action, bool& Exit)
{
    if (HasWindow(OSD_WIN_TELETEXT))
    {
        auto* tt = qobject_cast<TeletextScreen*>(m_children.value(OSD_WIN_TELETEXT));
        if (tt)
            return tt->KeyPress(Action, Exit);
    }
    return false;
}

void MythCaptionsOverlay::TeletextReset()
{
    if (HasWindow(OSD_WIN_TELETEXT))
    {
        TeletextScreen* tt = InitTeletext();
        if (tt)
            tt->Reset();
    }
}

void MythCaptionsOverlay::TeletextClear()
{
    if (HasWindow(OSD_WIN_TELETEXT))
    {
        auto* tt = qobject_cast<TeletextScreen*>(m_children.value(OSD_WIN_TELETEXT));
        if (tt)
            tt->ClearScreen();
    }
}

SubtitleScreen* MythCaptionsOverlay::InitSubtitles()
{
    SubtitleScreen *sub = nullptr;
    if (m_children.contains(OSD_WIN_SUBTITLE))
    {
        sub = qobject_cast<SubtitleScreen*>(m_children.value(OSD_WIN_SUBTITLE));
    }
    else
    {
        OverrideUIScale();
        sub = new SubtitleScreen(m_player, m_painter, OSD_WIN_SUBTITLE, m_fontStretch);
        if (sub->Create())
        {
            m_children.insert(OSD_WIN_SUBTITLE, sub);
            LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Created window %1").arg(OSD_WIN_SUBTITLE));
        }
        else
        {
            delete sub;
            sub = nullptr;
        }
        RevertUIScale();
    }

    if (!sub)
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create subtitle window");
    return sub;
}

void MythCaptionsOverlay::EnableSubtitles(int Type, bool ForcedOnly)
{
    SubtitleScreen* sub = InitSubtitles();
    if (sub)
        sub->EnableSubtitles(Type, ForcedOnly);
}

void MythCaptionsOverlay::DisableForcedSubtitles()
{
    if (HasWindow(OSD_WIN_SUBTITLE))
    {
        SubtitleScreen *sub = InitSubtitles();
        if (sub)
            sub->DisableForcedSubtitles();
    }
}

void MythCaptionsOverlay::ClearSubtitles()
{
    if (HasWindow(OSD_WIN_SUBTITLE))
    {
        SubtitleScreen* sub = InitSubtitles();
        if (sub)
            sub->ClearAllSubtitles();
    }
}

void MythCaptionsOverlay::DisplayDVDButton(AVSubtitle* DVDButton, QRect &Pos)
{
    if (DVDButton)
    {
        SubtitleScreen* sub = InitSubtitles();
        if (sub)
        {
            EnableSubtitles(kDisplayDVDButton);
            sub->DisplayDVDButton(DVDButton, Pos);
        }
    }
}

void MythCaptionsOverlay::DisplayBDOverlay(MythBDOverlay* Overlay)
{
    if (Overlay)
    {
        auto * bd = qobject_cast<MythBDOverlayScreen*>(GetWindow(OSD_WIN_BDOVERLAY));
        if (bd)
            bd->DisplayBDOverlay(Overlay);
    }
}
