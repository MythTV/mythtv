// Qt
#include <utility>

// libmythbase
#include "libmythbase/mythlogging.h"

// libmythui
#include "libmythui/mythdialogbox.h"
#include "libmythui/mythmainwindow.h"
#include "libmythui/mythpainter.h"
#include "libmythui/mythuibutton.h"
#include "libmythui/mythuieditbar.h"
#include "libmythui/mythuigroup.h"
#include "libmythui/mythuihelper.h"
#include "libmythui/mythuiimage.h"
#include "libmythui/mythuiprogressbar.h"
#include "libmythui/mythuistatetype.h"
#include "libmythui/mythuitext.h"

// libmythtv
#include "overlays/mythnavigationoverlay.h"
#include "overlays/mythchanneloverlay.h"
#include "channelutil.h"
#include "osd.h"
#include "tv_play.h"
#include "mythplayerui.h"

#define LOC     QString("OSD: ")

OSD::OSD(MythMainWindow *MainWindow, TV *Tv, MythPlayerUI* Player, MythPainter* Painter)
  : MythMediaOverlay(MainWindow, Tv, Player, Painter)
{
    connect(this, &OSD::HideOSD,        m_tv, &TV::HandleOSDClosed);
    connect(m_tv, &TV::IsOSDVisible,    this, &OSD::IsOSDVisible);
    connect(m_tv, &TV::ChangeOSDDialog, this, &OSD::ShowDialog);
    connect(m_tv, &TV::ChangeOSDText,   this, &OSD::SetText);
}

OSD::~OSD()
{
    OSD::TearDown();
}

void OSD::TearDown()
{
    MythMediaOverlay::TearDown();
    m_dialog = nullptr;
}

bool OSD::Init(QRect Rect, float FontAspect)
{
    int newstretch = static_cast<int>(lroundf(FontAspect * 100));
    if ((Rect == m_rect) && (newstretch == m_fontStretch))
        return true;

    HideAll(false);
    TearDown();
    m_rect = Rect;
    m_fontStretch = newstretch;

    OverrideUIScale();
    LoadWindows();
    RevertUIScale();

    if (m_children.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to load any windows.");
        return false;
    }

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Loaded OSD: size %1x%2 offset %3+%4")
        .arg(m_rect.width()).arg(m_rect.height()).arg(m_rect.left()).arg(m_rect.top()));
    HideAll(false);
    return true;
}

void OSD::Embed(bool Embedding)
{
    m_embedded = Embedding;
}

void OSD::IsOSDVisible(bool& Visible)
{
    if (m_mainWindow->GetCurrentNotificationCenter()->DisplayedNotifications() > 0)
    {
        Visible = true;
        return;
    }

    Visible = std::any_of(m_children.cbegin(), m_children.cend(),
                [](MythScreenType* child) { return child->IsVisible(); });
}

void OSD::HideAll(bool KeepSubs, MythScreenType* Except, bool DropNotification)
{
    if (DropNotification)
    {
        if (m_mainWindow->GetCurrentNotificationCenter()->RemoveFirst())
            return; // we've removed the top window, don't process any further
    }

    QMutableMapIterator<QString, MythScreenType*> it(m_children);
    while (it.hasNext())
    {
        it.next();
        if (Except && Except->objectName() == OSD_DLG_NAVIGATE
            && it.value()->objectName() == OSD_WIN_STATUS)
            continue;
        bool match1 = KeepSubs &&
                     (it.key() == OSD_WIN_SUBTITLE  ||
                      it.key() == OSD_WIN_TELETEXT);
        bool match2 = it.key() == OSD_WIN_BDOVERLAY ||
                      it.key() == OSD_WIN_INTERACT  ||
                      it.value() == Except;
        if (!(match1 || match2))
            HideWindow(it.key());
    }
}

