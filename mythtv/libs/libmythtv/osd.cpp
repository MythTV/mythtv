// Qt
#include <QCoreApplication>
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
#include "teletextscreen.h"
#include "subtitlescreen.h"
#include "interactivescreen.h"
#include "osd.h"
#include "Bluray/bdringbuffer.h"
#include "Bluray/bdoverlayscreen.h"
#include "tv_actions.h"

#define LOC     QString("OSD: ")

QEvent::Type OSDHideEvent::kEventType = static_cast<QEvent::Type>(QEvent::registerEventType());

bool ChannelEditor::Create(void)
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
    connect(okButton,    SIGNAL(Clicked()), SLOT(Confirm()));
    connect(probeButton, SIGNAL(Clicked()), SLOT(Probe()));
    SetFocusWidget(okButton);

    return true;
}

void ChannelEditor::Confirm(void)
{
    SendResult(1);
}

void ChannelEditor::Probe(void)
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
    bool handled = GetMythMainWindow()->TranslateKeyPress("qt", Event, actions);

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
    if (!m_retObject)
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
    QCoreApplication::postEvent(m_retObject, dce);
}

OSD::~OSD()
{
    TearDown();
}

void OSD::TearDown(void)
{
    foreach(MythScreenType* screen, m_Children)
        delete screen;
    m_Children.clear();
    m_Dialog = nullptr;
}

bool OSD::Init(const QRect &Rect, float FontAspect)
{
    m_Rect = Rect;
    m_fontStretch = static_cast<int>(lroundf(FontAspect * 100));
    OverrideUIScale();
    LoadWindows();
    RevertUIScale();

    if (m_Children.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to load any windows.");
        return false;
    }

    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("Loaded OSD: size %1x%2 offset %3+%4")
            .arg(m_Rect.width()).arg(m_Rect.height())
            .arg(m_Rect.left()).arg(m_Rect.top()));
    HideAll(false);
    return true;
}

void OSD::SetPainter(MythPainter *Painter)
{
    if (Painter == m_CurrentPainter)
        return;

    m_CurrentPainter = Painter;
    QMapIterator<QString, MythScreenType*> it(m_Children);
    while (it.hasNext())
    {
        it.next();
        it.value()->SetPainter(m_CurrentPainter);
    }
}

void OSD::OverrideUIScale(bool Log)
{
    QRect uirect = GetMythMainWindow()->GetUIScreenRect();
    if (uirect == m_Rect)
        return;

    m_savedFontStretch = GetMythUI()->GetFontStretch();
    GetMythUI()->SetFontStretch(m_fontStretch);

    int width;
    int height;
    GetMythUI()->GetScreenSettings(width, m_SavedWMult, height, m_SavedHMult);
    QSize theme_size = GetMythUI()->GetBaseSize();
    m_SavedUIRect = uirect;
    float tmp_wmult = static_cast<float>(m_Rect.size().width()) / static_cast<float>(theme_size.width());
    float tmp_hmult = static_cast<float>(m_Rect.size().height()) / static_cast<float>(theme_size.height());
    if (Log)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Base theme size: %1x%2")
            .arg(theme_size.width()).arg(theme_size.height()));
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Scaling factors: %1x%2")
            .arg(static_cast<double>(tmp_wmult)).arg(static_cast<double>(tmp_hmult)));
    }
    m_UIScaleOverride = true;
    GetMythMainWindow()->SetScalingFactors(tmp_wmult, tmp_hmult);
    GetMythMainWindow()->SetUIScreenRect(m_Rect);
}

void OSD::RevertUIScale(void)
{
    if (m_UIScaleOverride)
    {
        GetMythUI()->SetFontStretch(m_savedFontStretch);
        GetMythMainWindow()->SetScalingFactors(m_SavedWMult, m_SavedHMult);
        GetMythMainWindow()->SetUIScreenRect(m_SavedUIRect);
    }
    m_UIScaleOverride = false;
}

bool OSD::Reinit(const QRect &Rect, float FontAspect)
{
    m_Refresh = true;
    int new_stretch = static_cast<int>(lroundf(FontAspect * 100));
    if ((Rect == m_Rect) && (new_stretch == m_fontStretch))
        return true;
    if (m_Dialog && m_Dialog->objectName() == OSD_DLG_NAVIGATE
        && m_Dialog->IsVisible())
    {
        return true;
    }

    HideAll(false);
    TearDown();
    if (!Init(Rect, FontAspect))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Failed to re-init OSD."));
        return false;
    }
    return true;
}

