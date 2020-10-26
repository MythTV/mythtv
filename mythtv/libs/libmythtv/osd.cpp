// Qt
#include <utility>

// libmyth
#include "mythlogging.h"

// libmythui
#include "mythmainwindow.h"
#include "mythuihelper.h"
#include "mythpainter.h"
#include "mythuiimage.h"
#include "mythuiprogressbar.h"
#include "mythdialogbox.h"
#include "mythuitext.h"
#include "mythuibutton.h"
#include "mythuieditbar.h"
#include "mythuistatetype.h"
#include "mythuigroup.h"

// libmythtv
#include "channelutil.h"
#include "osd.h"
#include "tv_play.h"
#include "mythplayerui.h"

#define LOC     QString("OSD: ")

ChannelEditor::ChannelEditor(MythMainWindow* MainWindow, TV* Tv, const QString& Name)
  : MythScreenType(static_cast<MythScreenType*>(nullptr), Name),
    m_mainWindow(MainWindow),
    m_tv(Tv)
{
}

bool ChannelEditor::Create()
{
    if (!XMLParseBase::LoadWindowFromXML("osd.xml", "ChannelEditor", this))
        return false;

    MythUIButton *probeButton = nullptr;
    MythUIButton *okButton    = nullptr;

    bool err = false;
    UIUtilE::Assign(this, m_callsignEdit, "callsign", &err);
    UIUtilE::Assign(this, m_channumEdit,  "channum",  &err);
    UIUtilE::Assign(this, m_channameEdit, "channame", &err);
    UIUtilE::Assign(this, m_xmltvidEdit,  "XMLTV",    &err);
    UIUtilE::Assign(this, probeButton,    "probe",    &err);
    UIUtilE::Assign(this, okButton,       "ok",       &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'ChannelEditor'");
        return false;
    }

    BuildFocusList();
    connect(okButton,    &MythUIButton::Clicked, this, &ChannelEditor::Confirm);
    connect(probeButton, &MythUIButton::Clicked, this, &ChannelEditor::Probe);
    SetFocusWidget(okButton);

    return true;
}

void ChannelEditor::Confirm()
{
    SendResult(1);
}

void ChannelEditor::Probe()
{
    SendResult(2);
}

void ChannelEditor::SetText(const InfoMap &Map)
{
    if (Map.contains("callsign"))
        m_callsignEdit->SetText(Map.value("callsign"));
    if (Map.contains("channum"))
        m_channumEdit->SetText(Map.value("channum"));
    if (Map.contains("channame"))
        m_channameEdit->SetText(Map.value("channame"));
    if (Map.contains("XMLTV"))
        m_xmltvidEdit->SetText(Map.value("XMLTV"));
}

void ChannelEditor::GetText(InfoMap &Map)
{
    Map["callsign"] = m_callsignEdit->GetText();
    Map["channum"]  = m_channumEdit->GetText();
    Map["channame"] = m_channameEdit->GetText();
    Map["XMLTV"]    = m_xmltvidEdit->GetText();
}

bool ChannelEditor::keyPressEvent(QKeyEvent *Event)
{
    if (GetFocusWidget()->keyPressEvent(Event))
        return true;

    QStringList actions;
    bool handled = m_mainWindow->TranslateKeyPress("qt", Event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        if (action == "ESCAPE" )
        {
            SendResult(3);
            handled = true;
        }
    }

    if (!handled && MythScreenType::keyPressEvent(Event))
        handled = true;

    return handled;
}

void ChannelEditor::SendResult(int result)
{
    if (!m_tv)
        return;

    QString  message = "";
    switch (result)
    {
        case 1:
            message = "DIALOG_EDITOR_OK_0";
            break;
        case 2:
            message = "DIALOG_EDITOR_PROBE_0";
            break;
        case 3:
            message = "DIALOG_EDITOR_QUIT_0";
            break;
    }

    auto *dce = new DialogCompletionEvent("", result, "", message);
    QCoreApplication::postEvent(m_tv, dce);
}

OSD::OSD(MythMainWindow *MainWindow, TV *Tv, MythPlayerUI* Player, MythPainter* Painter)
  : MythCaptionsOverlay(MainWindow, Tv, Player, Painter)
{
    connect(this, &OSD::HideOSD, m_tv, &TV::HandleOSDClosed);
    connect(m_tv, &TV::ChangeOSDDialog, this, &OSD::ShowDialog);
    connect(m_tv, &TV::ChangeOSDText, this, &OSD::SetText);
}

OSD::~OSD()
{
    OSD::TearDown();
}

void OSD::TearDown()
{
    MythCaptionsOverlay::TearDown();
    m_dialog = nullptr;
}