void OSD::LoadWindows()
{
    static const std::array<const QString,7> s_defaultWindows {
        OSD_WIN_MESSAGE, OSD_WIN_INPUT, OSD_WIN_PROGINFO, OSD_WIN_BROWSE,
        OSD_WIN_STATUS, OSD_WIN_PROGEDIT, OSD_WIN_DEBUG };

    for (const auto & window : s_defaultWindows)
    {
        auto * win = new MythOverlayWindow(nullptr, m_painter, window, true);
        if (win->Create())
        {
            PositionWindow(win);
            LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Loaded window %1").arg(window));
            m_children.insert(window, win);

            // Update player for window visibility
            if (window == OSD_WIN_BROWSE)
                connect(win, &MythOverlayWindow::VisibilityChanged, m_player, &MythPlayerUI::BrowsingChanged);
            if (window == OSD_WIN_PROGEDIT)
                connect(win, &MythOverlayWindow::VisibilityChanged, m_player, &MythPlayerUI::EditingChanged);
            if (window == OSD_WIN_DEBUG)
                connect(win, &MythOverlayWindow::VisibilityChanged, m_player, &MythPlayerUI::OSDDebugVisibilityChanged);
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("Failed to load window %1").arg(window));
            delete win;
        }
    }
}

void OSD::SetValues(const QString &Window, const QHash<QString,int> &Map, OSDTimeout Timeout)
{
    MythScreenType *win = GetWindow(Window);
    if (!win)
        return;

    bool found = false;
    if (Map.contains("position"))
    {
        MythUIProgressBar *bar = dynamic_cast<MythUIProgressBar *> (win->GetChild("position"));
        if (bar)
        {
            bar->SetVisible(true);
            bar->SetStart(0);
            bar->SetTotal(1000);
            bar->SetUsed(Map.value("position"));
            found = true;
        }
    }
    if (Map.contains("relposition"))
    {
        MythUIProgressBar *bar = dynamic_cast<MythUIProgressBar *> (win->GetChild("relposition"));
        if (bar)
        {
            bar->SetVisible(true);
            bar->SetStart(0);
            bar->SetTotal(1000);
            bar->SetUsed(Map.value("relposition"));
            found = true;
        }
    }

    if (found)
        SetExpiry(Window, Timeout);
}

void OSD::SetValues(const QString &Window, const QHash<QString,float> &Map,
                    OSDTimeout Timeout)
{
    MythScreenType *win = GetWindow(Window);
    if (!win)
        return;

    bool found = false;
    if (Map.contains("position"))
    {
        MythUIEditBar *edit = dynamic_cast<MythUIEditBar *> (win->GetChild("editbar"));
        if (edit)
        {
            edit->SetEditPosition(static_cast<double>(Map.value("position")));
            found = true;
        }
    }

    if (found)
        SetExpiry(Window, Timeout);
}