bool OSD::IsVisible(void)
{
    if (GetNotificationCenter()->DisplayedNotifications() > 0)
        return true;

    foreach(MythScreenType* child, m_Children)
    {
        if (child->IsVisible() &&
            child->objectName() != OSD_WIN_SUBTITLE &&
            child->objectName() != OSD_WIN_TELETEXT &&
            child->objectName() != OSD_WIN_BDOVERLAY &&
            child->objectName() != OSD_WIN_INTERACT)
            return true;
    }

    return false;
}

void OSD::HideAll(bool KeepSubs, MythScreenType* Except, bool DropNotification)
{
    if (DropNotification)
    {
        if (GetNotificationCenter()->RemoveFirst())
            return; // we've removed the top window, don't process any further
    }
    QMutableMapIterator<QString, MythScreenType*> it(m_Children);
    while (it.hasNext())
    {
        it.next();
        if (Except && Except->objectName() == OSD_DLG_NAVIGATE
            && it.value()->objectName() == "osd_status")
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

void OSD::LoadWindows(void)
{
    static const char* s_defaultWindows[7] = {
        "osd_message", "osd_input", "program_info", "browse_info", "osd_status",
        "osd_program_editor", "osd_debug"};

    for (int i = 0; i < 7; i++)
    {
        const char* window = s_defaultWindows[i];
        auto *win = new MythOSDWindow(nullptr, window, true);

        win->SetPainter(m_CurrentPainter);
        if (win->Create())
        {
            PositionWindow(win);
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                QString("Loaded window %1").arg(window));
            m_Children.insert(window, win);
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("Failed to load window %1")
                .arg(window));
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
            edit->SetEditPosition(static_cast<long long>(Map.value("position")));
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
#if QT_VERSION < QT_VERSION_CHECK(5,8,0)
        int startts = Map["startts"].toInt();
        int endts   = Map["endts"].toInt();
        int nowts   = MythDate::current().toTime_t();
#else
        qint64 startts = Map["startts"].toLongLong();
        qint64 endts   = Map["endts"].toLongLong();
        qint64 nowts   = MythDate::current().toSecsSinceEpoch();
#endif
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
#if QT_VERSION < QT_VERSION_CHECK(5,8,0)
            int duration = endts - startts;
#else
            qint64 duration = endts - startts;
#endif
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

    if (win == m_Dialog)
    {
        auto *edit = dynamic_cast<ChannelEditor*>(m_Dialog);
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

    MythImage* mi = m_parent->GetAudioGraph().GetImage(Timecode);
    if (mi)
        image->SetImage(mi);
}

bool OSD::Draw(MythPainter* Painter, QSize Size, bool Repaint)
{
    if (!Painter)
        return false;

    bool visible = false;
    bool redraw  = m_Refresh;
    m_Refresh    = false;
    QTime now = MythDate::current().time();

    CheckExpiry();
    QMap<QString,MythScreenType*>::const_iterator it;
    for (it = m_Children.begin(); it != m_Children.end(); ++it)
    {
        if ((*it)->IsVisible())
        {
            visible = true;
            (*it)->Pulse();
            if (m_ExpireTimes.contains((*it)))
            {
                QTime expires = m_ExpireTimes.value((*it)).time();
                int left = now.msecsTo(expires);
                if (left < m_FadeTime)
                    (*it)->SetAlpha((255 * left) / m_FadeTime);
            }
            if ((*it)->NeedsRedraw())
                redraw = true;
        }
    }

    MythNotificationCenter *nc = GetNotificationCenter();
    QList<MythScreenType*> notifications;
    nc->GetNotificationScreens(notifications);
    QList<MythScreenType*>::iterator it2 = notifications.begin();
    while (it2 != notifications.end())
    {
        if (!MythNotificationCenter::ScreenCreated(*it2))
        {
            LOG(VB_GUI, LOG_DEBUG, LOC + "Creating OSD Notification");

            if (!m_UIScaleOverride)
            {
                OverrideUIScale(false);
            }
            (*it2)->SetPainter(m_CurrentPainter);
            if (!(*it2)->Create())
            {
                it2 = notifications.erase(it2);
                continue;
            }
        }
        if ((*it2)->IsVisible())
        {
            if (!m_UIScaleOverride)
            {
                OverrideUIScale(false);
            }

            (*it2)->SetPainter(m_CurrentPainter);

            MythNotificationCenter::UpdateScreen(*it2);

            visible = true;
            (*it2)->Pulse();
            QTime expires = MythNotificationCenter::ScreenExpiryTime(*it2).time();
            int left = now.msecsTo(expires);
            if (left < 0)
                left = 0;
            if (expires.isValid() && left < m_FadeTime)
                (*it2)->SetAlpha((255 * left) / m_FadeTime);
            if ((*it2)->NeedsRedraw())
                redraw = true;
        }
        ++it2;
    }
    RevertUIScale();

    redraw |= Repaint;

    if (redraw && visible)
    {
        QRect cliprect = QRect(QPoint(0, 0), Size);
        Painter->Begin(nullptr);
        for (it = m_Children.begin(); it != m_Children.end(); ++it)
        {
            if ((*it)->IsVisible())
            {
                (*it)->Draw(Painter, 0, 0, 255, cliprect);
                (*it)->SetAlpha(255);
                (*it)->ResetNeedsRedraw();
            }
        }
        for (it2 = notifications.begin(); it2 != notifications.end(); ++it2)
        {
            if ((*it2)->IsVisible())
            {
                (*it2)->Draw(Painter, 0, 0, 255, cliprect);
                (*it2)->SetAlpha(255);
                (*it2)->ResetNeedsRedraw();
            }
        }
        Painter->End();
    }

    // Force a redraw if it just became invisible
    if (m_Visible && !visible)
        redraw=true;
    m_Visible = visible;

    return redraw;
}

void OSD::CheckExpiry(void)
{
    QDateTime now = MythDate::current();
    QMutableHashIterator<MythScreenType*, QDateTime> it(m_ExpireTimes);
    while (it.hasNext())
    {
        it.next();
        if (it.value() < now)
        {
            if (it.key() == m_Dialog)
                DialogQuit();
            else
                HideWindow(m_Children.key(it.key()));
        }
        else if (it.key() == m_Dialog)
        {
            if (!m_PulsedDialogText.isEmpty() && now > m_NextPulseUpdate)
            {
                QString newtext = m_PulsedDialogText;
                auto *dialog = dynamic_cast<MythDialogBox*>(m_Dialog);
                if (dialog)
                {
                    // The disambiguation string must be an empty string
                    // and not a NULL to get extracted by the Qt tools.
                    QString replace = QCoreApplication::translate("(Common)",
                                          "%n second(s)", "",
                                          static_cast<int>(now.secsTo(it.value())));
                    dialog->SetText(newtext.replace("%d", replace));
                }
                auto *cdialog = dynamic_cast<MythConfirmationDialog*>(m_Dialog);
                if (cdialog)
                {
                    QString replace = QString::number(now.secsTo(it.value()));
                    cdialog->SetMessage(newtext.replace("%d", replace));
                }
                m_NextPulseUpdate = now.addSecs(1);
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
            SetExpiryPriv("osd_status", Timeout, CustomTimeout);
        else if (Window == "osd_status" && IsWindowVisible(OSD_DLG_NAVIGATE))
            SetExpiryPriv(OSD_DLG_NAVIGATE, Timeout, CustomTimeout);
    }
}

void OSD::SetExpiryPriv(const QString &Window, enum OSDTimeout Timeout, int CustomTimeout)
{
    if (Timeout == kOSDTimeout_Ignore && !CustomTimeout)
        return;

    MythScreenType *win = GetWindow(Window);
    int time = CustomTimeout ? CustomTimeout : m_Timeouts[Timeout];
    if ((time > 0) && win)
    {
        QDateTime expires = MythDate::current().addMSecs(time);
            m_ExpireTimes.insert(win, expires);
    }
    else if ((time < 0) && win)
    {
        if (m_ExpireTimes.contains(win))
            m_ExpireTimes.remove(win);
    }
}

void OSD::SetTimeouts(int Short, int Medium, int Long)
{
    m_Timeouts[kOSDTimeout_None]  = -1;
    m_Timeouts[kOSDTimeout_Short] = Short;
    m_Timeouts[kOSDTimeout_Med]   = Medium;
    m_Timeouts[kOSDTimeout_Long]  = Long;
}

bool OSD::IsWindowVisible(const QString &Window)
{
    if (!m_Children.contains(Window))
        return false;

    return m_Children.value(Window)->IsVisible(/*true*/);
}

void OSD::ResetWindow(const QString &Window)
{
    if (!m_Children.contains(Window))
        return;

    m_Children.value(Window)->Reset();
}

void OSD::PositionWindow(MythScreenType *Window)
{
    if (!Window)
        return;

    MythRect rect = Window->GetArea();
    rect.translate(m_Rect.left(), m_Rect.top());
    Window->SetArea(rect);
}

void OSD::RemoveWindow(const QString &Window)
{
    if (!m_Children.contains(Window))
        return;

    HideWindow(Window);
    MythScreenType *child = m_Children.value(Window);
    m_Children.remove(Window);
    delete child;
}

MythScreenType *OSD::GetWindow(const QString &Window)
{
    if (m_Children.contains(Window))
        return m_Children.value(Window);

    MythScreenType *new_window = nullptr;

    if (Window == OSD_WIN_INTERACT)
    {
        new_window = new InteractiveScreen(m_parent, Window);
    }
    else if (Window == OSD_WIN_BDOVERLAY)
    {
        new_window = new BDOverlayScreen(m_parent, Window);
    }
    else
    {
        new_window = new MythOSDWindow(nullptr, Window, false);
    }

    new_window->SetPainter(m_CurrentPainter);
    if (new_window->Create())
    {
        m_Children.insert(Window, new_window);
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("Created window %1").arg(Window));
        return new_window;
    }

    LOG(VB_GENERAL, LOG_ERR, LOC + QString("Failed to create window %1")
            .arg(Window));
    delete new_window;
    return nullptr;
}

void OSD::SetFunctionalWindow(const QString &window, enum OSDFunctionalType Type)
{
    if (m_FunctionalType != kOSDFunctionalType_Default &&
        m_FunctionalType != Type)
        SendHideEvent();

    m_FunctionalWindow = window;
    m_FunctionalType   = Type;
}

void OSD::HideWindow(const QString &Window)
{
    if (!m_Children.contains(Window))
        return;
    m_Children.value(Window)->SetVisible(false);
    m_Children.value(Window)->Close(); // for InteractiveScreen
    SetExpiry(Window, kOSDTimeout_None);
    m_Refresh = true;

    if (m_FunctionalType != kOSDFunctionalType_Default)
    {
        bool valid   = m_Children.contains(m_FunctionalWindow);
        bool visible = valid && m_Children.value(m_FunctionalWindow)->IsVisible(false);
        if (!valid || !visible)
        {
            SendHideEvent();
            m_FunctionalType = kOSDFunctionalType_Default;
            m_FunctionalWindow = QString();
        }
    }
}

void OSD::SendHideEvent(void)
{
    auto *event = new OSDHideEvent(m_FunctionalType);
    QCoreApplication::postEvent(m_ParentObject, event);
}

bool OSD::HasWindow(const QString &Window)
{
    return m_Children.contains(Window);
}

bool OSD::DialogVisible(const QString& Window)
{
    if (!m_Dialog || Window.isEmpty())
        return m_Dialog;
    return m_Dialog->objectName() == Window;
}

bool OSD::DialogHandleKeypress(QKeyEvent *Event)
{
    if (!m_Dialog)
        return false;
    return m_Dialog->keyPressEvent(Event);
}

bool OSD::DialogHandleGesture(MythGestureEvent *Event)
{
    if (!m_Dialog)
        return false;
    return m_Dialog->gestureEvent(Event);
}

void OSD::DialogQuit(void)
{
    if (!m_Dialog)
        return;

    RemoveWindow(m_Dialog->objectName());
    m_Dialog = nullptr;
    m_PulsedDialogText = QString();
}

void OSD::DialogShow(const QString &Window, const QString &Text, int UpdateFor)
{
    if (m_Dialog)
    {
        QString current = m_Dialog->objectName();
        if (current != Window)
        {
            DialogQuit();
        }
        else
        {
            auto *dialog = dynamic_cast<MythDialogBox*>(m_Dialog);
            if (dialog)
                dialog->Reset();

            DialogSetText(Text);
        }
    }

    if (!m_Dialog)
    {
        OverrideUIScale();
        MythScreenType *dialog;

        if (Window == OSD_DLG_EDITOR)
            dialog = new ChannelEditor(m_ParentObject, Window.toLatin1());
        else if (Window == OSD_DLG_CONFIRM)
            dialog = new MythConfirmationDialog(nullptr, Text, false);
        else if (Window == OSD_DLG_NAVIGATE)
            dialog = new OsdNavigation(m_ParentObject, Window, this);
        else
            dialog = new MythDialogBox(Text, nullptr, Window.toLatin1(), false, true);

        dialog->SetPainter(m_CurrentPainter);
        if (dialog->Create())
        {
            PositionWindow(dialog);
            m_Dialog = dialog;
            auto *dbox = dynamic_cast<MythDialogBox*>(m_Dialog);
            if (dbox)
                dbox->SetReturnEvent(m_ParentObject, Window);
            auto *cbox = dynamic_cast<MythConfirmationDialog*>(m_Dialog);
            if (cbox)
            {
                cbox->SetReturnEvent(m_ParentObject, Window);
                cbox->SetData("DIALOG_CONFIRM_X_X");
            }
            m_Children.insert(Window, m_Dialog);
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
        m_NextPulseUpdate  = MythDate::current();
        m_PulsedDialogText = Text;
        SetExpiry(Window, kOSDTimeout_None, UpdateFor);
    }

    DialogBack();
    HideAll(true, m_Dialog);
    m_Dialog->SetVisible(true);
}

void OSD::DialogSetText(const QString &Text)
{
    auto *dialog = dynamic_cast<MythDialogBox*>(m_Dialog);
    if (dialog)
        dialog->SetText(Text);
}

void OSD::DialogBack(const QString& Text, const QVariant& Data, bool Exit)
{
    auto *dialog = dynamic_cast<MythDialogBox*>(m_Dialog);
    if (dialog)
    {
        dialog->SetBackAction(Text, Data);
        if (Exit)
            dialog->SetExitAction(Text, Data);
    }
}

void OSD::DialogAddButton(const QString& Text, QVariant Data, bool Menu, bool Current)
{
    auto *dialog = dynamic_cast<MythDialogBox*>(m_Dialog);
    if (dialog)
        dialog->AddButton(Text, std::move(Data), Menu, Current);
}

void OSD::DialogGetText(InfoMap &Map)
{
    auto *edit = dynamic_cast<ChannelEditor*>(m_Dialog);
    if (edit)
        edit->GetText(Map);
}

TeletextScreen* OSD::InitTeletext(void)
{
    TeletextScreen *tt = nullptr;
    if (m_Children.contains(OSD_WIN_TELETEXT))
    {
        tt = dynamic_cast<TeletextScreen*>(m_Children.value(OSD_WIN_TELETEXT));
    }
    else
    {
        OverrideUIScale();
        tt = new TeletextScreen(m_parent, OSD_WIN_TELETEXT, m_fontStretch);

        tt->SetPainter(m_CurrentPainter);
        if (tt->Create())
        {
            m_Children.insert(OSD_WIN_TELETEXT, tt);
            LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Created window %1")
                .arg(OSD_WIN_TELETEXT));
        }
        else
        {
            delete tt;
            tt = nullptr;
        }
        RevertUIScale();
    }
    if (!tt)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create Teletext window");
        return nullptr;
    }

    HideWindow(OSD_WIN_TELETEXT);
    tt->SetDisplaying(false);
    return tt;
}

void OSD::EnableTeletext(bool Enable, int Page)
{
    TeletextScreen *tt = InitTeletext();
    if (!tt)
        return;

    tt->SetVisible(Enable);
    tt->SetDisplaying(Enable);
    if (Enable)
    {
        tt->SetPage(Page, -1);
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Enabled teletext page %1")
                                   .arg(Page));
    }
    else
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Disabled teletext");
}

bool OSD::TeletextAction(const QString &Action)
{
    if (!HasWindow(OSD_WIN_TELETEXT))
        return false;

    TeletextScreen* tt = dynamic_cast<TeletextScreen*>(m_Children.value(OSD_WIN_TELETEXT));
    if (tt)
        return tt->KeyPress(Action);
    return false;
}

void OSD::TeletextReset(void)
{
    if (!HasWindow(OSD_WIN_TELETEXT))
        return;

    TeletextScreen* tt = InitTeletext();
    if (tt)
        tt->Reset();
}

void OSD::TeletextClear(void)
{
    if (!HasWindow(OSD_WIN_TELETEXT))
        return;

    TeletextScreen* tt = dynamic_cast<TeletextScreen*>(m_Children.value(OSD_WIN_TELETEXT));
    if (tt)
        tt->ClearScreen();
}

SubtitleScreen* OSD::InitSubtitles(void)
{
    SubtitleScreen *sub = nullptr;
    if (m_Children.contains(OSD_WIN_SUBTITLE))
    {
        sub = dynamic_cast<SubtitleScreen*>(m_Children.value(OSD_WIN_SUBTITLE));
    }
    else
    {
        OverrideUIScale();
        sub = new SubtitleScreen(m_parent, OSD_WIN_SUBTITLE, m_fontStretch);
        sub->SetPainter(m_CurrentPainter);
        if (sub->Create())
        {
            m_Children.insert(OSD_WIN_SUBTITLE, sub);
            LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Created window %1")
                .arg(OSD_WIN_SUBTITLE));
        }
        else
        {
            delete sub;
            sub = nullptr;
        }
        RevertUIScale();
    }
    if (!sub)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create subtitle window");
        return nullptr;
    }
    return sub;
}