bool OSD::Init(const QRect &Rect, float FontAspect)
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
        auto * win = new MythOSDWindow(nullptr, m_painter, window, true);
        if (win->Create())
        {
            PositionWindow(win);
            LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Loaded window %1").arg(window));
            m_children.insert(window, win);

            // Update player for window visibility
            if (window == OSD_WIN_BROWSE)
                connect(win, &MythOSDWindow::VisibilityChanged, m_player, &MythPlayerUI::BrowsingChanged);
            if (window == OSD_WIN_PROGEDIT)
                connect(win, &MythOSDWindow::VisibilityChanged, m_player, &MythPlayerUI::EditingChanged);
            if (window == OSD_WIN_DEBUG)
                connect(win, &MythOSDWindow::VisibilityChanged, m_player, &MythPlayerUI::OSDDebugVisibilityChanged);
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
        auto *edit = qobject_cast<ChannelEditor*>(m_dialog);
        if (edit)
            edit->SetText(Map);
        else
            win->SetTextFromMap(Map);
    }
    else
        win->SetTextFromMap(Map);

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

void OSD::SetGraph(const QString &Window, const QString &Graph, int64_t Timecode)
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

void OSD::Draw(const QRect &Rect)
{
    bool visible = false;
    QTime now = MythDate::current().time();

    CheckExpiry();
    for (auto * screen : qAsConst(m_children))
    {
        if (screen->IsVisible())
        {
            visible = true;
            screen->Pulse();
            if (m_expireTimes.contains(screen))
            {
                QTime expires = m_expireTimes.value(screen).time();
                int left = now.msecsTo(expires);
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
            int left = now.msecsTo(expires);
            if (left < 0)
                left = 0;
            if (expires.isValid() && left < m_fadeTime)
                (*it2)->SetAlpha((255 * left) / m_fadeTime);
        }
        ++it2;
    }
    RevertUIScale();

    if (visible)
    {
        m_painter->Begin(nullptr);
        for (auto * screen : qAsConst(m_children))
        {
            if (screen->IsVisible())
            {
                screen->Draw(m_painter, 0, 0, 255, Rect);
                screen->SetAlpha(255);
                screen->ResetNeedsRedraw();
            }
        }
        for (auto * notif : qAsConst(notifications))
        {
            if (notif->IsVisible())
            {
                notif->Draw(m_painter, 0, 0, 255, Rect);
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
                    int CustomTimeout)
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

void OSD::SetExpiryPriv(const QString &Window, enum OSDTimeout Timeout, int CustomTimeout)
{
    if (Timeout == kOSDTimeout_Ignore && !CustomTimeout)
        return;

    MythScreenType *win = GetWindow(Window);
    int time = CustomTimeout ? CustomTimeout : m_timeouts[static_cast<size_t>(Timeout)];
    if ((time > 0) && win)
    {
        QDateTime expires = MythDate::current().addMSecs(time);
            m_expireTimes.insert(win, expires);
    }
    else if ((time < 0) && win)
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

    MythCaptionsOverlay::HideWindow(Window);

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

void OSD::DialogShow(const QString &Window, const QString &Text, int UpdateFor)
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
            dialog = new ChannelEditor(m_mainWindow, m_tv, Window.toLatin1());
        else if (Window == OSD_DLG_CONFIRM)
            dialog = new MythConfirmationDialog(nullptr, Text, false);
        else if (Window == OSD_DLG_NAVIGATE)
            dialog = new OsdNavigation(m_mainWindow, m_tv, m_player, Window, this);
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

    if (UpdateFor)
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
        dialog->AddButton(Text, std::move(Data), Menu, Current);
}

void OSD::DialogGetText(InfoMap &Map)
{
    auto *edit = qobject_cast<ChannelEditor*>(m_dialog);
    if (edit)
        edit->GetText(Map);
}

OsdNavigation::OsdNavigation(MythMainWindow *MainWindow, TV* Tv, MythPlayerUI *Player, const QString& Name, OSD* Osd)
  : MythScreenType(static_cast<MythScreenType*>(nullptr), Name),
    m_mainWindow(MainWindow),
    m_tv(Tv),
    m_player(Player),
    m_osd(Osd)
{
}

bool OsdNavigation::Create()
{
    // Setup screen
    if (!XMLParseBase::LoadWindowFromXML("osd.xml", "osd_navigation", this))
        return false;

    MythUIButton* more = nullptr;
    UIUtilW::Assign(this, more, "more");
    if (more)
        connect(more, &MythUIButton::Clicked, this, &OsdNavigation::More);
    UIUtilW::Assign(this, m_pauseButton,  "PAUSE");
    UIUtilW::Assign(this, m_playButton,   "PLAY");
    UIUtilW::Assign(this, m_muteButton,   "MUTE");
    UIUtilW::Assign(this, m_unMuteButton, "unmute");

    // Listen for player state changes
    connect(m_player, &MythPlayerUI::PauseChanged,      this, &OsdNavigation::PauseChanged);
    connect(m_player, &MythPlayerUI::AudioStateChanged, this, &OsdNavigation::AudioStateChanged);

    // Set initial player state
    if (!(m_audioState.m_hasAudioOut && m_audioState.m_volumeControl))
    {
        if (m_muteButton)
            m_muteButton->Hide();
        if (m_unMuteButton)
            m_unMuteButton->Hide();
    }
    else
    {
        // Fudge to ensure we start with the correct mute state
        MythAudioState state = m_player->GetAudioState();
        m_audioState.m_muteState = VolumeBase::NextMuteState(state.m_muteState);
        AudioStateChanged(state);
    }

    bool paused = m_player->IsPaused();
    m_paused = !paused;
    PauseChanged(paused);

    // Find number of groups and make sure only corrrect one is visible
    MythUIGroup *group = nullptr;
    for (int i = 0; i < 100 ; i++)
    {
        UIUtilW::Assign(this, group, QString("grp%1").arg(i));
        if (!group)
            break;

        m_maxGroupNum = i;
        if (i != m_visibleGroup)
            group->SetVisible(false);
        QList<MythUIType *> * children = group->GetAllChildren();
        for (auto * child : *children)
        {
            if (child != more)
            {
                auto * button = dynamic_cast<MythUIButton*>(child);
                if (button)
                    connect(button, &MythUIButton::Clicked, this, &OsdNavigation::GeneralAction);
            }
        }
    }

    BuildFocusList();

    return true;
}

bool OsdNavigation::keyPressEvent(QKeyEvent* Event)
{
    bool extendtimeout = true;
    bool handled = false;

    MythUIType* current = GetFocusWidget();
    if (current && current->keyPressEvent(Event))
        handled = true;

    if (!handled)
    {
        QStringList actions;
        handled = m_mainWindow->TranslateKeyPress("qt", Event, actions);

        for (int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            if (action == "ESCAPE" )
            {
                SendResult(-1, action);
                handled = true;
                extendtimeout = false;
            }
        }
    }

    if (!handled && MythScreenType::keyPressEvent(Event))
        handled = true;

    if (extendtimeout)
        m_osd->SetExpiry(OSD_DLG_NAVIGATE, kOSDTimeout_Long);

    return handled;
}

void OsdNavigation::ShowMenu()
{
    SendResult(100, "MENU");
}

void OsdNavigation::SendResult(int Result, const QString& Action)
{
    auto * dce = new DialogCompletionEvent("", Result, "", Action);
    QCoreApplication::postEvent(m_tv, dce);
}

void OsdNavigation::PauseChanged(bool Paused)
{
    if (!(m_playButton && m_pauseButton))
        return;

    if (m_paused == Paused)
        return;

    m_paused = Paused;
    MythUIType* current = GetFocusWidget();
    m_playButton->SetVisible(m_paused);
    m_pauseButton->SetVisible(!m_paused);

    if (current && (current == m_playButton || current == m_pauseButton))
    {
        current->LoseFocus();
        SetFocusWidget(m_paused ? m_playButton : m_pauseButton);
    }
}

void OsdNavigation::AudioStateChanged(MythAudioState AudioState)
{   
    if (!(m_muteButton && m_unMuteButton))
        return;

    if (!(AudioState.m_volumeControl && AudioState.m_hasAudioOut))
    {
        m_muteButton->Hide();
        m_unMuteButton->Hide();
    }
    else if (m_audioState.m_muteState != AudioState.m_muteState)
    {
        bool muted = AudioState.m_muteState == kMuteAll;
        MythUIType* current = GetFocusWidget();
        m_muteButton->SetVisible(!muted);
        m_unMuteButton->SetVisible(muted);

        if (current && (current == m_muteButton || current == m_unMuteButton))
        {
            current->LoseFocus();
            SetFocusWidget(muted ? m_unMuteButton : m_muteButton);
        }
    }

    m_audioState = AudioState;
}

void OsdNavigation::GeneralAction()
{
    MythUIType* current = GetFocusWidget();
    if (current)
    {
        QString name = current->objectName();
        int result = 100;
        int hashPos = name.indexOf('#');
        if (hashPos > -1)
            name.truncate(hashPos);
        if (name == "INFO")
            result = 0;
        if (name == "unmute")
            name = "MUTE";
        SendResult(result, name);
    }
}

// Switch to next group of icons. They have to be
// named grp0, grp1, etc with no gaps in numbers.
void OsdNavigation::More()
{
    if (m_maxGroupNum <= 0)
        return;

    MythUIGroup* group = nullptr;
    UIUtilW::Assign(this, group, QString("grp%1").arg(m_visibleGroup));
    if (group != nullptr)
        group->SetVisible(false);

    // wrap around after last group displayed
    if (++m_visibleGroup > m_maxGroupNum)
        m_visibleGroup = 0;

    UIUtilW::Assign(this, group, QString("grp%1").arg(m_visibleGroup));
    if (group != nullptr)
        group->SetVisible(true);
}