void OSD::SetText(const QString &Window, const InfoMap &Map, OSDTimeout Timeout)
{
    MythScreenType *win = GetWindow(Window);
    if (!win)
        return;

    if (Map.contains("numstars"))
    {
        MythUIStateType *state = dynamic_cast<MythUIStateType *> (win->GetChild("ratingstate"));
        if (state)
            state->DisplayState(Map["numstars"]);
    }
    if (Map.contains("tvstate"))
    {
        MythUIStateType *state = dynamic_cast<MythUIStateType *> (win->GetChild("tvstate"));
        if (state)
            state->DisplayState(Map["tvstate"]);
    }
    if (Map.contains("videocodec"))
    {
        MythUIStateType *state = dynamic_cast<MythUIStateType *> (win->GetChild("videocodec"));
        if (state)
            state->DisplayState(Map["videocodec"]);
    }
    if (Map.contains("videodescrip"))
    {
        MythUIStateType *state = dynamic_cast<MythUIStateType *> (win->GetChild("videodescrip"));
        if (state)
            state->DisplayState(Map["videodescrip"]);
    }
    if (Map.contains("audiocodec"))
    {
        MythUIStateType *state = dynamic_cast<MythUIStateType *> (win->GetChild("audiocodec"));
        if (state)
            state->DisplayState(Map["audiocodec"]);
    }
    if (Map.contains("audiochannels"))
    {
        MythUIStateType *state = dynamic_cast<MythUIStateType *> (win->GetChild("audiochannels"));
        if (state)
            state->DisplayState(Map["audiochannels"]);
    }
    if (Map.contains("chanid"))
    {
        MythUIImage *icon = dynamic_cast<MythUIImage *> (win->GetChild("iconpath"));
        if (icon)
        {
            icon->Reset();

            uint chanid = Map["chanid"].toUInt();
            QString iconpath;
            if (Map.contains("iconpath"))
                iconpath = Map["iconpath"];
            else
                iconpath = ChannelUtil::GetIcon(chanid);

            if (!iconpath.isEmpty())
            {
                QString iconurl =
                                gCoreContext->GetMasterHostPrefix("ChannelIcons",
                                                                  iconpath);

                icon->SetFilename(iconurl);
                icon->Load(false);
            }
        }
    }

    if (Map.contains("channelgroup"))
    {
        MythUIText *textArea = dynamic_cast<MythUIText *> (win->GetChild("channelgroup"));
        if (textArea)
        {
            textArea->SetText(Map["channelgroup"]);
        }
    }

    if (Map.contains("inetref"))
    {
        MythUIImage *cover = dynamic_cast<MythUIImage *> (win->GetChild("coverart"));
        if (cover && Map.contains("coverartpath"))
        {
            QString coverpath = Map["coverartpath"];
            cover->SetFilename(coverpath);
            cover->Load(false);
        }
        MythUIImage *fanart = dynamic_cast<MythUIImage *> (win->GetChild("fanart"));
        if (fanart && Map.contains("fanartpath"))
        {
            QString fanartpath = Map["fanartpath"];
            fanart->SetFilename(fanartpath);
            fanart->Load(false);
        }
        MythUIImage *banner = dynamic_cast<MythUIImage *> (win->GetChild("banner"));
        if (banner && Map.contains("bannerpath"))
        {
            QString bannerpath = Map["bannerpath"];
            banner->SetFilename(bannerpath);
            banner->Load(false);
        }
        MythUIImage *screenshot = dynamic_cast<MythUIImage *> (win->GetChild("screenshot"));
        if (screenshot && Map.contains("screenshotpath"))
        {
            QString screenshotpath = Map["screenshotpath"];
            screenshot->SetFilename(screenshotpath);
            screenshot->Load(false);
        }
    }
    if (Map.contains("nightmode"))
    {
        MythUIStateType *state = dynamic_cast<MythUIStateType *> (win->GetChild("nightmode"));
        if (state)
            state->DisplayState(Map["nightmode"]);
    }
    if (Map.contains("mediatype"))
    {
        MythUIStateType *state = dynamic_cast<MythUIStateType *> (win->GetChild("mediatype"));
        if (state)
            state->DisplayState(Map["mediatype"]);
    }

    MythUIProgressBar *bar =
        dynamic_cast<MythUIProgressBar *>(win->GetChild("elapsedpercent"));
    if (bar)
    {
        qint64 startts = Map["startts"].toLongLong();
        qint64 endts   = Map["endts"].toLongLong();
        qint64 nowts   = MythDate::current().toSecsSinceEpoch();
        if (startts > nowts)
        {
            bar->SetUsed(0);
        }
        else if (endts < nowts)
        {
            bar->SetUsed(1000);
        }
        else
        {
            qint64 duration = endts - startts;
            if (duration > 0)
                bar->SetUsed(static_cast<int>(1000 * (nowts - startts) / duration));
            else
                bar->SetUsed(0);
        }
        bar->SetVisible(startts > 0);
        bar->SetStart(0);
        bar->SetTotal(1000);
    }

    win->SetVisible(true);

    if (win == m_dialog)
    {
        auto *edit = qobject_cast<MythChannelOverlay*>(m_dialog);
        if (edit)
            edit->SetText(Map);
        else
            win->SetTextFromMap(Map);
    }
    else
    {
        win->SetTextFromMap(Map);
    }

    SetExpiry(Window, Timeout);
}

void OSD::SetRegions(const QString &Window, frm_dir_map_t &Map, long long Total)
{
    MythScreenType *win = GetWindow(Window);
    if (!win)
        return;

    MythUIEditBar *bar = dynamic_cast<MythUIEditBar*>(win->GetChild("editbar"));
    if (!bar)
        return;

    bar->ClearRegions();
    if (Map.empty() || Total < 1)
    {
        bar->Display();
        return;
    }

    long long start = -1;
    long long end   = -1;
    bool first = true;
    QMapIterator<uint64_t, MarkTypes> it(Map);
    while (it.hasNext())
    {
        bool error = false;
        it.next();
        if (it.value() == MARK_CUT_START)
        {
            start = static_cast<long long>(it.key());
            if (end > -1)
                error = true;
        }
        else if (it.value() == MARK_CUT_END)
        {
            if (first)
                start = 0;
            if (start < 0)
                error = true;
            end = static_cast<long long>(it.key());
        }
        else if (it.value() == MARK_PLACEHOLDER)
        {
            start = end = static_cast<long long>(it.key());
        }
        first = false;

        if (error)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "deleteMap discontinuity");
            start = -1;
            end   = -1;
        }

        if (start >=0 && end >= 0)
        {
            bar->AddRegion((static_cast<double>(start) / static_cast<double>(Total)),
                           (static_cast<double>(end) / static_cast<double>(Total)));
            start = -1;
            end   = -1;
        }
    }
    if (start > -1 && end < 0)
        bar->AddRegion(static_cast<double>(start) / static_cast<double>(Total), 1.0);

    bar->Display();
}