void OSD::EnableSubtitles(int Type, bool ForcedOnly)
{
    SubtitleScreen *sub = InitSubtitles();
    if (sub)
        sub->EnableSubtitles(Type, ForcedOnly);
}

void OSD::DisableForcedSubtitles(void)
{
    if (!HasWindow(OSD_WIN_SUBTITLE))
        return;

    SubtitleScreen *sub = InitSubtitles();
    sub->DisableForcedSubtitles();
}

void OSD::ClearSubtitles(void)
{
    if (!HasWindow(OSD_WIN_SUBTITLE))
        return;

    SubtitleScreen* sub = InitSubtitles();
    if (sub)
        sub->ClearAllSubtitles();
}

void OSD::DisplayDVDButton(AVSubtitle* DVDButton, QRect &Pos)
{
    if (!DVDButton)
        return;

    SubtitleScreen* sub = InitSubtitles();
    if (sub)
    {
        EnableSubtitles(kDisplayDVDButton);
        sub->DisplayDVDButton(DVDButton, Pos);
    }
}

void OSD::DisplayBDOverlay(BDOverlay* Overlay)
{
    if (!Overlay)
        return;

    BDOverlayScreen* bd = dynamic_cast<BDOverlayScreen*>(GetWindow(OSD_WIN_BDOVERLAY));
    if (bd)
        bd->DisplayBDOverlay(Overlay);
}

