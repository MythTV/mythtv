// Qt
#include <QCoreApplication>

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
#include "mythuinotificationcenter.h"

// libmythtv
#include "channelutil.h"
#include "teletextscreen.h"
#include "subtitlescreen.h"
#include "interactivescreen.h"
#include "osd.h"
#include "Bluray/bdringbuffer.h"
#include "Bluray/bdoverlayscreen.h"

#define LOC     QString("OSD: ")

QEvent::Type OSDHideEvent::kEventType =
    (QEvent::Type) QEvent::registerEventType();

ChannelEditor::ChannelEditor(QObject *retobject, const char *name)
  : MythScreenType((MythScreenType*)NULL, name)
{
    m_retObject    = retobject;
    m_callsignEdit = NULL;
    m_channumEdit  = NULL;
    m_channameEdit = NULL;
    m_xmltvidEdit  = NULL;
}

bool ChannelEditor::Create(void)
{
    if (!XMLParseBase::LoadWindowFromXML("osd.xml", "ChannelEditor", this))
        return false;

    MythUIButton *probeButton = NULL;
    MythUIButton *okButton    = NULL;

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
    sendResult(1);
}

void ChannelEditor::Probe(void)
{
    sendResult(2);
}

void ChannelEditor::SetText(QHash<QString,QString>&map)
{
    if (map.contains("callsign"))
        m_callsignEdit->SetText(map.value("callsign"));
    if (map.contains("channum"))
        m_channumEdit->SetText(map.value("channum"));
    if (map.contains("channame"))
        m_channameEdit->SetText(map.value("channame"));
    if (map.contains("XMLTV"))
        m_xmltvidEdit->SetText(map.value("XMLTV"));
}

void ChannelEditor::GetText(QHash<QString,QString>&map)
{
    map["callsign"] = m_callsignEdit->GetText();
    map["channum"]  = m_channumEdit->GetText();
    map["channame"] = m_channameEdit->GetText();
    map["XMLTV"]    = m_xmltvidEdit->GetText();
}

bool ChannelEditor::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    QStringList actions;
    bool handled = GetMythMainWindow()->TranslateKeyPress("qt", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        if (action == "ESCAPE" )
        {
            sendResult(3);
            handled = true;
        }
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void ChannelEditor::sendResult(int result)
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

    DialogCompletionEvent *dce = new DialogCompletionEvent("", result,
                                                           "", message);
    QCoreApplication::postEvent(m_retObject, dce);
}

OSD::OSD(MythPlayer *player, QObject *parent, MythPainter *painter)
  : m_parent(player), m_ParentObject(parent), m_CurrentPainter(painter),
    m_Rect(QRect()), m_Effects(true), m_FadeTime(kOSDFadeTime), m_Dialog(NULL),
    m_PulsedDialogText(QString()), m_NextPulseUpdate(QDateTime()),
    m_Refresh(false),   m_UIScaleOverride(false),
    m_SavedWMult(1.0f), m_SavedHMult(1.0f),   m_SavedUIRect(QRect()),
    m_fontStretch(100), m_savedFontStretch(100),
    m_FunctionalType(kOSDFunctionalType_Default), m_FunctionalWindow(QString())
{
    SetTimeouts(3000, 5000, 10000);
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
    m_Dialog = NULL;
}

bool OSD::Init(const QRect &rect, float font_aspect)
{
    m_Rect = rect;
    m_fontStretch = (int)((font_aspect * 100) + 0.5f);
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

void OSD::SetPainter(MythPainter *painter)
{
    if (painter == m_CurrentPainter)
        return;

    m_CurrentPainter = painter;
    QMapIterator<QString, MythScreenType*> it(m_Children);
    while (it.hasNext())
    {
        it.next();
        it.value()->SetPainter(m_CurrentPainter);
    }
}

void OSD::OverrideUIScale(bool log)
{
    QRect uirect = GetMythMainWindow()->GetUIScreenRect();
    if (uirect == m_Rect)
        return;

    m_savedFontStretch = GetMythUI()->GetFontStretch();
    GetMythUI()->SetFontStretch(m_fontStretch);

    int width, height;
    MythUIHelper::getMythUI()->GetScreenSettings(width,  m_SavedWMult,
                                                 height, m_SavedHMult);
    QSize theme_size = MythUIHelper::getMythUI()->GetBaseSize();
    m_SavedUIRect = uirect;
    float tmp_wmult = (float)m_Rect.size().width() / (float)theme_size.width();
    float tmp_hmult = (float)m_Rect.size().height() /
                      (float)theme_size.height();
    if (log)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Base theme size: %1x%2")
            .arg(theme_size.width()).arg(theme_size.height()));
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Scaling factors: %1x%2")
            .arg(tmp_wmult).arg(tmp_hmult));
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