void OSD::SetGraph(const QString &Window, const QString &Graph, std::chrono::milliseconds Timecode)
{
    MythScreenType *win = GetWindow(Window);
    if (!win)
        return;

    auto *image = dynamic_cast<MythUIImage* >(win->GetChild(Graph));
    if (!image)
        return;

    MythImage* mi = m_player->GetAudioGraph().GetImage(Timecode);
    if (mi)
        image->SetImage(mi);
}

void OSD::Draw()
{
    if (m_embedded)
        return;

    bool visible = false;
    QTime now = MythDate::current().time();

    CheckExpiry();
    for (auto * screen : std::as_const(m_children))
    {
        if (screen->IsVisible())
        {
            visible = true;
            screen->Pulse();
            if (m_expireTimes.contains(screen))
            {
                QTime expires = m_expireTimes.value(screen).time();
                auto left = std::chrono::milliseconds(now.msecsTo(expires));
                if (left < m_fadeTime)
                    screen->SetAlpha((255 * left) / m_fadeTime);
            }
        }
    }

    QList<MythScreenType*> notifications;
    m_mainWindow->GetCurrentNotificationCenter()->GetNotificationScreens(notifications);
    QList<MythScreenType*>::iterator it2 = notifications.begin();
    while (it2 != notifications.end())
    {
        if (!MythNotificationCenter::ScreenCreated(*it2))
        {
            LOG(VB_GUI, LOG_DEBUG, LOC + "Creating OSD Notification");

            if (!m_uiScaleOverride)
                OverrideUIScale(false);
            (*it2)->SetPainter(m_painter);
            if (!(*it2)->Create())
            {
                it2 = notifications.erase(it2);
                continue;
            }
        }

        if ((*it2)->IsVisible())
        {
            if (!m_uiScaleOverride)
                OverrideUIScale(false);

            (*it2)->SetPainter(m_painter);

            MythNotificationCenter::UpdateScreen(*it2);

            visible = true;
            (*it2)->Pulse();
            QTime expires = MythNotificationCenter::ScreenExpiryTime(*it2).time();
            auto left = std::chrono::milliseconds(now.msecsTo(expires));
            if (left < 0ms)
                left = 0ms;
            if (expires.isValid() && left < m_fadeTime)
                (*it2)->SetAlpha((255 * left) / m_fadeTime);
        }
        ++it2;
    }
    RevertUIScale();

    if (visible)
    {
        m_painter->Begin(nullptr);
        for (auto * screen : std::as_const(m_children))
        {
            if (screen->IsVisible())
            {
                screen->Draw(m_painter, 0, 0, 255, m_rect);
                screen->SetAlpha(255);
                screen->ResetNeedsRedraw();
            }
        }
        for (auto * notif : std::as_const(notifications))
        {
            if (notif->IsVisible())
            {
                notif->Draw(m_painter, 0, 0, 255, m_rect);
                notif->SetAlpha(255);
                notif->ResetNeedsRedraw();
            }
        }
        m_painter->End();
    }
}