bool OsdNavigation::Create(void)
{
    if (!XMLParseBase::LoadWindowFromXML("osd.xml", "osd_navigation", this))
        return false;

    MythUIButton *moreButton;
    UIUtilW::Assign(this, moreButton, "more");
    if (moreButton)
        connect(moreButton, SIGNAL(Clicked()), SLOT(More()));
    UIUtilW::Assign(this, m_pauseButton, "PAUSE");
    UIUtilW::Assign(this, m_playButton, "PLAY");
    UIUtilW::Assign(this, m_muteButton, "MUTE");
    UIUtilW::Assign(this, m_unMuteButton, "unmute");

    MythPlayer *player = m_osd->GetPlayer();

    if (!player || !player->HasAudioOut() ||
        !player->PlayerControlsVolume())
    {
        m_IsVolumeControl = false;
        if (m_muteButton)
            m_muteButton->Hide();
        if (m_unMuteButton)
            m_unMuteButton->Hide();
    }

    // find number of groups and make sure only corrrect one is visible
    MythUIGroup *group;
    for (int i = 0; i < 100 ; i++)
    {
        UIUtilW::Assign(this, group, QString("grp%1").arg(i));
        if (group)
        {
            m_maxGroupNum = i;
            if (i != m_visibleGroup)
                group->SetVisible (false);
            QList<MythUIType *> * children = group->GetAllChildren();
            QList<MythUIType *>::iterator it;
            for (it = children->begin(); it != children->end(); ++it)
            {
                MythUIType *child = *it;
                if (child == moreButton)
                    continue;
                connect(child, SIGNAL(Clicked()), SLOT(GeneralAction()));
            }
        }
        else
            break;
    }

    BuildFocusList();

    return true;
}