bool OSD::Reinit(const QRect &rect, float font_aspect)
{
    m_Refresh = true;
    int new_stretch = (int)((font_aspect * 100) + 0.5f);
    if ((rect == m_Rect) && (new_stretch == m_fontStretch))
        return true;

    HideAll(false);
    TearDown();
    if (!Init(rect, font_aspect))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Failed to re-init OSD."));
        return false;
    }
    return true;
}

bool OSD::IsVisible(void)
{
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

void OSD::HideAll(bool keepsubs, MythScreenType* except)
{
    QMutableMapIterator<QString, MythScreenType*> it(m_Children);
    while (it.hasNext())
    {
        it.next();
        bool match1 = keepsubs &&
                     (it.key() == OSD_WIN_SUBTITLE  ||
                      it.key() == OSD_WIN_TELETEXT);
        bool match2 = it.key() == OSD_WIN_BDOVERLAY ||
                      it.key() == OSD_WIN_INTERACT  ||
                      it.value() == except;
        if (!(match1 || match2))
            HideWindow(it.key());
    }
}

void OSD::LoadWindows(void)
{
    static const char* default_windows[7] = {
        "osd_message", "osd_input", "program_info", "browse_info", "osd_status",
        "osd_program_editor", "osd_debug"};

    for (int i = 0; i < 7; i++)
    {
        const char* window = default_windows[i];
        MythOSDWindow *win = new MythOSDWindow(NULL, window, true);

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

void OSD::SetValues(const QString &window, QHash<QString,int> &map,
                    OSDTimeout timeout)
{
    MythScreenType *win = GetWindow(window);
    if (!win)
        return;

    bool found = false;
    if (map.contains("position"))
    {
        MythUIProgressBar *bar = dynamic_cast<MythUIProgressBar *> (win->GetChild("position"));
        if (bar)
        {
            bar->SetVisible(true);
            bar->SetStart(0);
            bar->SetTotal(1000);
            bar->SetUsed(map.value("position"));
            found = true;
        }
    }
    if (map.contains("relposition"))
    {
        MythUIProgressBar *bar = dynamic_cast<MythUIProgressBar *> (win->GetChild("relposition"));
        if (bar)
        {
            bar->SetVisible(true);
            bar->SetStart(0);
            bar->SetTotal(1000);
            bar->SetUsed(map.value("relposition"));
            found = true;
        }
    }

    if (found)
        SetExpiry(window, timeout);
}

void OSD::SetValues(const QString &window, QHash<QString,float> &map,
                    OSDTimeout timeout)
{
    MythScreenType *win = GetWindow(window);
    if (!win)
        return;

    bool found = false;
    if (map.contains("position"))
    {
        MythUIEditBar *edit = dynamic_cast<MythUIEditBar *> (win->GetChild("editbar"));
        if (edit)
            edit->SetEditPosition(map.value("position"));
    }

    if (found)
        SetExpiry(window, timeout);
}

void OSD::SetText(const QString &window, QHash<QString,QString> &map,
                  OSDTimeout timeout)
{
    MythScreenType *win = GetWindow(window);
    if (!win)
        return;

    if (map.contains("numstars"))
    {
        MythUIStateType *state = dynamic_cast<MythUIStateType *> (win->GetChild("ratingstate"));
        if (state)
            state->DisplayState(map["numstars"]);
    }
    if (map.contains("tvstate"))
    {
        MythUIStateType *state = dynamic_cast<MythUIStateType *> (win->GetChild("tvstate"));
        if (state)
            state->DisplayState(map["tvstate"]);
    }
    if (map.contains("videocodec"))
    {
        MythUIStateType *state = dynamic_cast<MythUIStateType *> (win->GetChild("videocodec"));
        if (state)
            state->DisplayState(map["videocodec"]);
    }
    if (map.contains("videodescrip"))
    {
        MythUIStateType *state = dynamic_cast<MythUIStateType *> (win->GetChild("videodescrip"));
        if (state)
            state->DisplayState(map["videodescrip"]);
    }
    if (map.contains("audiocodec"))
    {
        MythUIStateType *state = dynamic_cast<MythUIStateType *> (win->GetChild("audiocodec"));
        if (state)
            state->DisplayState(map["audiocodec"]);
    }
    if (map.contains("audiochannels"))
    {
        MythUIStateType *state = dynamic_cast<MythUIStateType *> (win->GetChild("audiochannels"));
        if (state)
            state->DisplayState(map["audiochannels"]);
    }
    if (map.contains("chanid"))
    {
        MythUIImage *icon = dynamic_cast<MythUIImage *> (win->GetChild("iconpath"));
        if (icon)
        {
            icon->Reset();

            uint chanid = map["chanid"].toUInt();
            QString iconpath;
            if (map.contains("iconpath"))
                iconpath = map["iconpath"];
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
    if (map.contains("inetref"))
    {
        MythUIImage *cover = dynamic_cast<MythUIImage *> (win->GetChild("coverart"));
        if (cover && map.contains("coverartpath"))
        {
            QString coverpath = map["coverartpath"];
            cover->SetFilename(coverpath);
            cover->Load(false);
        }
        MythUIImage *fanart = dynamic_cast<MythUIImage *> (win->GetChild("fanart"));
        if (fanart && map.contains("fanartpath"))
        {
            QString fanartpath = map["fanartpath"];
            fanart->SetFilename(fanartpath);
            fanart->Load(false);
        }
        MythUIImage *banner = dynamic_cast<MythUIImage *> (win->GetChild("banner"));
        if (banner && map.contains("bannerpath"))
        {
            QString bannerpath = map["bannerpath"];
            banner->SetFilename(bannerpath);
            banner->Load(false);
        }
        MythUIImage *screenshot = dynamic_cast<MythUIImage *> (win->GetChild("screenshot"));
        if (screenshot && map.contains("screenshotpath"))
        {
            QString screenshotpath = map["screenshotpath"];
            screenshot->SetFilename(screenshotpath);
            screenshot->Load(false);
        }
    }
    if (map.contains("nightmode"))
    {
        MythUIStateType *state = dynamic_cast<MythUIStateType *> (win->GetChild("nightmode"));
        if (state)
            state->DisplayState(map["nightmode"]);
    }
    if (map.contains("mediatype"))
    {
        MythUIStateType *state = dynamic_cast<MythUIStateType *> (win->GetChild("mediatype"));
        if (state)
            state->DisplayState(map["mediatype"]);
    }

    MythUIProgressBar *bar =
        dynamic_cast<MythUIProgressBar *>(win->GetChild("elapsedpercent"));
    if (bar)
    {
        int startts = map["startts"].toInt();
        int endts   = map["endts"].toInt();
        int nowts   = MythDate::current().toTime_t();
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
            int duration = endts - startts;
            if (duration > 0)
                bar->SetUsed(1000 * (nowts - startts) / duration);
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
        ChannelEditor *edit = dynamic_cast<ChannelEditor*>(m_Dialog);
        if (edit)
            edit->SetText(map);
    }
    else
        win->SetTextFromMap(map);

    SetExpiry(window, timeout);
}

void OSD::SetRegions(const QString &window, frm_dir_map_t &map,
                     long long total)
{
    MythScreenType *win = GetWindow(window);
    if (!win)
        return;

    MythUIEditBar *bar = dynamic_cast<MythUIEditBar*>(win->GetChild("editbar"));
    if (!bar)
        return;

    bar->ClearRegions();
    if (!map.size() || total < 1)
    {
        bar->Display();
        return;
    }

    long long start = -1;
    long long end   = -1;
    bool first = true;
    QMapIterator<uint64_t, MarkTypes> it(map);
    while (it.hasNext())
    {
        bool error = false;
        it.next();
        if (it.value() == MARK_CUT_START)
        {
            start = it.key();
            if (end > -1)
                error = true;
        }
        else if (it.value() == MARK_CUT_END)
        {
            if (first)
                start = 0;
            if (start < 0)
                error = true;
            end = it.key();
        }
        else if (it.value() == MARK_PLACEHOLDER)
        {
            start = end = it.key();
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
            bar->AddRegion((float)((double)start/(double)total),
                           (float)((double)end/(double)total));
            start = -1;
            end   = -1;
        }
    }
    if (start > -1 && end < 0)
        bar->AddRegion((float)((double)start/(double)total), 1.0f);

    bar->Display();
}

bool OSD::DrawDirect(MythPainter* painter, QSize size, bool repaint)
{
    if (!painter)
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
            if (m_Effects && m_ExpireTimes.contains((*it)))
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

    MythUINotificationCenter *nc = MythUINotificationCenter::GetInstance();
    QList<MythScreenType*> notifications;
    nc->GetNotificationScreens(notifications);
    QList<MythScreenType*>::iterator it2 = notifications.begin();
    while (it2 != notifications.end())
    {
        if (!MythUINotificationCenter::GetInstance()->ScreenCreated(*it2))
        {
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
            nc->UpdateScreen(*it2);

            visible = true;
            (*it2)->Pulse();
            if (m_Effects)
            {
                QTime expires = nc->ScreenExpiryTime(*it2).time();
                int left = now.msecsTo(expires);
                if (expires.isValid() && left < m_FadeTime)
                    (*it2)->SetAlpha((255 * left) / m_FadeTime);
            }
            if ((*it2)->NeedsRedraw())
                redraw = true;
        }
        ++it2;
    }
    RevertUIScale();

    redraw |= repaint;

    if (redraw && visible)
    {
        QRect cliprect = QRect(QPoint(0, 0), size);
        painter->Begin(NULL);
        for (it = m_Children.begin(); it != m_Children.end(); ++it)
        {
            if ((*it)->IsVisible())
            {
                (*it)->Draw(painter, 0, 0, 255, cliprect);
                (*it)->SetAlpha(255);
                (*it)->ResetNeedsRedraw();
            }
        }
        for (it2 = notifications.begin(); it2 != notifications.end(); ++it2)
        {
            if ((*it2)->IsVisible())
            {
                (*it2)->Draw(painter, 0, 0, 255, cliprect);
                (*it2)->SetAlpha(255);
                (*it2)->ResetNeedsRedraw();
            }
        }
        painter->End();
    }

    return visible;
}

QRegion OSD::Draw(MythPainter* painter, QPaintDevice *device, QSize size,
                  QRegion &changed, int alignx, int aligny)
{
    bool redraw     = m_Refresh;
    QRegion visible = QRegion();
    QRegion dirty   = m_Refresh ? QRegion(QRect(QPoint(0,0), m_Rect.size())) :
                                  QRegion();
    m_Refresh       = false;

    if (!painter || !device)
        return visible;

    QTime now = MythDate::current().time();
    CheckExpiry();

    // first update for alpha pulse and fade
    QMap<QString,MythScreenType*>::const_iterator it;
    for (it = m_Children.begin(); it != m_Children.end(); ++it)
    {
        if ((*it)->IsVisible())
        {
            QRect vis = (*it)->GetArea().toQRect();
            if (visible.isEmpty())
                visible = QRegion(vis);
            else
                visible = visible.united(vis);

            (*it)->Pulse();
            if (m_Effects && m_ExpireTimes.contains((*it)))
            {
                QTime expires = m_ExpireTimes.value((*it)).time();
                int left = now.msecsTo(expires);
                if (left < m_FadeTime)
                    (*it)->SetAlpha((255 * left) / m_FadeTime);
            }
        }

        if ((*it)->NeedsRedraw())
        {
            QRegion area = (*it)->GetDirtyArea();
            dirty = dirty.united(area);
            redraw = true;
        }
    }

    MythUINotificationCenter *nc = MythUINotificationCenter::GetInstance();
    QList<MythScreenType*> notifications;
    nc->GetNotificationScreens(notifications);
    QList<MythScreenType*>::iterator it2 = notifications.begin();
    while (it2 != notifications.end())
    {
        if (!MythUINotificationCenter::GetInstance()->ScreenCreated(*it2))
        {
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
            nc->UpdateScreen(*it2);

            QRect vis = (*it2)->GetArea().toQRect();
            if (visible.isEmpty())
                visible = QRegion(vis);
            else
                visible = visible.united(vis);

            (*it2)->Pulse();
            if (m_Effects)
            {
                QTime expires = nc->ScreenExpiryTime(*it2).time();
                int left = now.msecsTo(expires);
                if (expires.isValid() && left < m_FadeTime)
                    (*it2)->SetAlpha((255 * left) / m_FadeTime);
            }
        }

        if ((*it2)->NeedsRedraw())
        {
            QRegion area = (*it2)->GetDirtyArea();
            dirty = dirty.united(area);
            redraw = true;
        }
        ++it2;
    }
    RevertUIScale();

    if (redraw)
    {
        // clear the dirty area
        painter->Clear(device, dirty);

        // set redraw for any widgets that may now need a partial repaint
        for (it = m_Children.begin(); it != m_Children.end(); ++it)
        {
            if ((*it)->IsVisible() && !(*it)->NeedsRedraw() &&
                dirty.intersects((*it)->GetArea().toQRect()))
            {
                (*it)->SetRedraw();
            }
        }

        for (it2 = notifications.begin(); it2 != notifications.end(); ++it2)
        {
            if ((*it2)->IsVisible() && !(*it2)->NeedsRedraw() &&
                dirty.intersects((*it2)->GetArea().toQRect()))
            {
                (*it2)->SetRedraw();
            }
        }

        // and finally draw
        QRect cliprect = dirty.boundingRect();
        painter->Begin(device);
        painter->SetClipRegion(dirty);
        // TODO painting in reverse may be more efficient...
        for (it = m_Children.begin(); it != m_Children.end(); ++it)
        {
            if ((*it)->NeedsRedraw())
            {
                if ((*it)->IsVisible())
                    (*it)->Draw(painter, 0, 0, 255, cliprect);
                (*it)->SetAlpha(255);
                (*it)->ResetNeedsRedraw();
            }
        }

        for (it2 = notifications.begin(); it2 != notifications.end(); ++it2)
        {
            if ((*it2)->NeedsRedraw())
            {
                if ((*it2)->IsVisible())
                    (*it2)->Draw(painter, 0, 0, 255, cliprect);
                (*it2)->SetAlpha(255);
                (*it2)->ResetNeedsRedraw();
            }
        }

        painter->End();
    }

    changed = dirty;

    if (visible.isEmpty() || (!alignx && !aligny))
        return visible;

    // assist yuv blending with some friendly alignments
    QRegion aligned;
    QVector<QRect> rects = visible.rects();
    for (int i = 0; i < rects.size(); i++)
    {
        QRect r   = rects[i];
        int left  = r.left() & ~(alignx - 1);
        int top   = r.top()  & ~(aligny - 1);
        int right = (r.left() + r.width());
        int bot   = (r.top() + r.height());
        if (right & (alignx - 1))
            right += alignx - (right & (alignx - 1));
        if (bot % aligny)
            bot += aligny - (bot % aligny);
        aligned = aligned.united(QRegion(left, top, right - left, bot - top));
    }

    return aligned.intersected(QRect(QPoint(0,0), size));
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
                MythDialogBox *dialog = dynamic_cast<MythDialogBox*>(m_Dialog);
                if (dialog)
                {
                    QString replace = QObject::tr("%n second(s)", NULL,
                                                  now.secsTo(it.value()));
                    dialog->SetText(newtext.replace("%d", replace));
                }
                MythConfirmationDialog *cdialog = dynamic_cast<MythConfirmationDialog*>(m_Dialog);
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

void OSD::SetExpiry(const QString &window, enum OSDTimeout timeout,
                    int custom_timeout)
{
    if (timeout == kOSDTimeout_Ignore && !custom_timeout)
        return;

    MythScreenType *win = GetWindow(window);
    int time = custom_timeout ? custom_timeout : m_Timeouts[timeout];
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

void OSD::SetTimeouts(int _short, int _medium, int _long)
{
    m_Timeouts[kOSDTimeout_None]  = -1;
    m_Timeouts[kOSDTimeout_Short] = _short;
    m_Timeouts[kOSDTimeout_Med]   = _medium;
    m_Timeouts[kOSDTimeout_Long]  = _long;
}

bool OSD::IsWindowVisible(const QString &window)
{
    if (!m_Children.contains(window))
        return false;

    return m_Children.value(window)->IsVisible(/*true*/);
}

void OSD::ResetWindow(const QString &window)
{
    if (!m_Children.contains(window))
        return;

    m_Children.value(window)->Reset();
}

void OSD::PositionWindow(MythScreenType *window)
{
    if (!window)
        return;

    MythRect rect = window->GetArea();
    rect.translate(m_Rect.left(), m_Rect.top());
    window->SetArea(rect);
}

void OSD::RemoveWindow(const QString &window)
{
    if (!m_Children.contains(window))
        return;

    HideWindow(window);
    MythScreenType *child = m_Children.value(window);
    m_Children.remove(window);
    delete child;
}

MythScreenType *OSD::GetWindow(const QString &window)
{
    if (m_Children.contains(window))
        return m_Children.value(window);

    MythScreenType *new_window = NULL;

    if (window == OSD_WIN_INTERACT)
    {
        new_window = new InteractiveScreen(m_parent, window);
    }
    else if (window == OSD_WIN_BDOVERLAY)
    {
        new_window = new BDOverlayScreen(m_parent, window);
    }
    else
    {
        new_window = new MythOSDWindow(NULL, window, false);
    }

    new_window->SetPainter(m_CurrentPainter);
    if (new_window->Create())
    {
        m_Children.insert(window, new_window);
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("Created window %1").arg(window));
        return new_window;
    }

    LOG(VB_GENERAL, LOG_ERR, LOC + QString("Failed to create window %1")
            .arg(window));
    delete new_window;
    return NULL;
}

void OSD::SetFunctionalWindow(const QString window, enum OSDFunctionalType type)
{
    if (m_FunctionalType != kOSDFunctionalType_Default &&
        m_FunctionalType != type)
        SendHideEvent();

    m_FunctionalWindow = window;
    m_FunctionalType   = type;
}

void OSD::HideWindow(const QString &window)
{
    if (!m_Children.contains(window))
        return;
    m_Children.value(window)->SetVisible(false);
    m_Children.value(window)->Close(); // for InteractiveScreen
    SetExpiry(window, kOSDTimeout_None);
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
    OSDHideEvent *event = new OSDHideEvent(m_FunctionalType);
    QCoreApplication::postEvent(m_ParentObject, event);
}

bool OSD::HasWindow(const QString &window)
{
    return m_Children.contains(window);
}

bool OSD::DialogVisible(QString window)
{
    if (!m_Dialog || window.isEmpty())
        return m_Dialog;

    return m_Dialog->objectName() == window;
}

bool OSD::DialogHandleKeypress(QKeyEvent *e)
{
    if (!m_Dialog)
        return false;
    return m_Dialog->keyPressEvent(e);
}

void OSD::DialogQuit(void)
{
    if (!m_Dialog)
        return;

    RemoveWindow(m_Dialog->objectName());
    m_Dialog = NULL;
    m_PulsedDialogText = QString();
}

void OSD::DialogShow(const QString &window, const QString &text, int updatefor)
{
    if (m_Dialog)
    {
        QString current = m_Dialog->objectName();
        if (current != window)
        {
            DialogQuit();
        }
        else
        {
            MythDialogBox *dialog = dynamic_cast<MythDialogBox*>(m_Dialog);
            if (dialog)
                dialog->Reset();

            DialogSetText(text);
        }
    }

    if (!m_Dialog)
    {
        OverrideUIScale();
        MythScreenType *dialog;

        if (window == OSD_DLG_EDITOR)
            dialog = new ChannelEditor(m_ParentObject, window.toLatin1());
        else if (window == OSD_DLG_CONFIRM)
            dialog = new MythConfirmationDialog(NULL, text, false);
        else
            dialog = new MythDialogBox(text, NULL, window.toLatin1(), false, true);

        dialog->SetPainter(m_CurrentPainter);
        if (dialog->Create())
        {
            PositionWindow(dialog);
            m_Dialog = dialog;
            MythDialogBox *dbox = dynamic_cast<MythDialogBox*>(m_Dialog);
            if (dbox)
                dbox->SetReturnEvent(m_ParentObject, window);
            MythConfirmationDialog *cbox = dynamic_cast<MythConfirmationDialog*>(m_Dialog);
            if (cbox)
            {
                cbox->SetReturnEvent(m_ParentObject, window);
                cbox->SetData("DIALOG_CONFIRM_X_X");
            }
            m_Children.insert(window, m_Dialog);
        }
        else
        {
            RevertUIScale();
            delete dialog;
            return;
        }

        RevertUIScale();
    }

    if (updatefor)
    {
        m_NextPulseUpdate  = MythDate::current();
        m_PulsedDialogText = text;
        SetExpiry(window, kOSDTimeout_None, updatefor);
    }

    DialogBack();
    HideAll(true, m_Dialog);
    m_Dialog->SetVisible(true);
}

void OSD::DialogSetText(const QString &text)
{
    MythDialogBox *dialog = dynamic_cast<MythDialogBox*>(m_Dialog);
    if (dialog)
        dialog->SetText(text);
}

void OSD::DialogBack(QString text, QVariant data, bool exit)
{
    MythDialogBox *dialog = dynamic_cast<MythDialogBox*>(m_Dialog);
    if (dialog)
    {
        dialog->SetBackAction(text, data);
        if (exit)
            dialog->SetExitAction(text, data);
    }
}

void OSD::DialogAddButton(QString text, QVariant data, bool menu, bool current)
{
    MythDialogBox *dialog = dynamic_cast<MythDialogBox*>(m_Dialog);
    if (dialog)
        dialog->AddButton(text, data, menu, current);
}

void OSD::DialogGetText(QHash<QString,QString> &map)
{
    ChannelEditor *edit = dynamic_cast<ChannelEditor*>(m_Dialog);
    if (edit)
        edit->GetText(map);
}

TeletextScreen* OSD::InitTeletext(void)
{
    TeletextScreen *tt = NULL;
    if (m_Children.contains(OSD_WIN_TELETEXT))
    {
        tt = (TeletextScreen*)m_Children.value(OSD_WIN_TELETEXT);
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
            tt = NULL;
        }
        RevertUIScale();
    }
    if (!tt)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create Teletext window");
        return NULL;
    }

    HideWindow(OSD_WIN_TELETEXT);
    tt->SetDisplaying(false);
    return tt;
}

void OSD::EnableTeletext(bool enable, int page)
{
    TeletextScreen *tt = InitTeletext();
    if (!tt)
        return;

    tt->SetVisible(enable);
    tt->SetDisplaying(enable);
    if (enable)
    {
        tt->SetPage(page, -1);
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Enabled teletext page %1")
                                   .arg(page));
    }
    else
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Disabled teletext");
}

bool OSD::TeletextAction(const QString &action)
{
    if (!HasWindow(OSD_WIN_TELETEXT))
        return false;

    TeletextScreen* tt = (TeletextScreen*)m_Children.value(OSD_WIN_TELETEXT);
    if (tt)
        return tt->KeyPress(action);
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

    TeletextScreen* tt = (TeletextScreen*)m_Children.value(OSD_WIN_TELETEXT);
    if (tt)
        tt->ClearScreen();
}

SubtitleScreen* OSD::InitSubtitles(void)
{
    SubtitleScreen *sub = NULL;
    if (m_Children.contains(OSD_WIN_SUBTITLE))
    {
        sub = (SubtitleScreen*)m_Children.value(OSD_WIN_SUBTITLE);
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
            sub = NULL;
        }
        RevertUIScale();
    }
    if (!sub)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create subtitle window");
        return NULL;
    }
    return sub;
}

void OSD::EnableSubtitles(int type, bool forced_only)
{
    SubtitleScreen *sub = InitSubtitles();
    if (sub)
        sub->EnableSubtitles(type, forced_only);
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

void OSD::DisplayDVDButton(AVSubtitle* dvdButton, QRect &pos)
{
    if (!dvdButton)
        return;

    SubtitleScreen* sub = InitSubtitles();
    if (sub)
    {
        EnableSubtitles(kDisplayDVDButton);
        sub->DisplayDVDButton(dvdButton, pos);
    }
}

void OSD::DisplayBDOverlay(BDOverlay* overlay)
{
    if (!overlay)
        return;

    BDOverlayScreen* bd = (BDOverlayScreen*)GetWindow(OSD_WIN_BDOVERLAY);
    if (bd)
        bd->DisplayBDOverlay(overlay);
}