void OSD::CheckExpiry()
{
    QDateTime now = MythDate::current();
    QMutableHashIterator<MythScreenType*, QDateTime> it(m_expireTimes);
    while (it.hasNext())
    {
        it.next();
        if (it.value() < now)
        {
            if (it.key() == m_dialog)
                DialogQuit();
            else
                HideWindow(m_children.key(it.key()));
        }
        else if (it.key() == m_dialog)
        {
            if (!m_pulsedDialogText.isEmpty() && now > m_nextPulseUpdate)
            {
                QString newtext = m_pulsedDialogText;
                auto *dialog = qobject_cast<MythDialogBox*>(m_dialog);
                if (dialog)
                {
                    // The disambiguation string must be an empty string
                    // and not a NULL to get extracted by the Qt tools.
                    QString replace = QCoreApplication::translate("(Common)",
                                          "%n second(s)", "",
                                          static_cast<int>(now.secsTo(it.value())));
                    dialog->SetText(newtext.replace("%d", replace));
                }
                auto *cdialog = qobject_cast<MythConfirmationDialog*>(m_dialog);
                if (cdialog)
                {
                    QString replace = QString::number(now.secsTo(it.value()));
                    cdialog->SetMessage(newtext.replace("%d", replace));
                }
                m_nextPulseUpdate = now.addSecs(1);
            }
        }
    }
}

void OSD::SetExpiry(const QString &Window, enum OSDTimeout Timeout,
                    std::chrono::milliseconds CustomTimeout)
{
    SetExpiryPriv(Window, Timeout, CustomTimeout);
    if (IsWindowVisible(Window))
    {
        // Keep status and nav timeouts in sync
        if (Window == OSD_DLG_NAVIGATE)
            SetExpiryPriv(OSD_WIN_STATUS, Timeout, CustomTimeout);
        else if (Window == OSD_WIN_STATUS && IsWindowVisible(OSD_DLG_NAVIGATE))
            SetExpiryPriv(OSD_DLG_NAVIGATE, Timeout, CustomTimeout);
    }
}

void OSD::SetExpiryPriv(const QString &Window, enum OSDTimeout Timeout,
                        std::chrono::milliseconds CustomTimeout)
{
    std::chrono::milliseconds time { 0ms };
    if (CustomTimeout != 0ms)
        time = CustomTimeout;
    else if ((Timeout > kOSDTimeout_Ignore) && (Timeout <= kOSDTimeout_Long))
        time = m_timeouts[static_cast<size_t>(Timeout)];
    else
        return;

    MythScreenType *win = GetWindow(Window);
    if ((time > 0ms) && win)
    {
        QDateTime expires = MythDate::current().addMSecs(time.count());
            m_expireTimes.insert(win, expires);
    }
    else if ((time < 0ms) && win)
    {
        if (m_expireTimes.contains(win))
            m_expireTimes.remove(win);
    }
}

bool OSD::IsWindowVisible(const QString &Window)
{
    if (!m_children.contains(Window))
        return false;

    return m_children.value(Window)->IsVisible(/*true*/);
}

void OSD::ResetWindow(const QString &Window)
{
    if (!m_children.contains(Window))
        return;

    MythScreenType *screen = m_children.value(Window);
    if (screen != nullptr)
        screen->Reset();
}

void OSD::PositionWindow(MythScreenType *Window)
{
    if (!Window)
        return;

    MythRect rect = Window->GetArea();
    rect.translate(m_rect.left(), m_rect.top());
    Window->SetArea(rect);
}

void OSD::RemoveWindow(const QString &Window)
{
    if (!m_children.contains(Window))
        return;

    HideWindow(Window);
    MythScreenType *child = m_children.value(Window);
    m_children.remove(Window);
    delete child;
}

void OSD::SetFunctionalWindow(const QString &window, enum OSDFunctionalType Type)
{
    if (m_functionalType != kOSDFunctionalType_Default && m_functionalType != Type)
        emit HideOSD(m_functionalType);
    m_functionalWindow = window;
    m_functionalType   = Type;
}

void OSD::HideWindow(const QString &Window)
{
    if (!m_children.contains(Window))
        return;

    MythMediaOverlay::HideWindow(Window);

    SetExpiry(Window, kOSDTimeout_None);

    MythScreenType* screen = m_children.value(Window);
    if ((m_functionalType != kOSDFunctionalType_Default) && screen)
    {
        bool valid   = m_children.contains(m_functionalWindow);
        screen = m_children.value(m_functionalWindow);
        bool visible = valid && screen && screen->IsVisible(false);
        if (!valid || !visible)
        {
            emit HideOSD(m_functionalType);
            m_functionalType = kOSDFunctionalType_Default;
            m_functionalWindow = QString();
        }
    }
}

bool OSD::DialogVisible(const QString& Window)
{
    if (!m_dialog || Window.isEmpty())
        return m_dialog;
    return m_dialog->objectName() == Window;
}