bool OsdNavigation::keyPressEvent(QKeyEvent *Event)
{
    // bool extendTimeout = (m_paused != 'Y');
    bool extendTimeout = true;
    bool handled = false;

    MythUIType *focus = GetFocusWidget();
    if (focus && focus->keyPressEvent(Event))
        handled = true;

    if (!handled)
    {
        QStringList actions;
        handled = GetMythMainWindow()->TranslateKeyPress("qt", Event, actions);

        for (int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            if (action == "ESCAPE" )
            {
                SendResult(-1,action);
                handled = true;
                extendTimeout = false;
            }
        }
    }
    if (!handled && MythScreenType::keyPressEvent(Event))
        handled = true;

    if (extendTimeout)
    {
        m_osd->SetExpiry(OSD_DLG_NAVIGATE, kOSDTimeout_Long);
        // m_osd->SetExpiry("osd_status", kOSDTimeout_Long);
    }

    return handled;
}

// Virtual
void OsdNavigation::ShowMenu(void)
{
    SendResult(100,"MENU");
}

void OsdNavigation::SendResult(int Result, const QString& Action)
{
    if (!m_retObject)
        return;

    auto *dce = new DialogCompletionEvent("", Result, "", Action);
    QCoreApplication::postEvent(m_retObject, dce);
}