bool OSD::DialogHandleKeypress(QKeyEvent *Event)
{
    if (!m_dialog)
        return false;
    return m_dialog->keyPressEvent(Event);
}

bool OSD::DialogHandleGesture(MythGestureEvent *Event)
{
    if (!m_dialog)
        return false;
    return m_dialog->gestureEvent(Event);
}

void OSD::DialogQuit()
{
    if (!m_dialog)
        return;

    RemoveWindow(m_dialog->objectName());
    m_dialog = nullptr;
    m_pulsedDialogText = QString();
}

/*! \brief Show a dialog menu, removing any existing dialog
 *
 * This slot deliberately uses a const reference despite the minor
 * performance penalty. This simplifies memory management (e.g. if the signal
 * is not delivered when there is no OSD) and allows for possible future changes
 * where the OSD and TV objects do not reside in the same thread.
*/
void OSD::ShowDialog(const MythOSDDialogData& Data)
{
    DialogShow(Data.m_dialogName, Data.m_message, Data.m_timeout);
    std::for_each(Data.m_buttons.cbegin(), Data.m_buttons.cend(),
        [this](const MythOSDDialogData::MythOSDDialogButton& B) {
            DialogAddButton(B.m_text, B.m_data, B.m_menu, B.m_current); });
    DialogBack(Data.m_back.m_text, Data.m_back.m_data, Data.m_back.m_exit);
}

void OSD::DialogShow(const QString &Window, const QString &Text, std::chrono::milliseconds UpdateFor)
{
    if (m_dialog)
    {
        QString current = m_dialog->objectName();
        if (current != Window)
        {
            DialogQuit();
        }
        else
        {
            auto *dialog = qobject_cast<MythDialogBox*>(m_dialog);
            if (dialog)
            {
                dialog->Reset();
                dialog->SetText(Text);
            }
        }
    }

    if (!m_dialog)
    {
        OverrideUIScale();
        MythScreenType *dialog = nullptr;

        if (Window == OSD_DLG_EDITOR)
            dialog = new MythChannelOverlay(m_mainWindow, m_tv, Window.toLatin1());
        else if (Window == OSD_DLG_CONFIRM)
            dialog = new MythConfirmationDialog(nullptr, Text, false);
        else if (Window == OSD_DLG_NAVIGATE)
            dialog = new MythNavigationOverlay(m_mainWindow, m_tv, m_player, Window, this);
        else
            dialog = new MythDialogBox(Text, nullptr, Window.toLatin1(), false, true);

        dialog->SetPainter(m_painter);
        if (dialog->Create())
        {
            PositionWindow(dialog);
            m_dialog = dialog;
            auto *dbox = qobject_cast<MythDialogBox*>(m_dialog);
            if (dbox)
                dbox->SetReturnEvent(m_tv, Window);
            auto *cbox = qobject_cast<MythConfirmationDialog*>(m_dialog);
            if (cbox)
            {
                cbox->SetReturnEvent(m_tv, Window);
                cbox->SetData("DIALOG_CONFIRM_X_X");
            }
            m_children.insert(Window, m_dialog);
        }
        else
        {
            RevertUIScale();
            delete dialog;
            return;
        }

        RevertUIScale();
    }

    if (UpdateFor > 0ms)
    {
        m_nextPulseUpdate  = MythDate::current();
        m_pulsedDialogText = Text;
        SetExpiry(Window, kOSDTimeout_None, UpdateFor);
    }

    DialogBack();
    HideAll(true, m_dialog);
    m_dialog->SetVisible(true);
}

void OSD::DialogBack(const QString& Text, const QVariant& Data, bool Exit)
{
    auto *dialog = qobject_cast<MythDialogBox*>(m_dialog);
    if (dialog)
    {
        dialog->SetBackAction(Text, Data);
        if (Exit)
            dialog->SetExitAction(Text, Data);
    }
}

void OSD::DialogAddButton(const QString& Text, QVariant Data, bool Menu, bool Current)
{
    auto *dialog = qobject_cast<MythDialogBox*>(m_dialog);
    if (dialog)
        dialog->AddButtonV(Text, std::move(Data), Menu, Current);
}

void OSD::DialogGetText(InfoMap &Map)
{
    auto *edit = qobject_cast<MythChannelOverlay*>(m_dialog);
    if (edit)
        edit->GetText(Map);
}