void OsdNavigation::GeneralAction(void)
{
    MythUIType *fw = GetFocusWidget();
    if (fw)
    {
        QString nameClicked = fw->objectName();
        int result = 100;
        int hashPos = nameClicked.indexOf('#');
        if (hashPos > -1)
            nameClicked.truncate(hashPos);
        if (nameClicked == "INFO")
            result=0;
        if (nameClicked == "unmute")
            nameClicked = "MUTE";
        SendResult(result, nameClicked);
    }
}

// Switch to next group of icons. They have to be
// named grp0, grp1, etc with no gaps in numbers.
void OsdNavigation::More(void)
{
    if (m_maxGroupNum <= 0)
        return;

    MythUIGroup *group;
    UIUtilW::Assign(this, group, QString("grp%1").arg(m_visibleGroup));
    group->SetVisible (false);

    // wrap around after last group displayed
    if (++m_visibleGroup > m_maxGroupNum)
        m_visibleGroup = 0;

    UIUtilW::Assign(this, group, QString("grp%1").arg(m_visibleGroup));
    group->SetVisible (true);
}

void OsdNavigation::SetTextFromMap(const InfoMap &Map)
{

    char paused = Map.value("paused", "X").toLocal8Bit().at(0);
    if (paused != 'X')
    {
        if (m_playButton && m_pauseButton && paused != m_paused)
        {
            MythUIType *fw = GetFocusWidget();
            m_playButton->SetVisible(paused=='Y');
            m_pauseButton->SetVisible(paused!='Y');
            if (fw && (fw == m_playButton || fw == m_pauseButton))
            {
                fw->LoseFocus();
                MythUIType *newfw = (paused=='Y' ? m_playButton : m_pauseButton);
                SetFocusWidget(newfw);
                if (m_paused == 'X')
                     newfw->TakeFocus();
            }
            m_paused = paused;
        }
    }

    char muted = Map.value("muted","X").toLocal8Bit().at(0);
    if (m_IsVolumeControl && muted != 'X')
    {
        if (m_muteButton && m_unMuteButton && muted != m_muted)
        {
            MythUIType *fw = GetFocusWidget();
            m_muteButton->SetVisible(muted!='Y');
            m_unMuteButton->SetVisible(muted=='Y');
            m_muted = muted;
            if (fw && (fw == m_muteButton || fw == m_unMuteButton))
            {
                fw->LoseFocus();
                SetFocusWidget(muted=='Y' ? m_unMuteButton : m_muteButton);
            }
        }
    }

    MythScreenType::SetTextFromMap(Map);
}
